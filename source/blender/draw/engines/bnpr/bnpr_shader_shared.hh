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

#endif



#ifdef __cplusplus

// Template to set buffer size in compile time
using StrokeGenTestBuf = draw::StorageArrayBuffer<uint, 4096, true>;


}

// namespace blender::bnpr
#endif
