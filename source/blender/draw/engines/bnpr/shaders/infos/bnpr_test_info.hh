/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "bnpr_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Tutorial
 * \{ */

/* For details, see "gpu_shader_create_info.hh" */
// GPU_SHADER_CREATE_INFO(bnpr_strokegen_test)
  // .do_static_compilation(true)

  /* -------------------------------------------------------------------- */
  /* Name of other infos to recursively merge with this one.
   * No data slot must overlap otherwise we throw an error. */
  // .additional_info("eevee_shared", "draw_view", "draw_view_culling")

  /* -------------------------------------------------------------------- */
  /* Macros */
  // .define("DOF_BOKEH_TEXTURE", "false")

  /* -------------------------------------------------------------------- */
  /** Resources bindings points
  // .uniform_buf(6, "DepthOfFieldData", "dof_buf")
  // .storage_buf(0, Qualifier::READ_WRITE, "LightCullingData", "light_cull_buf")
  // .storage_buf(1, Qualifier::READ, "LightData", "in_light_buf[]")
  // .storage_buf(2, Qualifier::WRITE, "LightData", "out_light_buf[]")
  // .sampler(0, ImageType::FLOAT_2D, "downsample_tx")
  // .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "inout_color_lod0_img")
  /*          eGPUTextureFormat                     ImageType  */

  /* -------------------------------------------------------------------- */
  /** Comptue shader
  // .local_group_size(CULLING_SELECT_GROUP_SIZE) /* <== from "bnpr_defines.hh" */
  // .compute_source("eevee_light_culling_select_comp.glsl");

  /* -------------------------------------------------------------------- */
  // Vertex & Fragment shader
  // .vertex_in(0, Type::VEC3, "pos")
  // .vertex_in(1, Type::VEC3, "nor")
  // .vertex_source("eevee_geom_mesh_vert.glsl")
  //
  // .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
  // .fragment_out(1, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
  // .fragment_source("eevee_surf_forward_frag.glsl")
  //
  /* In order to use .vertex_out for vs output,
   * we firstly need to define an interface:
// GPU_SHADER_INTERFACE_INFO(interface_info_name, "interp")
  // .smooth(Type::VEC3, "a") /* smooth: conventional interpolation for fragments */
  // .flat(Type::VEC3, "b"); /* flat: no interpolation, instead, use attribute from a "provoking vertex" */
  // .noperspective(Type::VEC3, "c") /* interpolation, without perspective correction */
  /* Then use the interface to declare a .vertex_out: */
  // .vertex_out(interface_info_name)

/** \} */



/* -------------------------------------------------------------------- */
/** \test
 * \{ */
GPU_SHADER_CREATE_INFO(bnpr_strokegen_test_xxx)
  .do_static_compilation(true)
  .storage_buf(0, Qualifier::READ_WRITE, "uint", "buf_test")
  .local_group_size(GROUP_SIZE_STROKEGEN_TEST) /* <== from "bnpr_defines.hh" */
  .compute_source("bnpr_strokegen_test_comp.glsl");

/** \} */
