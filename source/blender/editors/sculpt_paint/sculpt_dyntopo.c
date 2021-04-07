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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_undo.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>

void SCULPT_dynamic_topology_triangulate(BMesh *bm)
{
  if (bm->totloop != bm->totface * 3) {
    BM_mesh_triangulate(
        bm, MOD_TRIANGULATE_QUAD_BEAUTY, MOD_TRIANGULATE_NGON_EARCLIP, 4, false, NULL, NULL, NULL);
  }
}

void SCULPT_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = NULL;
  }

  if (ss->pmap) {
    MEM_freeN(ss->pmap);
    ss->pmap = NULL;
  }

  if (ss->pmap_mem) {
    MEM_freeN(ss->pmap_mem);
    ss->pmap_mem = NULL;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

void SCULPT_dyntopo_save_origverts(SculptSession *ss)
{
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

    copy_v3_v3(mv->origco, v->co);
    copy_v3_v3(mv->origno, v->no);

    if (ss->cd_vcol_offset >= 0) {
      MPropCol *mp = (MPropCol *)BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset);
      copy_v4_v4(mv->origcolor, mp->color);
    }
  }
}

static char layer_id[] = "_dyntopo_node_id";

void SCULPT_dyntopo_node_layers_update_offsets(SculptSession *ss)
{
  SCULPT_dyntopo_node_layers_add(ss);
}

bool SCULPT_dyntopo_has_templayer(SculptSession *ss, int type, const char *name)
{
  return CustomData_get_named_layer_index(&ss->bm->vdata, type, name) >= 0;
}

int SCULPT_dyntopo_ensure_templayer(SculptSession *ss, int type, const char *name)
{
  int li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);

  if (li < 0) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, type, name);
    SCULPT_dyntopo_node_layers_update_offsets(ss);
    BKE_pbvh_update_offsets(
        ss->pbvh, ss->cd_vert_node_offset, ss->cd_face_node_offset, ss->cd_dyn_vert);

    li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);
  }

  int cd_off = CustomData_get_n_offset(
      &ss->bm->vdata, type, li - CustomData_get_layer_index(&ss->bm->vdata, type));

  ss->bm->vdata.layers[li].flag |= CD_FLAG_TEMPORARY;

  return cd_off;
}

int SCULPT_dyntopo_get_templayer(SculptSession *ss, int type, const char *name)
{
  if (!SCULPT_dyntopo_has_templayer(ss, type, name)) {
    return -1;
  }

  return SCULPT_dyntopo_ensure_templayer(ss, type, name);
}

void SCULPT_dyntopo_node_layers_add(SculptSession *ss)
{
  int cd_node_layer_index, cd_face_node_layer_index;

  int cd_origco_index, cd_origno_index, cd_origvcol_index = -1;
  bool have_vcol = CustomData_has_layer(&ss->bm->vdata, CD_PROP_COLOR);

  if (!CustomData_has_layer(&ss->bm->vdata, CD_DYNTOPO_VERT)) {
    BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_DYNTOPO_VERT);

    int cd_dyn_vert = CustomData_get_layer_index(&ss->bm->vdata, CD_DYNTOPO_VERT);
    ss->bm->vdata.layers[cd_dyn_vert].flag |= CD_FLAG_TEMPORARY;
  }

  cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT32, layer_id);
  if (cd_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_INT32, layer_id);
  }

  cd_face_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->pdata, CD_PROP_INT32, layer_id);
  if (cd_face_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->pdata, CD_PROP_INT32, layer_id);
  }

  // get indices again, as they might have changed after adding new layers
  cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT32, layer_id);
  cd_face_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->pdata, CD_PROP_INT32, layer_id);

  ss->cd_origvcol_offset = -1;

  ss->cd_dyn_vert = CustomData_get_offset(&ss->bm->vdata, CD_DYNTOPO_VERT);

  ss->cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);

  ss->cd_vert_node_offset = CustomData_get_n_offset(
      &ss->bm->vdata,
      CD_PROP_INT32,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->vdata, CD_PROP_INT32));

  ss->bm->vdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;

  ss->cd_face_node_offset = CustomData_get_n_offset(
      &ss->bm->pdata,
      CD_PROP_INT32,
      cd_face_node_layer_index - CustomData_get_layer_index(&ss->bm->pdata, CD_PROP_INT32));

  ss->bm->pdata.layers[cd_face_node_layer_index].flag |= CD_FLAG_TEMPORARY;
  ss->cd_faceset_offset = CustomData_get_offset(&ss->bm->pdata, CD_SCULPT_FACE_SETS);
}

/**
  Syncs customdata layers with internal bmesh, but ignores deleted layers.
*/
void SCULPT_dynamic_topology_sync_layers(Object *ob, Mesh *me)
{
  SculptSession *ss = ob->sculpt;

  if (!ss || !ss->bm) {
    return;
  }

  bool modified = false;
  BMesh *bm = ss->bm;

  CustomData *cd1[4] = {&me->vdata, &me->edata, &me->ldata, &me->pdata};
  CustomData *cd2[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};
  int types[4] = {BM_VERT, BM_EDGE, BM_LOOP, BM_FACE};
  int badmask = CD_MASK_MLOOP | CD_MASK_MVERT | CD_MASK_MEDGE | CD_MASK_MPOLY | CD_MASK_ORIGINDEX |
                CD_MASK_ORIGSPACE | CD_MASK_MFACE;

  for (int i = 0; i < 4; i++) {
    CustomDataLayer **newlayers = NULL;
    BLI_array_declare(newlayers);

    CustomData *data1 = cd1[i];
    CustomData *data2 = cd2[i];

    if (!data1->layers) {
      modified |= data2->layers != NULL;
      continue;
    }

    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl1 = data1->layers + j;

      if ((1 << cl1->type) & badmask) {
        continue;
      }

      modified = modified || CustomData_get_active_layer_index(data1, cl1->type) !=
                                 CustomData_get_active_layer_index(data2, cl1->type);
      modified = modified || CustomData_get_render_layer_index(data1, cl1->type) !=
                                 CustomData_get_render_layer_index(data2, cl1->type);
      modified = modified || CustomData_get_stencil_layer_index(data1, cl1->type) !=
                                 CustomData_get_stencil_layer_index(data2, cl1->type);
      modified = modified || CustomData_get_clone_layer_index(data1, cl1->type) !=
                                 CustomData_get_clone_layer_index(data2, cl1->type);

      int idx = CustomData_get_named_layer_index(data2, cl1->type, cl1->name);
      if (idx < 0) {
        BLI_array_append(newlayers, cl1);
      }
    }

    for (int j = 0; j < BLI_array_len(newlayers); j++) {
      BM_data_layer_add_named(bm, data2, newlayers[j]->type, newlayers[j]->name);
      modified |= true;
    }

    char typemap[CD_NUMTYPES] = {
        0,
    };

    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl = data1->layers + j;
      CustomDataLayer *cl1 = cl;

      if ((1 << cl1->type) & badmask) {
        continue;
      }

      if (typemap[cl1->type]) {
        continue;
      }

      typemap[cl1->type] = 1;
      cl1 = cl + CustomData_get_active_layer(data1, cl1->type);

      int idx = CustomData_get_named_layer_index(data2, cl1->type, cl1->name);
      CustomData_set_layer_active_index(data2, cl1->type, idx);

      cl1 = cl + CustomData_get_render_layer(data1, cl1->type);
      idx = CustomData_get_named_layer_index(data2, cl1->type, cl1->name);
      CustomData_set_layer_render_index(data2, cl1->type, idx);

      cl1 = cl + CustomData_get_stencil_layer(data1, cl1->type);
      idx = CustomData_get_named_layer_index(data2, cl1->type, cl1->name);
      CustomData_set_layer_stencil_index(data2, cl1->type, idx);

      cl1 = cl + CustomData_get_clone_layer(data1, cl1->type);
      idx = CustomData_get_named_layer_index(data2, cl1->type, cl1->name);
      CustomData_set_layer_clone_index(data2, cl1->type, idx);
    }

    BLI_array_free(newlayers);
  }

  if (modified) {
    SCULPT_dyntopo_node_layers_update_offsets(ss);
    BKE_pbvh_update_offsets(
        ss->pbvh, ss->cd_vert_node_offset, ss->cd_face_node_offset, ss->cd_dyn_vert);
  }
}

void SCULPT_dynamic_topology_enable_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  SCULPT_pbvh_clear(ob);

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove T36280. */
  BKE_mesh_mselect_clear(me);

  /* Create triangles-only BMesh. */
  ss->bm = BM_mesh_create(&allocsize,
                          &((struct BMeshCreateParams){
                              .use_toolflags = false,
                          }));

  BM_mesh_bm_from_me(NULL,
                     ss->bm,
                     me,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                         .use_shapekey = true,
                         .active_shapekey = ob->shapenr,
                     }));
  SCULPT_dynamic_topology_triangulate(ss->bm);
  BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);

  SCULPT_dyntopo_node_layers_add(ss);
  SCULPT_dyntopo_save_origverts(ss);

  BMIter iter;
  BMVert *v;
  int cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);

  int cd_pers_co = -1, cd_pers_no = -1, cd_pers_disp = -1;
  int cd_layer_disp = -1;

  // convert layer brush data
  if (ss->persistent_base) {
    cd_pers_co = SCULPT_dyntopo_ensure_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO);
    cd_pers_no = SCULPT_dyntopo_ensure_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_NO);
    cd_pers_disp = SCULPT_dyntopo_ensure_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP);
    cd_layer_disp = SCULPT_dyntopo_ensure_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_DISP);

    SCULPT_dyntopo_node_layers_update_offsets(ss);
    BKE_pbvh_update_offsets(
        ss->pbvh, ss->cd_vert_node_offset, ss->cd_face_node_offset, ss->cd_dyn_vert);

    cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);
  }
  else {
    cd_layer_disp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP);
  }

  int i = 0;

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

    // persistent base
    if (cd_pers_co >= 0) {
      float(*co)[3] = BM_ELEM_CD_GET_VOID_P(v, cd_pers_co);
      float(*no)[3] = BM_ELEM_CD_GET_VOID_P(v, cd_pers_no);
      float *disp = BM_ELEM_CD_GET_VOID_P(v, cd_pers_disp);

      copy_v3_v3(co, ss->persistent_base[i].co);
      copy_v3_v3(no, ss->persistent_base[i].no);
      *disp = ss->persistent_base[i].disp;
    }

    if (cd_layer_disp >= 0) {
      float *disp = BM_ELEM_CD_GET_VOID_P(v, cd_layer_disp);
      *disp = 0.0f;
    }

    copy_v3_v3(mv->origco, v->co);
    copy_v3_v3(mv->origno, v->no);

    if (ss->cd_vcol_offset >= 0) {
      MPropCol *color = (MPropCol *)BM_ELEM_CD_GET_VOID_P(v, cd_vcol_offset);
      copy_v4_v4(mv->origcolor, color->color);
    }

    i++;
  }

  /* Make sure the data for existing faces are initialized. */
  if (me->totpoly != ss->bm->totface) {
    BM_mesh_normals_update(ss->bm);
  }

  /* Enable dynamic topology. */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  ss->update_boundary_info_bmesh = 1;

  /* Enable logging for undo/redo. */
  ss->bm_log = BM_log_create(
      ss->bm, ss->cd_origco_offset, ss->cd_origno_offset, ss->cd_origvcol_offset, ss->cd_dyn_vert);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

void SCULPT_dyntopo_save_persistent_base(SculptSession *ss) {
  int cd_pers_co = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO);
  int cd_pers_no = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_NO);
  int cd_pers_disp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP);

  if (cd_pers_co >= 0) {
    BMIter iter;

    MEM_SAFE_FREE(ss->persistent_base);
    ss->persistent_base = MEM_callocN(sizeof(*ss->persistent_base) * ss->bm->totvert,
                                      "ss->persistent_base");
    BMVert *v;
    int i = 0;

    BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
      float(*co)[3] = BM_ELEM_CD_GET_VOID_P(v, cd_pers_co);
      float(*no)[3] = BM_ELEM_CD_GET_VOID_P(v, cd_pers_no);
      float *disp = BM_ELEM_CD_GET_VOID_P(v, cd_pers_disp);

      copy_v3_v3(ss->persistent_base[i].co, co);
      copy_v3_v3(ss->persistent_base[i].no, no);
      ss->persistent_base[i].disp = *disp;

      i++;
    }
  }
}
/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from. */
static void SCULPT_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  SCULPT_pbvh_clear(ob);

  if (unode) {
    /* Free all existing custom data. */
    CustomData_free(&me->vdata, me->totvert);
    CustomData_free(&me->edata, me->totedge);
    CustomData_free(&me->fdata, me->totface);
    CustomData_free(&me->ldata, me->totloop);
    CustomData_free(&me->pdata, me->totpoly);

    /* Copy over stored custom data. */
    SculptUndoNodeGeometry *geometry = &unode->geometry_bmesh_enter;
    me->totvert = geometry->totvert;
    me->totloop = geometry->totloop;
    me->totpoly = geometry->totpoly;
    me->totedge = geometry->totedge;
    me->totface = 0;
    CustomData_copy(
        &geometry->vdata, &me->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, geometry->totvert);
    CustomData_copy(
        &geometry->edata, &me->edata, CD_MASK_MESH.emask, CD_DUPLICATE, geometry->totedge);
    CustomData_copy(
        &geometry->ldata, &me->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, geometry->totloop);
    CustomData_copy(
        &geometry->pdata, &me->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, geometry->totpoly);

    BKE_mesh_update_customdata_pointers(me, false);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);

    /* Sync the visibility to vertices manually as the pmap is still not initialized. */
    for (int i = 0; i < me->totvert; i++) {
      me->mvert[i].flag &= ~ME_HIDE;
      me->mvert[i].flag |= ME_VERT_PBVH_UPDATE;
    }
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Typically valid but with global-undo they can be NULL, see: T36234. */
  if (ss->bm) {
    //rebuild ss->persistent_base if necassary
    SCULPT_dyntopo_save_persistent_base(ss);

    BM_mesh_free(ss->bm);
    ss->bm = NULL;
  }
  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = NULL;
  }

  BKE_particlesystem_reset_all(ob);
  BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

void SCULPT_dynamic_topology_disable(bContext *C, SculptUndoNode *unode)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, unode);
}

void sculpt_dynamic_topology_disable_with_undo(Main *bmain,
                                               Depsgraph *depsgraph,
                                               Scene *scene,
                                               Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm != NULL) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != NULL) : true;
    if (use_undo) {
      SCULPT_undo_push_begin(ob, "Dynamic topology disable");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_END);
    }
    SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, NULL);
    if (use_undo) {
      SCULPT_undo_push_end();
    }
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm == NULL) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != NULL) : true;
    if (use_undo) {
      SCULPT_undo_push_begin(ob, "Dynamic topology enable");
    }
    SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    if (use_undo) {
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
      SCULPT_undo_push_end();
    }
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  WM_cursor_wait(true);

  if (ss->bm) {
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

  return OPERATOR_FINISHED;
}

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_VDATA | DYNTOPO_WARN_EDATA | DYNTOPO_WARN_LDATA)) {
    const char *msg_error = TIP_("Vertex Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve vertex colors, UVs, or other customdata");
    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  if (flag & DYNTOPO_WARN_MODIFIER) {
    const char *msg_error = TIP_("Generative Modifiers Detected!");
    const char *msg = TIP_(
        "Keeping the modifiers will increase polycount when returning to object mode");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0, NULL);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob)
{
  Mesh *me = ob->data;
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = 0;

  BLI_assert(ss->bm == NULL);
  UNUSED_VARS_NDEBUG(ss);

#ifndef DYNTOPO_CD_INTERP
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (!ELEM(i, CD_MVERT, CD_MEDGE, CD_MFACE, CD_MLOOP, CD_MPOLY, CD_PAINT_MASK, CD_ORIGINDEX)) {
      if (CustomData_has_layer(&me->vdata, i)) {
        flag |= DYNTOPO_WARN_VDATA;
      }
      if (CustomData_has_layer(&me->edata, i)) {
        flag |= DYNTOPO_WARN_EDATA;
      }
      if (CustomData_has_layer(&me->ldata, i)) {
        flag |= DYNTOPO_WARN_LDATA;
      }
    }
  }
#endif

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mti->type == eModifierTypeType_Constructive) {
        flag |= DYNTOPO_WARN_MODIFIER;
        break;
      }
    }
  }

  return flag;
}

static int sculpt_dynamic_topology_toggle_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    Scene *scene = CTX_data_scene(C);
    enum eDynTopoWarnFlag flag = SCULPT_dynamic_topology_check(scene, ob);

    if (flag) {
      /* The mesh has customdata that will be lost, let the user confirm this is OK. */
      return dyntopo_warning_popup(C, op->type, flag);
    }
  }

  return sculpt_dynamic_topology_toggle_exec(C, op);
}

void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Dynamic Topology Toggle";
  ot->idname = "SCULPT_OT_dynamic_topology_toggle";
  ot->description = "Dynamic topology alters the mesh topology while sculpting";

  /* API callbacks. */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
