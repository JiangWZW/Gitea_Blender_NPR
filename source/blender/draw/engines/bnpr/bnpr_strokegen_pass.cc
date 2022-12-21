/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup bnpr
 *
 */

#include "bnpr_strokegen_pass.hh"

namespace blender::bnpr
{
  using namespace blender;

  void StrokeGenPassModule::rebuild_pass_extract_mesh_contour(Object* ob)
  {
    pass_comp_test.init();
    {
      auto& sub = pass_comp_test.sub("strokegen_comp_test_subpass");
      sub.shader_set(shaders_.static_shader_get(eShaderType::COMPUTE_TEST));
      sub.bind_ssbo("buf_test", buffers_.strokegen_test_buf_);
      sub.dispatch(int3(32, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
  }



}
