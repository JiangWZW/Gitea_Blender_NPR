/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * The shadow module manages shadow update tagging & shadow rendering.
 */

#pragma once

#include "BLI_vector.hh"

#include "GPU_batch.h"

#include "eevee_allocator.hh"
#include "eevee_id_map.hh"
#include "eevee_material.hh"
#include "eevee_shader.hh"
#include "eevee_shader_shared.hh"

namespace blender::eevee {

/**
 * TODO(fclem): Future plans
 * The start of the implementation was done on CPU with the constraints of UBO limits and no
 * compute capabilities in mind.
 * But after removing this limit this left the door open for a full GPU driven pipeline of
 * shadow and light management where the CPU would only push Objects updates and manage buffer
 * grow/shrink behaviors. The GPU would then do what ShadowTileAllocator, ShadowPunctual and
 * ShadowDirectional classes are doing.
 * We still need to find a way to issue the shadow render passes at once and cull objects per view
 * on GPU.
 */

class Instance;
class ShadowModule;

/** World space axis aligned bounding box. */
struct AABB {
  float3 min, max;

  AABB() = default;
  AABB(float val) : min(-val), max(val){};
  AABB(Object *ob)
  {
    init_min_max();
    BoundBox *bb = BKE_object_boundbox_get(ob);
    for (int i = 0; i < 8; i++) {
      float vec[3];
      copy_v3_v3(vec, bb->vec[i]);
      mul_m4_v3(ob->obmat, vec);
      minmax_v3v3_v3(min, max, vec);
    }
  }

  void debug_draw(void)
  {
    BoundBox bb = *this;
    vec4 color = {1, 0, 0, 1};
    DRW_debug_bbox(&bb, color);
  }

  float3 center(void) const
  {
    return (min + max) * 0.5;
  }

  void init_min_max(void)
  {
    INIT_MINMAX(min, max);
  }

  void merge(const AABB &a)
  {
    DO_MIN(a.min, min);
    DO_MAX(a.max, max);
  }

  void merge(const float3 &a)
  {
    DO_MIN(a, min);
    DO_MAX(a, max);
  }

  float radius(void) const
  {
    return (max - min).length() / 2.0f;
  }

  operator BoundBox() const
  {
    float3 middle = center();
    float3 halfdim = max - middle;
    BoundBox bb;
    *reinterpret_cast<float3 *>(bb.vec[0]) = middle + halfdim * vec3(1, 1, 1);
    *reinterpret_cast<float3 *>(bb.vec[1]) = middle + halfdim * vec3(-1, 1, 1);
    *reinterpret_cast<float3 *>(bb.vec[2]) = middle + halfdim * vec3(-1, -1, 1);
    *reinterpret_cast<float3 *>(bb.vec[3]) = middle + halfdim * vec3(1, -1, 1);
    *reinterpret_cast<float3 *>(bb.vec[4]) = middle + halfdim * vec3(1, 1, -1);
    *reinterpret_cast<float3 *>(bb.vec[5]) = middle + halfdim * vec3(-1, 1, -1);
    *reinterpret_cast<float3 *>(bb.vec[6]) = middle + halfdim * vec3(-1, -1, -1);
    *reinterpret_cast<float3 *>(bb.vec[7]) = middle + halfdim * vec3(1, -1, -1);
    return bb;
  }
};

/* -------------------------------------------------------------------- */
/** \name Shadow
 *
 * \{ */

/* To be applied after viewmatrix. */
constexpr static const float shadow_face_mat[6][4][4] = {
    {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}},   /* Z_NEG */
    {{0, 0, -1, 0}, {-1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}}, /* X_POS */
    {{0, 0, 1, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}},   /* X_NEG */
    {{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}},  /* Y_POS */
    {{-1, 0, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}},  /* Y_NEG */
    {{1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, -1, 0}, {0, 0, 0, 1}}, /* Z_POS */
};

enum eCubeFace {
  /* Ordering by culling order. If cone aperture is shallow, we cull the later view. */
  Z_NEG = 0,
  X_POS,
  X_NEG,
  Y_POS,
  Y_NEG,
  Z_POS,
};

/**
 * Stores indirection table and states of each tile of a virtual shadowmap clipmap level.
 * One tilemap has the effective resolution of `pagesize * tile_map_resolution` .
 * Each tilemap overhead is quite small if they do not have any pages allocated.
 */
struct ShadowTileMap : public ShadowTileMapData {
  static constexpr int64_t tile_map_resolution = SHADOW_TILEMAP_RES;
  static constexpr int64_t tiles_count = tile_map_resolution * tile_map_resolution;

  /** Level of detail for clipmap. */
  int level = INT_MAX;
  /** Integer offset of the center of the 16x16 tiles from the origin of the tile space. */
  int2 grid_offset = int2(16);
  /** Cube face index. */
  eCubeFace cubeface = Z_NEG;
  /** Copy of the light object matrix. */
  float4x4 object_mat;
  /** Near and far clip distances. For clipmap they are updated after sync. */
  float near, far;

 public:
  ShadowTileMap(int64_t _index)
  {
    index = _index;
  };

  void sync_clipmap(const Camera &camera,
                    const float4x4 &object_mat_,
                    const AABB &casters_bounds,
                    int clipmap_level);
  void sync_cubeface(
      const float4x4 &object_mat, const float4x4 &winmat, float near, float far, eCubeFace face);

  float tilemap_coverage_get(void) const
  {
    BLI_assert(!is_cubeface);
    return powf(2.0f, level);
  }

  void debug_draw(void) const;

  void set_dirty()
  {
    grid_shift = int2(16);
  }

  void set_updated()
  {
    grid_shift = int2(0);
  }
};

struct ShadowCommon {
  /** Tilemap for each cubeface needed (in eCubeFace order) or for each clipmap level. */
  Vector<ShadowTileMap *> tilemaps;
  /** To have access to the tilemap allocator. */
  ShadowModule *shadows_;

  ShadowCommon(ShadowModule *shadows) : shadows_(shadows){};

  void free_resources();
};

class ShadowPunctual : public ShadowCommon {
 private:
  /** Cone angle to cover. Used to reject tiles not present in the lit part. */
  float cone_aperture_;
  /** Area light size. */
  float size_x_, size_y_;
  /** Shape type. */
  eLightType light_type_;
  /** Random position on the light. In world space. */
  vec3 random_offset_;
  /** Light position. */
  float3 position_;
  /** Near and far clip distances. For clipmap they are updated after sync. */
  float far_, near_;
  /** View space offset to apply to the shadow. */
  float bias_;

 public:
  ShadowPunctual(ShadowModule *shadows) : ShadowCommon(shadows){};

  void sync(eLightType light_type,
            const mat4 &object_mat,
            float cone_aperture,
            float near_clip,
            float far_clip,
            float bias);

  operator ShadowData();
};

class ShadowDirectional : public ShadowCommon {
 private:
  /** User minimum resolution. */
  float min_resolution_;
  /** View space offset to apply to the shadow. */
  float bias_;
  /** Near and far clip distances. For clipmap they are updated after sync. */
  float near_, far_;
  /** Copy of object matrix. */
  float4x4 object_mat_;

 public:
  ShadowDirectional(ShadowModule *shadows) : ShadowCommon(shadows){};

  void sync(const mat4 &object_mat, float bias, float min_resolution);
  void end_sync(const Camera &camera, const AABB &casters_bounds);

  operator ShadowData();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Casters & Receivers
 *
 * \{ */

/* Can be either a shadow caster or a shadow receiver. */
struct ShadowObject {
  AABB aabb;

  bool initialized = false;
  bool used;
  bool updated;

  void sync(Object *ob)
  {
    aabb = AABB(ob);
    initialized = true;
    updated = true;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShadowModule
 *
 * Manages shadow atlas and shadow region datas.
 * \{ */

/**
 * Manages the tilemaps and allocates continuous regions to a shadow object.
 * This way indexing is simple and fast inside the shaders.
 * The tilemap atlas has a fixed 64x64 size. So it can contain 4096 tilemap or 16x16 pixels each.
 * We allocate for many tilemaps because we don't want to reallocate the buffer as it would mean
 * trashing the whole cache which it.
 * In the future we could resize and copy old tilemap infos. But for now we KISS.
 */
struct ShadowTileAllocator {
  static constexpr int64_t size = MAX_SHADOW_TILEMAP;
  /** Limit the with of the texture. */
  static constexpr int64_t maps_per_row = SHADOW_TILEMAP_PER_ROW;
  /* TODO(fclem): Do it for real... Use real bitmap. */
  Vector<bool> usage_bitmap_ = Vector<bool>(size);
  /** Circular buffer allocation scheme. This is the last allocated index. */
  int64_t next_index = 0;
  /** Vector containning the actual maps. Unordered. */
  Vector<ShadowTileMap *> maps;
  /** Deleted maps go here to be freed after the next sync. */
  Vector<ShadowTileMap *> maps_deleted;
  /**
   * Tilemap atlas containing mapping to shadow pages inside the atlas texture.
   * All shadow tilemaps are packed into one texture.
   * Contains every clipmaps level of all directional light and each cubeface with mipmap.
   */
  Texture tilemap_tx = Texture("tilemap_tx");
  /** Very small texture containing the result of the update pass. */
  /** FIXME(fclem): It would be nice to avoid GPU > CPU readback. */
  Texture tilemap_update_result_tx = Texture("tilemap_update_result_tx");
  /** UBO containing the description for every allocated tilemap. */
  ShadowTileMapDataBuf tilemaps_data;
  /** Number of maps inside the tilemaps_data. */
  int64_t updated_maps_count = 0;
  /** Number of maps at the end of tilemaps_data that are being deleted and need clear. */
  int64_t deleted_maps_count = 0;

  ShadowTileAllocator();
  ~ShadowTileAllocator();

  /** Returns empty span on failure. */
  Span<ShadowTileMap *> alloc(int64_t count);

  void free(Vector<ShadowTileMap *> &free_list);

  void end_sync();
};

class ShadowModule {
  friend ShadowPunctual;
  friend ShadowDirectional;

  template<typename T> class ShadowAllocator : public IndexedAllocator<T> {
   private:
    ShadowModule &shadows_;

   public:
    ShadowAllocator(ShadowModule &shadows) : shadows_(shadows){};

    int64_t alloc(void)
    {
      return IndexedAllocator<T>::alloc(T(&shadows_));
    }
  };

 public:
  /** Need to be first because of destructor order. */
  ShadowTileAllocator tilemap_allocator;

  ShadowAllocator<ShadowPunctual> punctuals;
  ShadowAllocator<ShadowDirectional> directionals;

 private:
  Instance &inst_;

  /** Map of shadow casters to track deletion & update of intersected shadows. */
  Map<ObjectKey, ShadowObject> objects_;

  /** Used to detect sample change for soft shadows. */
  uint64_t last_sample_ = 0;

  /**
   * TODO(fclem) These should be stored inside the Shadow objects instead.
   * The issues is that only 32 DRWView can have effective culling data with the current
   * implementation. So we try to reduce the number of DRWView allocated to avoid the slow path.
   **/
  DRWView *views_[6] = {nullptr};

  /**
   * Contains pages of shadow_page_size_. Tilemaps reference these pages.
   * Managed using compute shaders.
   */
  eevee::Texture atlas_tx_ = Texture("atlas_tx");
  /**
   * Atlas map used from bookeeping of rendered page on the GPU.
   * Contains mapping from pages to pixel inside the tilemap.
   */
  eevee::Texture atlas_info_tx_ = Texture("atlas_info_tx");
  /**
   * Separate render buffer. This is meant to be replace by directly rendering inside the atlas.
   */
  eevee::Texture render_tx_ = Texture("shadow_target_tx_");
  eevee::Framebuffer render_fb_ = Framebuffer("shadow_fb");
  /**
   * For rendering occluders & non-opaque. Small 16x16 framebuffer.
   */
  eevee::Texture tagging_tx_ = Texture("tagging_tx");
  eevee::Framebuffer tagging_fb_ = Framebuffer("tagging_fb");

  /**
   * Clear the visibility, usage and request bits.
   * Also shifts the whole tilemap for directional shadow clipmaps.
   */
  DRWPass *tilemap_setup_ps_;
  bool tilemap_setup_has_run_ = false;
  /** Update passes that will mark all shadow pages from a light to update or as unused. */
  DRWPass *tilemap_visibility_ps_;
  /**
   * Update passes that will mark all shadow pages touching an updated shadow caster.
   * This uses point sprite rendering to render only compute intersection and update the
   * intersecting tiles. This reduces atomic contention.
   */
  DRWPass *tilemap_update_tag_ps_;
  DRWCallBuffer *casters_updated_;
  /**
   * Draw one point for each receiver that may not appear in the depth buffer.
   * This is to support forward shaded geometry.
   */
  /* NOTE(fclem): Until we implement depth buffer scanning, we rely solely on this to tag
   * needed tiles. */
  DRWPass *tilemap_usage_tag_ps_;
  DRWCallBuffer *receivers_non_opaque_;
  int updating_map_index_;

  /** Display informations about the virtual shadows. */
  DRWPass *debug_draw_ps_;
  /** Depth input for debug drawing. Reference only. */
  GPUTexture *input_depth_tx_;
  ObjectKey debug_light_key;

  /** Scene immutable parameter. */
  int shadow_page_size_ = 256;
  bool soft_shadows_enabled_ = false;
  /** Default to invalid texture type. */
  eGPUTextureFormat shadow_format_ = GPU_RGBA8;

  GPUTexture *atlas_tx_ptr_;

  /** Used for caster & receiver AABB lists. */
  GPUVertFormat aabb_format_;
  /** Global bounds that contains all shadow casters. Used by directionnal for best fit. */
  AABB casters_bounds_;

  ShadowDebugDataBuf debug_data_;

 public:
  ShadowModule(Instance &inst) : punctuals(*this), directionals(*this), inst_(inst)
  {
    GPU_vertformat_clear(&aabb_format_);
    /* Must match the C++ AABB layout. */
    BLI_assert(sizeof(AABB) == sizeof(float) * 6);
    GPU_vertformat_attr_add(&aabb_format_, "aabb_min", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&aabb_format_, "aabb_max", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  ~ShadowModule(){};

  void init(void);

  void begin_sync(void);
  void sync_object(Object *ob,
                   const ObjectHandle &handle,
                   bool is_shadow_caster,
                   bool is_alpha_blend);
  void end_sync(void);

  void update_visible(const DRWView *view);

  void debug_end_sync(void);
  void debug_draw(GPUFrameBuffer *view_fb, GPUTexture *depth_tx);

  GPUTexture **atlas_ref_get(void)
  {
    return &atlas_tx_ptr_;
  }

 private:
  void remove_unused(void);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShadowPass
 *
 * A simple depth pass to which all shadow casters subscribe.
 * \{ */

class ShadowPass {
 private:
  Instance &inst_;

  DRWPass *clear_ps_ = nullptr;
  DRWPass *surface_ps_ = nullptr;

 public:
  ShadowPass(Instance &inst) : inst_(inst){};

  void sync(void);

  DRWShadingGroup *material_add(::Material *blender_mat, GPUMaterial *gpumat);

  void render(void);
};

/** \} */

}  // namespace blender::eevee
