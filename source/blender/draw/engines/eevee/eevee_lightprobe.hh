/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "eevee_lightcache.hh"
#include "eevee_view.hh"

#include "eevee_wrapper.hh"

namespace blender::eevee {

class Instance;

class LightProbeModule {
 private:
  Instance &inst_;

  LightProbeFilterDataBuf filter_data_;
  LightProbeInfoDataBuf info_data_;
  GridDataBuf grid_data_;
  CubemapDataBuf cube_data_;

  /* Either scene lightcache or lookdev lightcache */
  LightCache *lightcache_ = nullptr;
  /* Own lightcache used for lookdev lighting or as fallback. */
  LightCache *lightcache_lookdev_ = nullptr;
  /* Temporary cache used for baking. */
  LightCache *lightcache_baking_ = nullptr;

  /* Used for rendering probes. */
  /* OPTI(fclem) Share for the whole scene? Only allocate temporary? */
  Texture cube_depth_tx_ = {"CubemapDepth"};
  Texture cube_color_tx_ = {"CubemapColor"};
  LightProbeView probe_views_[6];

  std::array<DRWView *, 6> face_view_ = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

  DRWPass *filter_cubemap_ps_ = nullptr;
  DRWPass *filter_diffuse_ps_ = nullptr;
  DRWPass *filter_visibility_ps_ = nullptr;

  DRWPass *display_ps_ = nullptr;

  /** Input texture view to filter pass. */
  GPUTexture *filter_input_tx_ = nullptr;
  /** Output texture view to filter pass. */
  GPUTexture *dst_view_lvl0_tx_ = nullptr;
  GPUTexture *dst_view_lvl1_tx_ = nullptr;
  GPUTexture *dst_view_lvl2_tx_ = nullptr;
  GPUTexture *dst_view_lvl3_tx_ = nullptr;
  GPUTexture *dst_view_lvl4_tx_ = nullptr;
  GPUTexture *dst_view_lvl5_tx_ = nullptr;
  /** Copy of actual textures from the lightcache_. */
  GPUTexture *active_grid_tx_ = nullptr;
  GPUTexture *active_cube_tx_ = nullptr;
  /** Constant values during baking. */
  float glossy_clamp_ = 0.0;
  float filter_quality_ = 0.0;
  /** Dispatch size for cubemap downsampling (filter). */
  int3 cube_dispatch_;

 public:
  LightProbeModule(Instance &inst)
      : inst_(inst),
        probe_views_{{inst, "posX_view", cubeface_mat[0], 0},
                     {inst, "negX_view", cubeface_mat[1], 1},
                     {inst, "posY_view", cubeface_mat[2], 2},
                     {inst, "negY_view", cubeface_mat[3], 3},
                     {inst, "posZ_view", cubeface_mat[4], 4},
                     {inst, "negZ_view", cubeface_mat[5], 5}}
  {
  }

  ~LightProbeModule()
  {
    MEM_delete(lightcache_lookdev_);
    MEM_delete(lightcache_baking_);
  }

  void init();

  void begin_sync();
  void end_sync();

  void set_view(const DRWView *view, const int2 extent);

  void set_world_dirty(void)
  {
    lightcache_->flag |= LIGHTCACHE_UPDATE_WORLD;
  }

  void swap_irradiance_cache(void)
  {
    if (lightcache_baking_ && lightcache_) {
      SWAP(GPUTexture *, lightcache_baking_->grid_tx.tex, lightcache_->grid_tx.tex);
    }
  }

  const GPUUniformBuf *grid_ubo_get() const
  {
    return grid_data_;
  }
  const GPUUniformBuf *cube_ubo_get() const
  {
    return cube_data_;
  }
  const GPUUniformBuf *info_ubo_get() const
  {
    return info_data_;
  }
  GPUTexture **grid_tx_ref_get()
  {
    return &active_grid_tx_;
  }
  GPUTexture **cube_tx_ref_get()
  {
    return &active_cube_tx_;
  }

  void bake(Depsgraph *depsgraph,
            int type,
            int index,
            int bounce,
            const float position[3],
            const LightProbe *probe = nullptr,
            float visibility_range = 0.0f);

  void draw_cache_display(void);

 private:
  void update_world_cache();

  void sync_world(const DRWView *view);
  void sync_grid(const DRWView *view, const struct LightGridCache &grid_cache, int grid_index);
  void sync_cubemap(const DRWView *view, const struct LightProbeCache &cube_cache, int cube_index);

  LightCache *baking_cache_get(void);

  void cubemap_prepare(float3 position, float near, float far, bool background_only);

  void filter_glossy(int cube_index, float intensity);
  void filter_diffuse(int sample_index, float intensity);
  void filter_visibility(int sample_index, float visibility_blur, float visibility_range);

  float lod_bias_from_cubemap(void)
  {
    float target_size_sq = square_f(GPU_texture_width(cube_color_tx_));
    return 0.5f * logf(target_size_sq / filter_data_.sample_count) / logf(2);
  }

  void cubemap_render(void);
};

}  // namespace blender::eevee
