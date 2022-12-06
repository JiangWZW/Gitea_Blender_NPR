/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup bnpr
 */

#include "bnpr_strokegen_buffer_pool.hh"
#include "bnpr_instance.hh"

namespace blender::bnpr
{
  void GPUBufferPoolModule::sync(Object* object)
  {
  }

  void GPUBufferPoolModule::end_sync()
  {
    // arr_buf_test_.resize(4096); // maybe needed for a few special buffers
  }



}
