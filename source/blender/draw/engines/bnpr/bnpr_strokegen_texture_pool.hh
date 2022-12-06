/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup bnpr
 */

#pragma once

#include "bnpr_shader_shared.hh"

namespace blender::bnpr
{
  class Instance;

  class GPUTexturePoolModule
  {
  private:
    /** Compute Resources */
    StrokeGenTestBuf arr_buf_test_;

    /** Instance */
    Instance &instance;


  public:
    GPUTexturePoolModule(Instance &inst) : instance(inst) {};
    ~GPUTexturePoolModule() {};

    void sync(Object* object) {};
    void end_sync() {};

  };
}
