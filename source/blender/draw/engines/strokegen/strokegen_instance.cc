/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup strokegen
 *
 * An instance contains all structures needed to do a complete render.
 */

#include <sstream>

#include "BKE_global.h"
#include "BKE_object.h"
#include "BLI_rect.h"
#include "DEG_depsgraph_query.h"
#include "DNA_ID.h"
#include "DNA_lightprobe_types.h"
#include "DNA_modifier_types.h"
#include "RE_pipeline.h"

#include "strokegen_instance.hh"

namespace blender::strokegen
{
  /* -------------------------------------------------------------------- */
  /** \name Initialization
   *
   * Initialization functions need to be called once at the start of a frame.
   * Active camera, render extent and enabled render passes are immutable until next init.
   * This takes care of resizing output buffers and view in case a parameter changed.
   * IMPORTANT: xxx.init() functions are NOT meant to acquire and allocate DRW resources.
   * Any attempt to do so will likely produce use after free situations.
   * \{ */
  void Instance::init(
    Depsgraph* depsgraph_,
    Manager* manager_,
    const View3D* v3d_,
    const RegionView3D* rv3d_,
    const DRWView* drw_view_,
    Object* camera_object_)
  {
    /* Init things static per render frame. (Not render graph related) */
    depsgraph = depsgraph_;
    manager = manager_;

    drw_view = drw_view_;
    v3d = v3d_;
    rv3d = rv3d_;

    camera_orig_object = camera_object_;

    info = "";
  }

  void Instance::update_eval_members()
  {
    scene = DEG_get_evaluated_scene(depsgraph);
    camera_eval_object = (camera_orig_object) ?
                         DEG_get_evaluated_object(depsgraph, camera_orig_object) :
                         nullptr;
  }

  /** \} */






  /* -------------------------------------------------------------------- */
  /** \name Sync
   *
   * Sync will gather data from the scene that can change over a time step (i.e: motion steps).
   * IMPORTANT: xxx.sync() functions area responsible for creating DRW resources (i.e: DRWView) as
   * well as querying temp texture pool. All DRWPasses should be ready by the end end_sync().
   * \{ */
  void Instance::begin_sync(Manager& manager)
  {
    /* Init draw passes and manager related stuff. (Begin render graph) */
  }

  void Instance::object_sync(Manager& manager, ObjectRef& object_ref)
  {
    /* Add object draw calls to passes. (Populate render graph) */
  }

  void Instance::end_sync(Manager&)
  {
    /* Post processing after all object. (End render graph) */
  }

  /** \} */






  /* -------------------------------------------------------------------- */
  /** \name Rendering
   * \{ */

  void Instance::draw_viewport(Manager& manager, View& view, GPUTexture* depth_tx,
    GPUTexture* color_tx)
  {
    /* Submit passes here. (Execute render graph) */
  }

  /** \} */





}
