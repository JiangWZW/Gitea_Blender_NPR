/* SPDX-License-Identifier: GPL-2.0-or-later
* Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Converts the different renderable object types to drawcalls.
 */

#include "bnpr_engine.h"

#include "BKE_gpencil.h"
#include "BKE_object.h"
#include "DEG_depsgraph_query.h"
#include "DNA_curves_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "bnpr_instance.hh"
#include "bnpr_sync.hh"

namespace blender::bnpr
{

  /* -------------------------------------------------------------------- */
  /** \name Draw Data
   *
   * \{ */

  static void draw_data_init_cb(struct DrawData *dd)
  {
    /* Object has just been created or was never evaluated by the engine. */
    dd->recalc = ID_RECALC_ALL; /* Tag given ID for an update in all the dependency graphs. */
  }

  // ObjectHandle& SyncModule::sync_object(Object* ob)
  // {
  //   DrawEngineType *owner = (DrawEngineType *)&DRW_engine_viewport_bnpr_type;
  //   return ObjectHandle();
  // }


}
