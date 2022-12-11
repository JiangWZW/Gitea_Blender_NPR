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

  class GPUBufferPoolModule
  {
    friend class StrokeGenPassModule;

  private:
    /** Instance */
    Instance &instance;

    /** Compute Resources */
    StrokeGenTestBuf arr_buf_test_;


  public:
    GPUBufferPoolModule(Instance &inst) : instance(inst) {};
    ~GPUBufferPoolModule() {};

    void sync(Object* object);
    void end_sync();

  };
}
