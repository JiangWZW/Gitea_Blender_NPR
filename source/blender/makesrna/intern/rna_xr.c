/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup RNA
 */

#include "BLI_math.h"

#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_xr_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BLI_listbase.h"

#  include "WM_api.h"

/* -------------------------------------------------------------------- */
/** \name XR Action Map
 * \{ */

static XrActionMapBinding *rna_XrActionMapBinding_new(XrActionMapItem *ami,
                                                      const char *name,
                                                      bool replace_existing)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_binding_new(ami, name, replace_existing);
#  else
  UNUSED_VARS(ami, name, replace_existing);
  return NULL;
#  endif
}

static XrActionMapBinding *rna_XrActionMapBinding_new_from_binding(XrActionMapItem *ami,
                                                                   XrActionMapBinding *amb_src)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_binding_add_copy(ami, amb_src);
#  else
  UNUSED_VARS(ami, amb_src);
  return NULL;
#  endif
}

static void rna_XrActionMapBinding_remove(XrActionMapItem *ami,
                                          ReportList *reports,
                                          PointerRNA *amb_ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapBinding *amb = amb_ptr->data;
  if (WM_xr_actionmap_binding_remove(ami, amb) == false) {
    BKE_reportf(reports,
                RPT_ERROR,
                "ActionMapBinding '%s' cannot be removed from '%s'",
                amb->name,
                ami->name);
    return;
  }
  RNA_POINTER_INVALIDATE(amb_ptr);
#  else
  UNUSED_VARS(ami, reports, amb_ptr);
#  endif
}

static XrActionMapBinding *rna_XrActionMapBinding_find(XrActionMapItem *ami, const char *name)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_binding_find(ami, name);
#  else
  UNUSED_VARS(ami, name);
  return NULL;
#  endif
}

static int rna_XrActionMapBinding_axis0_flag_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapBinding *amb = ptr->data;
  if ((amb->axis_flag & XR_AXIS0_POS) != 0) {
    return XR_AXIS0_POS;
  }
  if ((amb->axis_flag & XR_AXIS0_NEG) != 0) {
    return XR_AXIS0_NEG;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return 0;
}

static void rna_XrActionMapBinding_axis0_flag_set(PointerRNA *ptr, int value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapBinding *amb = ptr->data;
  amb->axis_flag &= ~(XR_AXIS0_POS | XR_AXIS0_NEG);
  amb->axis_flag |= value;
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static int rna_XrActionMapBinding_axis1_flag_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapBinding *amb = ptr->data;
  if ((amb->axis_flag & XR_AXIS1_POS) != 0) {
    return XR_AXIS1_POS;
  }
  if ((amb->axis_flag & XR_AXIS1_NEG) != 0) {
    return XR_AXIS1_NEG;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return 0;
}

static void rna_XrActionMapBinding_axis1_flag_set(PointerRNA *ptr, int value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapBinding *amb = ptr->data;
  amb->axis_flag &= ~(XR_AXIS1_POS | XR_AXIS1_NEG);
  amb->axis_flag |= value;
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static void rna_XrActionMapBinding_name_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = bmain->wm.first;
  if (wm && wm->xr.runtime) {
    ListBase *actionmaps = WM_xr_actionmaps_get(wm->xr.runtime);
    short idx = WM_xr_actionmap_selected_index_get(wm->xr.runtime);
    XrActionMap *actionmap = BLI_findlink(actionmaps, idx);
    if (actionmap) {
      XrActionMapItem *ami = BLI_findlink(&actionmap->items, actionmap->selitem);
      if (ami) {
        XrActionMapBinding *amb = ptr->data;
        WM_xr_actionmap_binding_ensure_unique(ami, amb);
      }
    }
  }
#  else
  UNUSED_VARS(bmain, ptr);
#  endif
}

static XrActionMapItem *rna_XrActionMapItem_new(XrActionMap *am,
                                                const char *name,
                                                bool replace_existing)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_item_new(am, name, replace_existing);
#  else
  UNUSED_VARS(am, name, replace_existing);
  return NULL;
#  endif
}

static XrActionMapItem *rna_XrActionMapItem_new_from_item(XrActionMap *am,
                                                          XrActionMapItem *ami_src)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_item_add_copy(am, ami_src);
#  else
  UNUSED_VARS(am, ami_src);
  return NULL;
#  endif
}

static void rna_XrActionMapItem_remove(XrActionMap *am, ReportList *reports, PointerRNA *ami_ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ami_ptr->data;
  if (WM_xr_actionmap_item_remove(am, ami) == false) {
    BKE_reportf(
        reports, RPT_ERROR, "ActionMapItem '%s' cannot be removed from '%s'", ami->name, am->name);
    return;
  }
  RNA_POINTER_INVALIDATE(ami_ptr);
#  else
  UNUSED_VARS(am, reports, ami_ptr);
#  endif
}

static XrActionMapItem *rna_XrActionMapItem_find(XrActionMap *am, const char *name)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_item_find(am, name);
#  else
  UNUSED_VARS(am, name);
  return NULL;
#  endif
}

static void rna_XrActionMapItem_op_name_get(PointerRNA *ptr, char *value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if (ami->op[0]) {
    if (ami->op_properties_ptr) {
      wmOperatorType *ot = WM_operatortype_find(ami->op, 1);
      if (ot) {
        strcpy(value, WM_operatortype_name(ot, ami->op_properties_ptr));
        return;
      }
    }
    strcpy(value, ami->op);
    return;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  value[0] = '\0';
}

static int rna_XrActionMapItem_op_name_length(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if (ami->op[0]) {
    if (ami->op_properties_ptr) {
      wmOperatorType *ot = WM_operatortype_find(ami->op, 1);
      if (ot) {
        return strlen(WM_operatortype_name(ot, ami->op_properties_ptr));
      }
    }
    return strlen(ami->op);
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return 0;
}

static PointerRNA rna_XrActionMapItem_op_properties_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if (ami->op_properties_ptr) {
    return *(ami->op_properties_ptr);
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return PointerRNA_NULL;
}

static bool rna_XrActionMapItem_bimanual_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if ((ami->action_flag & XR_ACTION_BIMANUAL) != 0) {
    return true;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return false;
}

static void rna_XrActionMapItem_bimanual_set(PointerRNA *ptr, bool value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if (value) {
    ami->action_flag |= XR_ACTION_BIMANUAL;
  }
  else {
    ami->action_flag &= ~XR_ACTION_BIMANUAL;
  }
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static bool rna_XrActionMapItem_haptic_match_user_paths_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if ((ami->haptic_flag & XR_HAPTIC_MATCHUSERPATHS) != 0) {
    return true;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return false;
}

static void rna_XrActionMapItem_haptic_match_user_paths_set(PointerRNA *ptr, bool value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if (value) {
    ami->haptic_flag |= XR_HAPTIC_MATCHUSERPATHS;
  }
  else {
    ami->haptic_flag &= ~XR_HAPTIC_MATCHUSERPATHS;
  }
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static int rna_XrActionMapItem_haptic_flag_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if ((ami->haptic_flag & XR_HAPTIC_RELEASE) != 0) {
    return ((ami->haptic_flag & XR_HAPTIC_PRESS) != 0) ? (XR_HAPTIC_PRESS | XR_HAPTIC_RELEASE) :
                                                         XR_HAPTIC_RELEASE;
  }
  if ((ami->haptic_flag & XR_HAPTIC_REPEAT) != 0) {
    return XR_HAPTIC_REPEAT;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return XR_HAPTIC_PRESS;
}

static void rna_XrActionMapItem_haptic_flag_set(PointerRNA *ptr, int value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  ami->haptic_flag &= ~(XR_HAPTIC_PRESS | XR_HAPTIC_RELEASE | XR_HAPTIC_REPEAT);
  ami->haptic_flag |= value;
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static bool rna_XrActionMapItem_pose_is_controller_grip_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if ((ami->pose_flag & XR_POSE_GRIP) != 0) {
    return true;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return false;
}

static void rna_XrActionMapItem_pose_is_controller_grip_set(PointerRNA *ptr, bool value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  SET_FLAG_FROM_TEST(ami->pose_flag, value, XR_POSE_GRIP);
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static bool rna_XrActionMapItem_pose_is_controller_aim_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  if ((ami->pose_flag & XR_POSE_AIM) != 0) {
    return true;
  }
#  else
  UNUSED_VARS(ptr);
#  endif
  return false;
}

static void rna_XrActionMapItem_pose_is_controller_aim_set(PointerRNA *ptr, bool value)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  SET_FLAG_FROM_TEST(ami->pose_flag, value, XR_POSE_AIM);
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static void rna_XrActionMapItem_name_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = bmain->wm.first;
  if (wm && wm->xr.runtime) {
    ListBase *actionmaps = WM_xr_actionmaps_get(wm->xr.runtime);
    short idx = WM_xr_actionmap_selected_index_get(wm->xr.runtime);
    XrActionMap *actionmap = BLI_findlink(actionmaps, idx);
    if (actionmap) {
      XrActionMapItem *ami = ptr->data;
      WM_xr_actionmap_item_ensure_unique(actionmap, ami);
    }
  }
#  else
  UNUSED_VARS(bmain, ptr);
#  endif
}

static void rna_XrActionMapItem_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMapItem *ami = ptr->data;
  WM_xr_actionmap_item_properties_update_ot(ami);
#  else
  UNUSED_VARS(ptr);
#  endif
}

static XrActionMap *rna_XrActionMap_new(wmXrData *xr, const char *name, bool replace_existing)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_new(xr->runtime, name, replace_existing);
#  else
  UNUSED_VARS(xr, name, replace_existing);
  return NULL;
#  endif
}

static XrActionMap *rna_XrActionMap_new_from_actionmap(wmXrData *xr, XrActionMap *am_src)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_add_copy(xr->runtime, am_src);
#  else
  UNUSED_VARS(xr, am_src);
  return NULL;
#  endif
}

static void rna_XrActionMap_remove(wmXrData *xr, ReportList *reports, PointerRNA *actionmap_ptr)
{
#  ifdef WITH_XR_OPENXR
  XrActionMap *actionmap = actionmap_ptr->data;
  if (WM_xr_actionmap_remove(xr->runtime, actionmap) == false) {
    BKE_reportf(reports, RPT_ERROR, "ActionMap '%s' cannot be removed", actionmap->name);
    return;
  }
  RNA_POINTER_INVALIDATE(actionmap_ptr);
#  else
  UNUSED_VARS(xr, reports, actionmap_ptr);
#  endif
}

static XrActionMap *rna_XrActionMap_find(wmXrData *xr, const char *name)
{
#  ifdef WITH_XR_OPENXR
  return WM_xr_actionmap_find(xr->runtime, name);
#  else
  UNUSED_VARS(xr, name);
  return NULL;
#  endif
}

static void rna_XrActionMap_name_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = bmain->wm.first;
  if (wm && wm->xr.runtime) {
    XrActionMap *actionmap = ptr->data;
    WM_xr_actionmap_ensure_unique(wm->xr.runtime, actionmap);
  }
#  else
  UNUSED_VARS(bmain, ptr);
#  endif
}

/** \} */

/* -------------------------------------------------------------------- */

#  ifdef WITH_XR_OPENXR
static wmXrData *rna_XrSession_wm_xr_data_get(PointerRNA *ptr)
{
  /* Callers could also get XrSessionState pointer through ptr->data, but prefer if we just
   * consistently pass wmXrData pointers to the WM_xr_xxx() API. */

  BLI_assert((ptr->type == &RNA_XrSessionSettings) || (ptr->type == &RNA_XrSessionState));

  wmWindowManager *wm = (wmWindowManager *)ptr->owner_id;
  BLI_assert(wm && (GS(wm->id.name) == ID_WM));

  return &wm->xr;
}
#  endif

/* -------------------------------------------------------------------- */
/** \name XR Session Settings
 * \{ */

static bool rna_XrSessionSettings_use_positional_tracking_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  const wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  return (xr->session_settings.flag & XR_SESSION_USE_POSITION_TRACKING) != 0;
#  else
  UNUSED_VARS(ptr);
  return false;
#  endif
}

static void rna_XrSessionSettings_use_positional_tracking_set(PointerRNA *ptr, bool value)
{
#  ifdef WITH_XR_OPENXR
  wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  SET_FLAG_FROM_TEST(xr->session_settings.flag, value, XR_SESSION_USE_POSITION_TRACKING);
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static bool rna_XrSessionSettings_use_absolute_tracking_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  const wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  return (xr->session_settings.flag & XR_SESSION_USE_ABSOLUTE_TRACKING) != 0;
#  else
  UNUSED_VARS(ptr);
  return false;
#  endif
}

static void rna_XrSessionSettings_use_absolute_tracking_set(PointerRNA *ptr, bool value)
{
#  ifdef WITH_XR_OPENXR
  wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  SET_FLAG_FROM_TEST(xr->session_settings.flag, value, XR_SESSION_USE_ABSOLUTE_TRACKING);
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Session State
 * \{ */

static bool rna_XrSessionState_is_running(bContext *C)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = CTX_wm_manager(C);
  return WM_xr_session_exists(&wm->xr);
#  else
  UNUSED_VARS(C);
  return false;
#  endif
}

static void rna_XrSessionState_reset_to_base_pose(bContext *C)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  WM_xr_session_base_pose_reset(&wm->xr);
#  else
  UNUSED_VARS(C);
#  endif
}

static bool rna_XrSessionState_action_set_create(bContext *C, XrActionMap *actionmap)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  return WM_xr_action_set_create(&wm->xr, actionmap->name);
#  else
  UNUSED_VARS(C, actionmap);
  return false;
#  endif
}

static bool rna_XrSessionState_action_create(bContext *C,
                                             XrActionMap *actionmap,
                                             XrActionMapItem *ami)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  unsigned int count_subaction_paths = 0;
  const char *subaction_paths[2];

  if (ami->user_path0[0]) {
    subaction_paths[0] = ami->user_path0;
    ++count_subaction_paths;

    if (ami->user_path1[0]) {
      subaction_paths[1] = ami->user_path1;
      ++count_subaction_paths;
    }
  }
  else {
    if (ami->user_path1[0]) {
      subaction_paths[0] = ami->user_path1;
      ++count_subaction_paths;
    }
    else {
      return false;
    }
  }

  const bool is_float_action = (ami->type == XR_FLOAT_INPUT || ami->type == XR_VECTOR2F_INPUT);
  const bool is_button_action = (is_float_action || ami->type == XR_BOOLEAN_INPUT);
  wmOperatorType *ot = NULL;
  IDProperty *op_properties = NULL;
  const char *haptic_name = NULL;
  int64_t haptic_duration_msec;

  if (is_button_action) {
    if (ami->op[0]) {
      char idname[OP_MAX_TYPENAME];
      WM_operator_bl_idname(idname, ami->op);
      ot = WM_operatortype_find(idname, true);
      if (ot) {
        op_properties = ami->op_properties;
      }
    }

    haptic_name = &ami->haptic_name[0];
    haptic_duration_msec = (int64_t)(ami->haptic_duration * 1000.0f);
  }

  return WM_xr_action_create(&wm->xr,
                             actionmap->name,
                             ami->name,
                             ami->type,
                             count_subaction_paths,
                             subaction_paths,
                             ot,
                             op_properties,
                             is_button_action ? &haptic_name : NULL,
                             is_button_action ? &haptic_duration_msec : NULL,
                             is_button_action ? &ami->haptic_frequency : NULL,
                             is_button_action ? &ami->haptic_amplitude : NULL,
                             ami->op_flag,
                             ami->action_flag,
                             ami->haptic_flag);
#  else
  UNUSED_VARS(C, actionmap, ami);
  return false;
#  endif
}

static bool rna_XrSessionState_action_binding_create(bContext *C,
                                                     XrActionMap *actionmap,
                                                     XrActionMapItem *ami,
                                                     XrActionMapBinding *amb)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  unsigned int count_subaction_paths = 0;
  const char *subaction_paths[2];
  const char *component_paths[2];

  if (ami->user_path0[0]) {
    subaction_paths[0] = ami->user_path0;
    component_paths[0] = amb->component_path0;
    ++count_subaction_paths;

    if (ami->user_path1[0]) {
      subaction_paths[1] = ami->user_path1;
      component_paths[1] = amb->component_path1;
      ++count_subaction_paths;
    }
  }
  else {
    if (ami->user_path1[0]) {
      subaction_paths[0] = ami->user_path1;
      component_paths[0] = amb->component_path1;
      ++count_subaction_paths;
    }
    else {
      return false;
    }
  }

  const bool is_float_action = (ami->type == XR_FLOAT_INPUT || ami->type == XR_VECTOR2F_INPUT);
  const bool is_button_action = (is_float_action || ami->type == XR_BOOLEAN_INPUT);
  const bool is_pose_action = (ami->type == XR_POSE_INPUT);
  float float_thresholds[2];
  eXrAxisFlag axis_flags[2];
  wmXrPose poses[2];

  if (is_float_action) {
    float_thresholds[0] = float_thresholds[1] = amb->float_threshold;
  }
  if (is_button_action) {
    axis_flags[0] = axis_flags[1] = amb->axis_flag;
  }
  if (is_pose_action) {
    copy_v3_v3(poses[0].position, amb->pose_location);
    eul_to_quat(poses[0].orientation_quat, amb->pose_rotation);
    normalize_qt(poses[0].orientation_quat);
    memcpy(&poses[1], &poses[0], sizeof(poses[1]));
  }

  return WM_xr_action_binding_create(&wm->xr,
                                     actionmap->name,
                                     ami->name,
                                     amb->profile,
                                     count_subaction_paths,
                                     subaction_paths,
                                     component_paths,
                                     is_float_action ? float_thresholds : NULL,
                                     is_button_action ? axis_flags : NULL,
                                     is_pose_action ? poses : NULL);
#  else
  UNUSED_VARS(C, actionmap, ami, amb);
  return false;
#  endif
}

bool rna_XrSessionState_active_action_set_set(bContext *C, const char *action_set_name)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  return WM_xr_active_action_set_set(&wm->xr, action_set_name);
#  else
  UNUSED_VARS(C, action_set_name);
  return false;
#  endif
}

bool rna_XrSessionState_controller_pose_actions_set(bContext *C,
                                                    const char *action_set_name,
                                                    const char *grip_action_name,
                                                    const char *aim_action_name)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  return WM_xr_controller_pose_actions_set(
      &wm->xr, action_set_name, grip_action_name, aim_action_name);
#  else
  UNUSED_VARS(C, action_set_name, grip_action_name, aim_action_name);
  return false;
#  endif
}

void rna_XrSessionState_action_state_get(bContext *C,
                                         const char *action_set_name,
                                         const char *action_name,
                                         const char *user_path,
                                         float r_state[2])
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrActionState state;
  if (WM_xr_action_state_get(&wm->xr, action_set_name, action_name, user_path, &state)) {
    switch (state.type) {
      case XR_BOOLEAN_INPUT:
        r_state[0] = (float)state.state_boolean;
        r_state[1] = 0.0f;
        return;
      case XR_FLOAT_INPUT:
        r_state[0] = state.state_float;
        r_state[1] = 0.0f;
        return;
      case XR_VECTOR2F_INPUT:
        copy_v2_v2(r_state, state.state_vector2f);
        return;
      case XR_POSE_INPUT:
      case XR_VIBRATION_OUTPUT:
        BLI_assert_unreachable();
        break;
    }
  }
#  else
  UNUSED_VARS(C, action_set_name, action_name, user_path);
#  endif
  zero_v2(r_state);
}

bool rna_XrSessionState_haptic_action_apply(bContext *C,
                                            const char *action_set_name,
                                            const char *action_name,
                                            const char *user_path,
                                            float duration,
                                            float frequency,
                                            float amplitude)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  int64_t duration_msec = (int64_t)(duration * 1000.0f);
  return WM_xr_haptic_action_apply(&wm->xr,
                                   action_set_name,
                                   action_name,
                                   user_path[0] ? &user_path : NULL,
                                   &duration_msec,
                                   &frequency,
                                   &amplitude);
#  else
  UNUSED_VARS(C, action_set_name, action_name, user_path, duration, frequency, amplitude);
  return false;
#  endif
}

void rna_XrSessionState_haptic_action_stop(bContext *C,
                                           const char *action_set_name,
                                           const char *action_name,
                                           const char *user_path)
{
#  ifdef WITH_XR_OPENXR
  wmWindowManager *wm = CTX_wm_manager(C);
  WM_xr_haptic_action_stop(
      &wm->xr, action_set_name, action_name, user_path[0] ? &user_path : NULL);
#  else
  UNUSED_VARS(C, action_set_name, action_name, user_path);
#  endif
}

static void rna_XrSessionState_controller_grip_location_get(bContext *C,
                                                            int *index,
                                                            float *r_values)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = CTX_wm_manager(C);
  WM_xr_session_state_controller_grip_location_get(&wm->xr, *index, r_values);
#  else
  UNUSED_VARS(C, index);
  zero_v3(r_values);
#  endif
}

static void rna_XrSessionState_controller_grip_rotation_get(bContext *C,
                                                            int index,
                                                            float *r_values)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = CTX_wm_manager(C);
  WM_xr_session_state_controller_grip_rotation_get(&wm->xr, index, r_values);
#  else
  UNUSED_VARS(C, index);
  unit_qt(r_values);
#  endif
}

static void rna_XrSessionState_controller_aim_location_get(bContext *C,
                                                           int *index,
                                                           float *r_values)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = CTX_wm_manager(C);
  WM_xr_session_state_controller_aim_location_get(&wm->xr, *index, r_values);
#  else
  UNUSED_VARS(C, index);
  zero_v3(r_values);
#  endif
}

static void rna_XrSessionState_controller_aim_rotation_get(bContext *C, int index, float *r_values)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = CTX_wm_manager(C);
  WM_xr_session_state_controller_aim_rotation_get(&wm->xr, index, r_values);
#  else
  UNUSED_VARS(C, index);
  unit_qt(r_values);
#  endif
}

static void rna_XrSessionState_viewer_pose_location_get(PointerRNA *ptr, float *r_values)
{
#  ifdef WITH_XR_OPENXR
  const wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  WM_xr_session_state_viewer_pose_location_get(xr, r_values);
#  else
  UNUSED_VARS(ptr);
  zero_v3(r_values);
#  endif
}

static void rna_XrSessionState_viewer_pose_rotation_get(PointerRNA *ptr, float *r_values)
{
#  ifdef WITH_XR_OPENXR
  const wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  WM_xr_session_state_viewer_pose_rotation_get(xr, r_values);
#  else
  UNUSED_VARS(ptr);
  unit_qt(r_values);
#  endif
}

static void rna_XrSessionState_actionmaps_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  ListBase *lb = WM_xr_actionmaps_get(xr->runtime);
  rna_iterator_listbase_begin(iter, lb, NULL);
}

static int rna_XrSessionState_active_actionmap_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  const wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  return WM_xr_actionmap_active_index_get(xr->runtime);
#  else
  UNUSED_VARS(ptr);
  return -1;
#  endif
}

static void rna_XrSessionState_active_actionmap_set(PointerRNA *ptr, int value)
{
#  ifdef WITH_XR_OPENXR
  wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  WM_xr_actionmap_active_index_set(xr->runtime, (short)value);
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

static int rna_XrSessionState_selected_actionmap_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  const wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  return WM_xr_actionmap_selected_index_get(xr->runtime);
#  else
  UNUSED_VARS(ptr);
  return -1;
#  endif
}

static void rna_XrSessionState_selected_actionmap_set(PointerRNA *ptr, int value)
{
#  ifdef WITH_XR_OPENXR
  wmXrData *xr = rna_XrSession_wm_xr_data_get(ptr);
  WM_xr_actionmap_selected_index_set(xr->runtime, (short)value);
#  else
  UNUSED_VARS(ptr, value);
#  endif
}

/** \} */

#else /* RNA_RUNTIME */

/* -------------------------------------------------------------------- */

static const EnumPropertyItem rna_enum_xr_action_types[] = {
    {XR_FLOAT_INPUT,
     "FLOAT",
     0,
     "Float",
     "Float action, representing either a digital or analog button"},
    {XR_VECTOR2F_INPUT,
     "VECTOR2D",
     0,
     "Vector2D",
     "2D float vector action, representing a thumbstick or trackpad"},
    {XR_POSE_INPUT,
     "POSE",
     0,
     "Pose",
     "3D pose action, representing a controller's location and rotation"},
    {XR_VIBRATION_OUTPUT,
     "VIBRATION",
     0,
     "Vibration",
     "Haptic vibration output action, to be applied with a duration, frequency, and amplitude"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_xr_op_flags[] = {
    {XR_OP_PRESS,
     "PRESS",
     0,
     "Press",
     "Execute operator on button press (non-modal operators only)"},
    {XR_OP_RELEASE,
     "RELEASE",
     0,
     "Release",
     "Execute operator on button release (non-modal operators only)"},
    {XR_OP_MODAL, "MODAL", 0, "Modal", "Use modal execution (modal operators only)"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_xr_haptic_flags[] = {
    {XR_HAPTIC_PRESS, "PRESS", 0, "Press", "Apply haptics on button press"},
    {XR_HAPTIC_RELEASE, "RELEASE", 0, "Release", "Apply haptics on button release"},
    {XR_HAPTIC_PRESS | XR_HAPTIC_RELEASE,
     "PRESS_RELEASE",
     0,
     "Press Release",
     "Apply haptics on button press and release"},
    {XR_HAPTIC_REPEAT,
     "REPEAT",
     0,
     "Repeat",
     "Apply haptics repeatedly for the duration of the button press"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_xr_axis0_flags[] = {
    {0, "ANY", 0, "Any", "Use any axis region for operator execution"},
    {XR_AXIS0_POS,
     "POSITIVE",
     0,
     "Positive",
     "Use positive axis region only for operator execution"},
    {XR_AXIS0_NEG,
     "NEGATIVE",
     0,
     "Negative",
     "Use negative axis region only for operator execution"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_xr_axis1_flags[] = {
    {0, "ANY", 0, "Any", "Use any axis region for operator execution"},
    {XR_AXIS1_POS,
     "POSITIVE",
     0,
     "Positive",
     "Use positive axis region only for operator execution"},
    {XR_AXIS1_NEG,
     "NEGATIVE",
     0,
     "Negative",
     "Use negative axis region only for operator execution"},
    {0, NULL, 0, NULL, NULL},
};

/* -------------------------------------------------------------------- */
/** \name XR Action Map
 * \{ */

static void rna_def_xr_actionmap_bindings(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "XrActionMapBindings");
  srna = RNA_def_struct(brna, "XrActionMapBindings", NULL);
  RNA_def_struct_sdna(srna, "XrActionMapItem");
  RNA_def_struct_ui_text(srna, "XR Action Map Bindings", "Collection of XR action map bindings");

  func = RNA_def_function(srna, "new", "rna_XrActionMapBinding_new");
  parm = RNA_def_string(func, "name", NULL, 0, "Name of the action map binding", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "replace_existing",
                         true,
                         "Replace Existing",
                         "Replace any existing binding with the same name");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "binding", "XrActionMapBinding", "Binding", "Added action map binding");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_from_binding", "rna_XrActionMapBinding_new_from_binding");
  parm = RNA_def_pointer(
      func, "binding", "XrActionMapBinding", "Binding", "Binding to use as a reference");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "result", "XrActionMapBinding", "Binding", "Added action map binding");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_XrActionMapBinding_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "binding", "XrActionMapBinding", "Binding", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "find", "rna_XrActionMapBinding_find");
  parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func,
                         "binding",
                         "XrActionMapBinding",
                         "Binding",
                         "The action map binding with the given name");
  RNA_def_function_return(func, parm);
}

static void rna_def_xr_actionmap_items(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "XrActionMapItems");
  srna = RNA_def_struct(brna, "XrActionMapItems", NULL);
  RNA_def_struct_sdna(srna, "XrActionMap");
  RNA_def_struct_ui_text(srna, "XR Action Map Items", "Collection of XR action map items");

  func = RNA_def_function(srna, "new", "rna_XrActionMapItem_new");
  parm = RNA_def_string(func, "name", NULL, 0, "Name of the action map item", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "replace_existing",
                         true,
                         "Replace Existing",
                         "Replace any existing item with the same name");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "item", "XrActionMapItem", "Item", "Added action map item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_from_item", "rna_XrActionMapItem_new_from_item");
  parm = RNA_def_pointer(func, "item", "XrActionMapItem", "Item", "Item to use as a reference");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "XrActionMapItem", "Item", "Added action map item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_XrActionMapItem_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "XrActionMapItem", "Item", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "find", "rna_XrActionMapItem_find");
  parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "item", "XrActionMapItem", "Item", "The action map item with the given name");
  RNA_def_function_return(func, parm);
}

static void rna_def_xr_actionmaps(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "XrActionMaps");
  srna = RNA_def_struct(brna, "XrActionMaps", NULL);
  RNA_def_struct_sdna(srna, "wmXrData");
  RNA_def_struct_ui_text(srna, "XR Action Maps", "Collection of XR action maps");

  func = RNA_def_function(srna, "new", "rna_XrActionMap_new");
  parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "replace_existing",
                         true,
                         "Replace Existing",
                         "Replace any existing actionmap with the same name");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap", "XrActionMap", "Action Map", "Added action map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_from_actionmap", "rna_XrActionMap_new_from_actionmap");
  parm = RNA_def_pointer(
      func, "actionmap", "XrActionMap", "Action Map", "Action map to use as a reference");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "XrActionMap", "Action Map", "Added action map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_XrActionMap_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "actionmap", "XrActionMap", "Action Map", "Removed action map");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "find", "rna_XrActionMap_find");
  parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "actionmap", "XrActionMap", "Action Map", "The action map with the given name");
  RNA_def_function_return(func, parm);
}

static void rna_def_xr_actionmap(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* XrActionMap */
  srna = RNA_def_struct(brna, "XrActionMap", NULL);
  RNA_def_struct_sdna(srna, "XrActionMap");
  RNA_def_struct_ui_text(srna, "XR Action Map", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of the action map");
  RNA_def_property_update(prop, 0, "rna_XrActionMap_name_update");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "actionmap_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "items", NULL);
  RNA_def_property_struct_type(prop, "XrActionMapItem");
  RNA_def_property_ui_text(
      prop,
      "Items",
      "Items in the action map, mapping an XR event to an operator, pose, or haptic output");
  rna_def_xr_actionmap_items(brna, prop);

  prop = RNA_def_property(srna, "selected_item", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "selitem");
  RNA_def_property_ui_text(prop, "Selected Item", "");

  /* XrActionMapItem */
  srna = RNA_def_struct(brna, "XrActionMapItem", NULL);
  RNA_def_struct_sdna(srna, "XrActionMapItem");
  RNA_def_struct_ui_text(srna, "XR Action Map Item", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of the action map item");
  RNA_def_property_update(prop, 0, "rna_XrActionMapItem_name_update");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_xr_action_types);
  RNA_def_property_ui_text(prop, "Type", "Action type");
  RNA_def_property_update(prop, 0, "rna_XrActionMapItem_update");

  prop = RNA_def_property(srna, "user_path0", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, 64);
  RNA_def_property_ui_text(prop, "User Path 0", "OpenXR user path");

  prop = RNA_def_property(srna, "user_path1", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, 64);
  RNA_def_property_ui_text(prop, "User Path 1", "OpenXR user path");

  prop = RNA_def_property(srna, "op", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME);
  RNA_def_property_ui_text(prop, "Operator", "Identifier of operator to call on action event");
  RNA_def_property_update(prop, 0, "rna_XrActionMapItem_update");

  prop = RNA_def_property(srna, "op_name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Operator Name", "Name of operator (translated) to call on action event");
  RNA_def_property_string_funcs(
      prop, "rna_XrActionMapItem_op_name_get", "rna_XrActionMapItem_op_name_length", NULL);

  prop = RNA_def_property(srna, "op_properties", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "OperatorProperties");
  RNA_def_property_pointer_funcs(prop, "rna_XrActionMapItem_op_properties_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Operator Properties", "Properties to set when the operator is called");
  RNA_def_property_update(prop, 0, "rna_XrActionMapItem_update");

  prop = RNA_def_property(srna, "op_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "op_flag");
  RNA_def_property_enum_items(prop, rna_enum_xr_op_flags);
  RNA_def_property_ui_text(prop, "Operator Mode", "Operator execution mode");

  prop = RNA_def_property(srna, "bimanual", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_XrActionMapItem_bimanual_get", "rna_XrActionMapItem_bimanual_set");
  RNA_def_property_ui_text(
      prop, "Bimanual", "The action depends on the states/poses of both user paths");

  prop = RNA_def_property(srna, "pose_is_controller_grip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_XrActionMapItem_pose_is_controller_grip_get",
                                 "rna_XrActionMapItem_pose_is_controller_grip_set");
  RNA_def_property_ui_text(
      prop, "Is Controller Grip", "The action poses will be used for the VR controller grips");

  prop = RNA_def_property(srna, "pose_is_controller_aim", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_XrActionMapItem_pose_is_controller_aim_get",
                                 "rna_XrActionMapItem_pose_is_controller_aim_set");
  RNA_def_property_ui_text(
      prop, "Is Controller Aim", "The action poses will be used for the VR controller aims");

  prop = RNA_def_property(srna, "haptic_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Haptic Name", "Name of the haptic action to apply when executing this action");

  prop = RNA_def_property(srna, "haptic_match_user_paths", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_XrActionMapItem_haptic_match_user_paths_get",
                                 "rna_XrActionMapItem_haptic_match_user_paths_set");
  RNA_def_property_ui_text(
      prop,
      "Haptic Match User Paths",
      "Apply haptics to the same user paths for the haptic action and this action");

  prop = RNA_def_property(srna, "haptic_duration", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Haptic Duration",
                           "Haptic duration in seconds. 0.0 is the minimum supported duration");

  prop = RNA_def_property(srna, "haptic_frequency", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Haptic Frequency",
                           "Frequency of the haptic vibration in hertz. 0.0 specifies the OpenXR "
                           "runtime's default frequency");

  prop = RNA_def_property(srna, "haptic_amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(
      prop, "Haptic Amplitude", "Intensity of the haptic vibration, ranging from 0.0 to 1.0");

  prop = RNA_def_property(srna, "haptic_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_xr_haptic_flags);
  RNA_def_property_enum_funcs(
      prop, "rna_XrActionMapItem_haptic_flag_get", "rna_XrActionMapItem_haptic_flag_set", NULL);
  RNA_def_property_ui_text(prop, "Haptic mode", "Haptic application mode");

  prop = RNA_def_property(srna, "bindings", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "XrActionMapBinding");
  RNA_def_property_ui_text(
      prop, "Bindings", "Bindings for the action map item, mapping the action to an XR input");
  rna_def_xr_actionmap_bindings(brna, prop);

  prop = RNA_def_property(srna, "selected_binding", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "selbinding");
  RNA_def_property_ui_text(prop, "Selected Binding", "Currently selected binding");

  /* XrActionMapBinding */
  srna = RNA_def_struct(brna, "XrActionMapBinding", NULL);
  RNA_def_struct_sdna(srna, "XrActionMapBinding");
  RNA_def_struct_ui_text(srna, "XR Action Map Binding", "Binding in an XR action map item");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of the action map binding");
  RNA_def_property_update(prop, 0, "rna_XrActionMapBinding_name_update");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "profile", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, 256);
  RNA_def_property_ui_text(prop, "Profile", "OpenXR interaction profile path");

  prop = RNA_def_property(srna, "component_path0", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, 192);
  RNA_def_property_ui_text(prop, "Component Path 0", "OpenXR component path");

  prop = RNA_def_property(srna, "component_path1", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, 192);
  RNA_def_property_ui_text(prop, "Component Path 1", "OpenXR component path");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "float_threshold");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Threshold", "Input threshold for button/axis actions");

  prop = RNA_def_property(srna, "axis0_region", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_xr_axis0_flags);
  RNA_def_property_enum_funcs(prop,
                              "rna_XrActionMapBinding_axis0_flag_get",
                              "rna_XrActionMapBinding_axis0_flag_set",
                              NULL);
  RNA_def_property_ui_text(
      prop, "Axis 0 Region", "Action execution region for the first input axis");

  prop = RNA_def_property(srna, "axis1_region", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_xr_axis1_flags);
  RNA_def_property_enum_funcs(prop,
                              "rna_XrActionMapBinding_axis1_flag_get",
                              "rna_XrActionMapBinding_axis1_flag_set",
                              NULL);
  RNA_def_property_ui_text(
      prop, "Axis 1 Region", "Action execution region for the second input axis");

  prop = RNA_def_property(srna, "pose_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_ui_text(prop, "Pose Location Offset", "");

  prop = RNA_def_property(srna, "pose_rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_ui_text(prop, "Pose Rotation Offset", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Session Settings
 * \{ */

static void rna_def_xr_session_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem base_pose_types[] = {
      {XR_BASE_POSE_SCENE_CAMERA,
       "SCENE_CAMERA",
       0,
       "Scene Camera",
       "Follow the active scene camera to define the VR view's base pose"},
      {XR_BASE_POSE_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Follow the transformation of an object to define the VR view's base pose"},
      {XR_BASE_POSE_CUSTOM,
       "CUSTOM",
       0,
       "Custom",
       "Follow a custom transformation to define the VR view's base pose"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "XrSessionSettings", NULL);
  RNA_def_struct_ui_text(srna, "XR Session Settings", "");

  prop = RNA_def_property(srna, "shading", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Shading Settings", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "base_pose_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, base_pose_types);
  RNA_def_property_ui_text(
      prop,
      "Base Pose Type",
      "Define where the location and rotation for the VR view come from, to which "
      "translation and rotation deltas from the VR headset will be applied to");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "base_pose_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Base Pose Object",
                           "Object to take the location and rotation to which translation and "
                           "rotation deltas from the VR headset will be applied to");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "base_pose_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_ui_text(prop,
                           "Base Pose Location",
                           "Coordinates to apply translation deltas from the VR headset to");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "base_pose_angle", PROP_FLOAT, PROP_AXISANGLE);
  RNA_def_property_ui_text(
      prop,
      "Base Pose Angle",
      "Rotation angle around the Z-Axis to apply the rotation deltas from the VR headset to");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "show_floor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "draw_flags", V3D_OFSDRAW_SHOW_GRIDFLOOR);
  RNA_def_property_ui_text(prop, "Display Grid Floor", "Show the ground plane grid");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "draw_flags", V3D_OFSDRAW_SHOW_ANNOTATION);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(prop, "Clip Start", "VR viewport near clipping distance");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(prop, "Clip End", "VR viewport far clipping distance");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "use_positional_tracking", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_XrSessionSettings_use_positional_tracking_get",
                                 "rna_XrSessionSettings_use_positional_tracking_set");
  RNA_def_property_ui_text(
      prop,
      "Positional Tracking",
      "Allow VR headsets to affect the location in virtual space, in addition to the rotation");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);

  prop = RNA_def_property(srna, "use_absolute_tracking", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_XrSessionSettings_use_absolute_tracking_get",
                                 "rna_XrSessionSettings_use_absolute_tracking_set");
  RNA_def_property_ui_text(
      prop, "Absolute Tracking", "Use unadjusted location/rotation as defined by the XR runtime");
  RNA_def_property_update(prop, NC_WM | ND_XR_DATA_CHANGED, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Session State
 * \{ */

static void rna_def_xr_session_state(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm, *prop;

  srna = RNA_def_struct(brna, "XrSessionState", NULL);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Session State", "Runtime state information about the VR session");

  func = RNA_def_function(srna, "is_running", "rna_XrSessionState_is_running");
  RNA_def_function_ui_description(func, "Query if the VR session is currently running");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "reset_to_base_pose", "rna_XrSessionState_reset_to_base_pose");
  RNA_def_function_ui_description(func, "Force resetting of position and rotation deltas");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "action_set_create", "rna_XrSessionState_action_set_create");
  RNA_def_function_ui_description(func, "Create a VR action set");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap", "XrActionMap", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "action_create", "rna_XrSessionState_action_create");
  RNA_def_function_ui_description(func, "Create a VR action");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap", "XrActionMap", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap_item", "XrActionMapItem", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(
      srna, "action_binding_create", "rna_XrSessionState_action_binding_create");
  RNA_def_function_ui_description(func, "Create a VR action binding");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap", "XrActionMap", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap_item", "XrActionMapItem", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "actionmap_binding", "XrActionMapBinding", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(
      srna, "active_action_set_set", "rna_XrSessionState_active_action_set_set");
  RNA_def_function_ui_description(func, "Set the active VR action set");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_set", NULL, 64, "Action Set", "Action set name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(
      srna, "controller_pose_actions_set", "rna_XrSessionState_controller_pose_actions_set");
  RNA_def_function_ui_description(func, "Set the actions that determine the VR controller poses");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_set", NULL, 64, "Action Set", "Action set name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func,
                        "grip_action",
                        NULL,
                        64,
                        "Grip Action",
                        "Name of the action representing the controller grips");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func,
                        "aim_action",
                        NULL,
                        64,
                        "Aim Action",
                        "Name of the action representing the controller aims");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "action_state_get", "rna_XrSessionState_action_state_get");
  RNA_def_function_ui_description(func, "Get the current state of a VR action");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_set_name", NULL, 64, "Action Set", "Action set name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_name", NULL, 64, "Action", "Action name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "user_path", NULL, 64, "User Path", "OpenXR user path");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float_array(
      func,
      "state",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Action State",
      "Current state of the VR action. Second float value is only set for 2D vector type actions",
      -FLT_MAX,
      FLT_MAX);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_OUTPUT);

  func = RNA_def_function(srna, "haptic_action_apply", "rna_XrSessionState_haptic_action_apply");
  RNA_def_function_ui_description(func, "Apply a VR haptic action");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_set_name", NULL, 64, "Action Set", "Action set name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_name", NULL, 64, "Action", "Action name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(
      func,
      "user_path",
      NULL,
      64,
      "User Path",
      "Optional OpenXR user path. If not set, the action will be applied to all paths");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "duration",
                       0.0f,
                       0.0f,
                       FLT_MAX,
                       "Duration",
                       "Haptic duration in seconds. 0.0 is the minimum supported duration",
                       0.0f,
                       FLT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "frequency",
                       0.0f,
                       0.0f,
                       FLT_MAX,
                       "Frequency",
                       "Frequency of the haptic vibration in hertz. 0.0 specifies the OpenXR "
                       "runtime's default frequency",
                       0.0f,
                       FLT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "amplitude",
                       1.0f,
                       0.0f,
                       1.0f,
                       "Amplitude",
                       "Haptic amplitude, ranging from 0.0 to 1.0",
                       0.0f,
                       1.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", 0, "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "haptic_action_stop", "rna_XrSessionState_haptic_action_stop");
  RNA_def_function_ui_description(func, "Stop a VR haptic action");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_set_name", NULL, 64, "Action Set", "Action set name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "action_name", NULL, 64, "Action", "Action name");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(
      func,
      "user_path",
      NULL,
      64,
      "User Path",
      "Optional OpenXR user path. If not set, the action will be stopped for all paths");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(
      srna, "controller_grip_location_get", "rna_XrSessionState_controller_grip_location_get");
  RNA_def_function_ui_description(func,
                                  "Get the last known controller grip location in world space");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func, "index", 0, 0, 255, "Index", "Controller index", 0, 255);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_float_translation(func,
                            "location",
                            3,
                            NULL,
                            -FLT_MAX,
                            FLT_MAX,
                            "Location",
                            "Controller grip location",
                            -FLT_MAX,
                            FLT_MAX);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_OUTPUT);

  func = RNA_def_function(
      srna, "controller_grip_rotation_get", "rna_XrSessionState_controller_grip_rotation_get");
  RNA_def_function_ui_description(
      func, "Get the last known controller grip rotation (quaternion) in world space");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func, "index", 0, 0, 255, "Index", "Controller index", 0, 255);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float_vector(func,
                              "rotation",
                              4,
                              NULL,
                              -FLT_MAX,
                              FLT_MAX,
                              "Rotation",
                              "Controller grip quaternion rotation",
                              -FLT_MAX,
                              FLT_MAX);
  parm->subtype = PROP_QUATERNION;
  RNA_def_property_ui_range(parm, -FLT_MAX, FLT_MAX, 1, 5);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_OUTPUT);

  func = RNA_def_function(
      srna, "controller_aim_location_get", "rna_XrSessionState_controller_aim_location_get");
  RNA_def_function_ui_description(func,
                                  "Get the last known controller aim location in world space");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func, "index", 0, 0, 255, "Index", "Controller index", 0, 255);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_float_translation(func,
                            "location",
                            3,
                            NULL,
                            -FLT_MAX,
                            FLT_MAX,
                            "Location",
                            "Controller aim location",
                            -FLT_MAX,
                            FLT_MAX);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_OUTPUT);

  func = RNA_def_function(
      srna, "controller_aim_rotation_get", "rna_XrSessionState_controller_aim_rotation_get");
  RNA_def_function_ui_description(
      func, "Get the last known controller aim rotation (quaternion) in world space");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func, "index", 0, 0, 255, "Index", "Controller index", 0, 255);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float_vector(func,
                              "rotation",
                              4,
                              NULL,
                              -FLT_MAX,
                              FLT_MAX,
                              "Rotation",
                              "Controller aim quaternion rotation",
                              -FLT_MAX,
                              FLT_MAX);
  parm->subtype = PROP_QUATERNION;
  RNA_def_property_ui_range(parm, -FLT_MAX, FLT_MAX, 1, 5);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_OUTPUT);

  prop = RNA_def_property(srna, "viewer_pose_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_XrSessionState_viewer_pose_location_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Viewer Pose Location",
      "Last known location of the viewer pose (center between the eyes) in world space");

  prop = RNA_def_property(srna, "viewer_pose_rotation", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(prop, "rna_XrSessionState_viewer_pose_rotation_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Viewer Pose Rotation",
      "Last known rotation of the viewer pose (center between the eyes) in world space");

  prop = RNA_def_property(srna, "actionmaps", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_XrSessionState_actionmaps_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "XrActionMap");
  RNA_def_property_ui_text(prop, "XR Action Maps", "");
  rna_def_xr_actionmaps(brna, prop);

  prop = RNA_def_property(srna, "active_actionmap", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop,
                             "rna_XrSessionState_active_actionmap_get",
                             "rna_XrSessionState_active_actionmap_set",
                             NULL);
  RNA_def_property_ui_text(prop, "Active Action Map", "");

  prop = RNA_def_property(srna, "selected_actionmap", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop,
                             "rna_XrSessionState_selected_actionmap_get",
                             "rna_XrSessionState_selected_actionmap_set",
                             NULL);
  RNA_def_property_ui_text(prop, "Selected Action Map", "");
}

/** \} */

void RNA_def_xr(BlenderRNA *brna)
{
  RNA_define_animate_sdna(false);

  rna_def_xr_actionmap(brna);
  rna_def_xr_session_settings(brna);
  rna_def_xr_session_state(brna);

  RNA_define_animate_sdna(true);
}

#endif /* RNA_RUNTIME */
