/* SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared structures, enums & defines between C++ and GLSL.
 * Can also include some math functions but they need to be simple enough to be valid in both
 * language.
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  pragma once

#  include "BLI_memory_utils.hh"
#  include "DRW_gpu_wrapper.hh"

#  include "draw_manager.hh"
#  include "draw_pass.hh"

#  include "bnpr_defines.hh"

#  include "GPU_shader_shared.h"

namespace blender::bnpr {

using namespace draw;

constexpr eGPUSamplerState no_filter = GPU_SAMPLER_DEFAULT;
constexpr eGPUSamplerState with_filter = GPU_SAMPLER_FILTER;

#endif



#define UBO_MIN_MAX_SUPPORTED_SIZE 1 << 14

/* -------------------------------------------------------------------- */
/** \name Debug Mode
 * \{ */

/** These are just to make more sense of G.debug_value's values. Reserved range is 1-30. */
enum eDebugMode : uint32_t {
  DEBUG_NONE = 0u,
  /**
   * Gradient showing light evaluation hot-spots.
   */
  DEBUG_LIGHT_CULLING = 1u,
  /**
   * Show incorrectly downsample tiles in red.
   */
  DEBUG_HIZ_VALIDATION = 2u,
  /**
   * Tile-maps to screen. Is also present in other modes.
   * - Black pixels, no pages allocated.
   * - Green pixels, pages cached.
   * - Red pixels, pages allocated.
   */
  DEBUG_SHADOW_TILEMAPS = 10u,
  /**
   * Random color per pages. Validates page density allocation and sampling.
   */
  DEBUG_SHADOW_PAGES = 11u,
  /**
   * Outputs random color per tile-map (or tile-map level). Validates tile-maps coverage.
   * Black means not covered by any tile-maps LOD of the shadow.
   */
  DEBUG_SHADOW_LOD = 12u,
  /**
   * Outputs white pixels for pages allocated and black pixels for unused pages.
   * This needs DEBUG_SHADOW_PAGE_ALLOCATION_ENABLED defined in order to work.
   */
  DEBUG_SHADOW_PAGE_ALLOCATION = 13u,
  /**
   * Outputs the tile-map atlas. Default tile-map is too big for the usual screen resolution.
   * Try lowering SHADOW_TILEMAP_PER_ROW and SHADOW_MAX_TILEMAP before using this option.
   */
  DEBUG_SHADOW_TILE_ALLOCATION = 14u,
  /**
   * Visualize linear depth stored in the atlas regions of the active light.
   * This way, one can check if the rendering, the copying and the shadow sampling functions works.
   */
  DEBUG_SHADOW_SHADOW_DEPTH = 15u
};
/** \} */




#ifdef __cplusplus

// Template to set buffer size in compile time
using StrokeGenTestBuf = draw::StorageArrayBuffer<uint, 4096, true>;


}


// namespace blender::bnpr
#endif
