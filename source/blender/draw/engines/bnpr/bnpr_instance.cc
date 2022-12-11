/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup bnpr
 *
 * An instance contains all structures needed to do a complete render.
 */

#include <sstream>

#include "BKE_global.h"
#include "BKE_object.h"
#include "BLI_rect.h"
#include "DEG_depsgraph_query.h"
#include "DNA_ID.h"
#include "DNA_lightprobe_types.h"
#include "DNA_modifier_types.h"
#include "RE_pipeline.h"

#include "bnpr_instance.hh"

namespace blender::bnpr
{
  /* -------------------------------------------------------------------- */
  /** \name Initialization
   *
   * Initialization functions need to be called once at the start of a frame.
   * Active camera, render extent and enabled render passes are immutable until next init.
   * This takes care of resizing output buffers and view in case a parameter changed.
   * IMPORTANT: xxx.init() functions are NOT meant to acquire and allocate DRW resources.
   * Any attempt to do so will likely produce use after free situations.
   * \{ */
  void Instance::init(
    Depsgraph* depsgraph_,
    Manager* manager_,
    const View3D* v3d_,
    const RegionView3D* rv3d_,
    const DRWView* drw_view_,
    Object* camera_object_)
  {
    /* Init things static per render frame. (Not render graph related) */
    depsgraph = depsgraph_;
    manager = manager_;

    drw_view = drw_view_;
    v3d = v3d_;
    rv3d = rv3d_;

    camera_orig_object = camera_object_;

    info = "";
  }

  void Instance::update_eval_members()
  {
    scene = DEG_get_evaluated_scene(depsgraph);
    camera_eval_object = (camera_orig_object) ?
                         DEG_get_evaluated_object(depsgraph, camera_orig_object) :
                         nullptr;
  }

  /** \} */






  /* -------------------------------------------------------------------- */
  /** \name Sync
   *
   * Sync will gather data from the scene that can change over a time step (i.e: motion steps).
   * IMPORTANT: xxx.sync() functions area responsible for creating DRW resources (i.e: DRWView) as
   * well as querying temp texture pool. All DRWPasses should be ready by the end end_sync().
   * \{ */
  void Instance::begin_sync(Manager& manager)
  {
    /* Init draw passes and manager related stuff. (Begin render graph) */
  }

  void Instance::object_sync(Manager& manager, ObjectRef& object_ref)
  { /* Add object draw calls to passes. (Populate render graph) */
    Object *ob = object_ref.object;

    const bool is_renderable_type = ELEM(ob->type, OB_CURVES, OB_GPENCIL, OB_MESH, OB_LAMP);
    const int ob_visibility = DRW_object_visibility_in_active_context(ob);
    const bool partsys_is_visible = (ob_visibility & OB_VISIBLE_PARTICLES) != 0 &&
                                    (ob->type == OB_MESH);
    const bool object_is_visible = DRW_object_is_renderable(ob) &&
                                   (ob_visibility & OB_VISIBLE_SELF) != 0;

    if (!is_renderable_type || (!partsys_is_visible && !object_is_visible)) {
      return;
    }

    /* fclem: TODO cleanup. */
    ObjectRef ob_ref = DRW_object_ref_get(ob);
    ResourceHandle res_handle = manager.resource_handle(ob_ref);

    ObjectHandle &ob_handle = sync.sync_object(ob);


    if (object_is_visible) {
      switch (ob->type) {
      case OB_MESH:
        sync.sync_mesh(ob, ob_handle, res_handle, ob_ref);
        break;
      default:
        break;
      }
    }

    ob_handle.reset_recalc_flag();
  }

  void Instance::end_sync(Manager&)
  {
    /* Post processing after all object. (End render graph) */
  }

  /** \} */






  /* -------------------------------------------------------------------- */
  /** \name Rendering
   * \{ */

  void Instance::draw_viewport(Manager& manager, View& view, GPUTexture* depth_tx,
    GPUTexture* color_tx)
  {
    /* Submit passes here. (Execute render graph) */
  }

  /** \} */





}
