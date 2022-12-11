﻿/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup bnpr
 */

#pragma once

#include "draw_pass.hh"
#include "draw_manager.hh"

#include "bnpr_shader.hh"
#include "bnpr_shader_shared.hh"
#include "bnpr_strokegen_buffer_pool.hh"
#include "bnpr_strokegen_texture_pool.hh"
#include "bnpr_strokegen_pass.hh"

namespace blender::bnpr
{
  class Instance;

  class StrokeGenPassModule // similar to "LineDrawingRenderPass"
  {
  private:
    /** Compute Passes */
    draw::PassSimple pass_comp_test = {"Strokegen Compute Test"};

    /** Instance */
    ShaderModule &shaders_;
    GPUBufferPoolModule& buffers_;
    GPUTexturePoolModule& textures_;


  public:
    StrokeGenPassModule(
      ShaderModule          &strokegen_shaders,
      GPUBufferPoolModule   &strokegen_buffers,
      GPUTexturePoolModule  &strokegen_textures
    ) :
      shaders_(strokegen_shaders),
      buffers_(strokegen_buffers),
      textures_(strokegen_textures)
    {};

    ~StrokeGenPassModule() {};

    /** Passes Batched by Usages */
    void dispatch_extract_mesh_contour(Object* ob);



  };
}

