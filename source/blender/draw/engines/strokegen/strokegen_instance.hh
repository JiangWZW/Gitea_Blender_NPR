/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup strokegen
 *
 * An renderer instance that contains all data to render a full frame.
*/

#pragma once

#include "BKE_image.h"
#include "DEG_depsgraph_query.h"
#include "DNA_shader_fx_types.h"
#include "DRW_engine.h"
#include "DRW_render.h"
#include "ED_view3d.h"
#include "GPU_capabilities.h"
#include "IMB_imbuf_types.h"

#include "draw_manager.hh"
#include "draw_pass.hh"
#include "strokegen_shader.hh"


namespace blender::strokegen
{
  using namespace draw;

  class Instance
  {
  private:


  public:
    Instance() : shaders(*ShaderModule::module_get())
    {
    };

    void init(Depsgraph* depsgraph, View3D* v3d)
    {
      /* Init things static per render frame. (Not render graph related) */
    }

    void begin_sync(Manager& /* manager */)
    {
      /* Init draw passes and manager related stuff. (Begin render graph) */
    }

    void object_sync(Manager& manager, ObjectRef& object_ref)
    {
      /* Add object draw calls to passes. (Populate render graph) */
    }

    void end_sync(Manager& /* manager */)
    {
      /* Post processing after all object. (End render graph) */
    }

    void draw_viewport(Manager& manager, View& view, GPUTexture* depth_tx,
                       GPUTexture* color_tx)
    {
      /* Submit passes here. (Execute render graph) */
    }



    ShaderModule shaders; // singleton class for handling GPUShader(s)


  };
}


