/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_multi_value_map.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.hh"
#include "BKE_idprop.h"
#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "ED_asset.h"

#include "node_intern.hh"

namespace blender::ed::space_node {

static bool node_add_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  return CTX_wm_space_node(C);
}

struct LibraryAsset {
  AssetLibraryReference library_ref;
  AssetHandle handle;
};

struct LibraryCatalog {
  bke::AssetLibrary *library;
  const bke::AssetCatalog *catalog;
};

struct AssetItemTree {
  bke::AssetCatalogTree catalogs;
  MultiValueMap<bke::AssetCatalogPath, LibraryAsset> assets_per_path;
  Map<const bke::AssetCatalogTreeItem *, bke::AssetCatalogPath> full_catalog_per_tree_item;
};

static AssetItemTree build_catalog_tree(const bContext &C, const bNodeTree &node_tree)
{
  const Main &bmain = *CTX_data_main(&C);
  const Vector<AssetLibraryReference> all_libraries = bke::all_asset_library_refs();

  /* Merge catalogs from all libraries to deduplicate menu items. Also store the catalog and
   * library for each asset ID in order to use them later when retrieving assets and removing
   * empty catalogs.  */
  Map<bke::CatalogID, LibraryCatalog> id_to_catalog_map;
  bke::AssetCatalogTree catalogs_from_all_libraries;
  for (const AssetLibraryReference &ref : all_libraries) {
    if (bke::AssetLibrary *library = BKE_asset_library_load(&bmain, ref)) {
      if (bke::AssetCatalogTree *tree = library->catalog_service->get_catalog_tree()) {
        tree->foreach_item([&](bke::AssetCatalogTreeItem &item) {
          const bke::CatalogID &id = item.get_catalog_id();
          bke::AssetCatalog *catalog = library->catalog_service->find_catalog(id);
          catalogs_from_all_libraries.insert_item(*catalog);
          id_to_catalog_map.add(item.get_catalog_id(), LibraryCatalog{library, catalog});
        });
      }
    }
  }

  /* Find all the matching node group assets for every catalog path. */
  MultiValueMap<bke::AssetCatalogPath, LibraryAsset> assets_per_path;
  for (const AssetLibraryReference &library_ref : all_libraries) {
    AssetFilterSettings type_filter{};
    type_filter.id_types = FILTER_ID_NT;

    ED_assetlist_storage_fetch(&library_ref, &C);
    ED_assetlist_ensure_previews_job(&library_ref, &C);
    ED_assetlist_iterate(library_ref, [&](AssetHandle asset) {
      if (!ED_asset_filter_matches_asset(&type_filter, &asset)) {
        return true;
      }
      const AssetMetaData &meta_data = *ED_asset_handle_get_metadata(&asset);
      const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
      if (tree_type == nullptr || IDP_Int(tree_type) != node_tree.type) {
        return true;
      }
      if (BLI_uuid_is_nil(meta_data.catalog_id)) {
        return true;
      }
      const LibraryCatalog &library_catalog = id_to_catalog_map.lookup(meta_data.catalog_id);
      assets_per_path.add(library_catalog.catalog->path, LibraryAsset{library_ref, asset});
      return true;
    });
  }

  /* Build the final tree without any of the catalogs that don't have proper node group assets. */
  bke::AssetCatalogTree catalogs_with_node_assets;
  catalogs_from_all_libraries.foreach_item([&](bke::AssetCatalogTreeItem &item) {
    if (!assets_per_path.lookup(item.catalog_path()).is_empty()) {
      const bke::CatalogID &id = item.get_catalog_id();
      const LibraryCatalog &library_catalog = id_to_catalog_map.lookup(id);
      bke::AssetCatalog *catalog = library_catalog.library->catalog_service->find_catalog(id);
      catalogs_with_node_assets.insert_item(*catalog);
    }
  });

  /* Build another map storing full asset paths for each tree item, in order to have stable
   * pointers to asset catalog paths to use for context pointers. This is necessary because
   * #bke::AssetCatalogTreeItem doesn't store its full path directly. */
  Map<const bke::AssetCatalogTreeItem *, bke::AssetCatalogPath> full_catalog_per_tree_item;
  catalogs_with_node_assets.foreach_item([&](bke::AssetCatalogTreeItem &item) {
    full_catalog_per_tree_item.add_new(&item, item.catalog_path());
  });

  return {std::move(catalogs_with_node_assets),
          std::move(assets_per_path),
          std::move(full_catalog_per_tree_item)};
}

static void node_add_catalog_assets_draw(const bContext *C, Menu *menu)
{
  bScreen &screen = *CTX_wm_screen(C);
  const SpaceNode &snode = *CTX_wm_space_node(C);
  if (!snode.runtime->assets_for_menu) {
    BLI_assert_unreachable();
    return;
  }
  AssetItemTree &tree = *snode.runtime->assets_for_menu;
  const bNodeTree *edit_tree = snode.edittree;
  if (!edit_tree) {
    return;
  }

  const PointerRNA menu_path_ptr = CTX_data_pointer_get(C, "asset_catalog_path");
  if (RNA_pointer_is_null(&menu_path_ptr)) {
    return;
  }
  const bke::AssetCatalogPath &menu_path = *static_cast<const bke::AssetCatalogPath *>(
      menu_path_ptr.data);

  const Span<LibraryAsset> asset_items = tree.assets_per_path.lookup(menu_path);
  bke::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(menu_path);
  BLI_assert(catalog_item != nullptr);

  if (asset_items.is_empty() && !catalog_item->has_children()) {
    return;
  }

  uiLayout *layout = menu->layout;
  uiItemS(layout);

  for (const LibraryAsset &item : asset_items) {
    uiLayout *row = uiLayoutRow(layout, false);
    PointerRNA file{
        &screen.id, &RNA_FileSelectEntry, const_cast<FileDirEntry *>(item.handle.file_data)};
    uiLayoutSetContextPointer(row, "active_file", &file);

    PointerRNA library_ptr{&screen.id,
                           &RNA_AssetLibraryReference,
                           const_cast<AssetLibraryReference *>(&item.library_ref)};
    uiLayoutSetContextPointer(row, "asset_library_ref", &library_ptr);

    uiItemO(row, ED_asset_handle_get_name(&item.handle), ICON_NONE, "NODE_OT_add_group_asset");
  }

  catalog_item->foreach_child([&](bke::AssetCatalogTreeItem &child_item) {
    const bke::AssetCatalogPath &path = tree.full_catalog_per_tree_item.lookup(&child_item);
    PointerRNA path_ptr{
        &screen.id, &RNA_AssetCatalogPath, const_cast<bke::AssetCatalogPath *>(&path)};
    uiLayout *row = uiLayoutRow(layout, false);
    uiLayoutSetContextPointer(row, "asset_catalog_path", &path_ptr);
    uiItemM(row, "NODE_MT_node_add_catalog_assets", path.name().c_str(), ICON_NONE);
  });
}

static void add_root_catalogs_draw(const bContext *C, Menu *menu)
{
  bScreen &screen = *CTX_wm_screen(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  const bNodeTree *edit_tree = snode.edittree;

  snode.runtime->assets_for_menu.reset();
  snode.runtime->assets_for_menu = std::make_shared<AssetItemTree>(
      build_catalog_tree(*C, *edit_tree));
  AssetItemTree &tree = *snode.runtime->assets_for_menu;
  if (tree.catalogs.is_empty()) {
    return;
  }

  uiLayout *layout = menu->layout;
  uiItemS(layout);

  tree.catalogs.foreach_root_item([&](bke::AssetCatalogTreeItem &item) {
    const bke::AssetCatalogPath &path = tree.full_catalog_per_tree_item.lookup(&item);
    PointerRNA path_ptr{
        &screen.id, &RNA_AssetCatalogPath, const_cast<bke::AssetCatalogPath *>(&path)};
    uiLayout *row = uiLayoutRow(layout, false);
    uiLayoutSetContextPointer(row, "asset_catalog_path", &path_ptr);
    uiItemM(row, "NODE_MT_node_add_catalog_assets", path.name().c_str(), ICON_NONE);
  });
}

MenuType add_catalog_assets_menu_type()
{
  MenuType type{};
  BLI_strncpy(type.idname, "NODE_MT_node_add_catalog_assets", sizeof(type.idname));
  type.poll = node_add_menu_poll;
  type.draw = node_add_catalog_assets_draw;
  return type;
}

MenuType add_root_catalogs_menu_type()
{
  MenuType type{};
  BLI_strncpy(type.idname, "NODE_MT_node_add_root_catalogs", sizeof(type.idname));
  type.poll = node_add_menu_poll;
  type.draw = add_root_catalogs_draw;
  return type;
}

}  // namespace blender::ed::space_node