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

  void StrokeGenPassModule::sync()
  {
    rebuild_pass_scan_test();
    rebuild_pass_segscan_test();
  }

  void StrokeGenPassModule::rebuild_pass_extract_mesh_geom(Object* ob, GPUBatch* gpu_batch)
  {
  }

  void StrokeGenPassModule::rebuild_pass_scan_test()
  {
    pass_scan_test.init();
    { // upsweep for tree-scan
      auto& sub = pass_scan_test.sub("strokegen_scan_test_upsweep");
      sub.shader_set(shaders_.static_shader_get(eShaderType::SCAN_TEST_UPSWEEP));

      // Note: keep the same slot binding as in shader_create_info
      sub.bind_ssbo(0, buffers_.ssbo_bnpr_in_scan_data_);
      sub.bind_ssbo(1, buffers_.ssbo_bnpr_out_scan_data_);
      sub.bind_ssbo(2, buffers_.ssbo_bnpr_scan_block_sum_);
      sub.bind_ubo(0, buffers_.ubo_bnpr_tree_scan_infos_);

      sub.dispatch(int3(buffers_.ubo_bnpr_tree_scan_infos_.num_thread_groups, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
    { // reduction for tree-scan
      auto& sub = pass_scan_test.sub("strokegen_scan_test_aggregate");
      sub.shader_set(shaders_.static_shader_get(eShaderType::SCAN_TEST_AGGREGATE));

      // Note: keep the same slot binding as in shader_create_info
      sub.bind_ssbo(0, buffers_.ssbo_bnpr_scan_block_sum_);

      sub.dispatch(int3(1, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
    { // down sweep for tree-scan
      auto& sub = pass_scan_test.sub("strokegen_scan_test_dwsweep");
      sub.shader_set(shaders_.static_shader_get(eShaderType::SCAN_TEST_DWSWEEP));

      // Note: keep the same slot binding as in shader_create_info
      sub.bind_ssbo(0, buffers_.ssbo_bnpr_out_scan_data_);
      sub.bind_ssbo(1, buffers_.ssbo_bnpr_scan_block_sum_);
      sub.bind_ubo(0, buffers_.ubo_bnpr_tree_scan_infos_);

      sub.dispatch(int3(buffers_.ubo_bnpr_tree_scan_infos_.num_thread_groups, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
  }

  void StrokeGenPassModule::rebuild_pass_segscan_test()
  {
    pass_segscan_test.init();
    { // upsweep for tree-scan
      auto& sub = pass_segscan_test.sub("strokegen_segscan_test_upsweep");
      sub.shader_set(shaders_.static_shader_get(SEGSCAN_TEST_UPSWEEP));

      // Note: keep the same slot binding as in shader_create_info
      sub.bind_ssbo(0, buffers_.ssbo_bnpr_in_scan_data_);
      sub.bind_ssbo(1, buffers_.ssbo_bnpr_out_scan_data_);
      sub.bind_ssbo(2, buffers_.ssbo_bnpr_scan_block_sum_);
      sub.bind_ubo(0, buffers_.ubo_bnpr_tree_scan_infos_);

      sub.dispatch(int3(buffers_.ubo_bnpr_tree_scan_infos_.num_thread_groups, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
    { // reduction for tree-scan
      auto& sub = pass_segscan_test.sub("strokegen_segscan_test_aggregate");
      sub.shader_set(shaders_.static_shader_get(SEGSCAN_TEST_AGGREGATE));

      // Note: keep the same slot binding as in shader_create_info
      sub.bind_ssbo(0, buffers_.ssbo_bnpr_scan_block_sum_);

      sub.dispatch(int3(1, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
    { // down sweep for tree-scan
      auto& sub = pass_segscan_test.sub("strokegen_segscan_test_dwsweep");
      sub.shader_set(shaders_.static_shader_get(SEGSCAN_TEST_DWSWEEP));

      // Note: keep the same slot binding as in shader_create_info
      sub.bind_ssbo(0, buffers_.ssbo_bnpr_out_scan_data_);
      sub.bind_ssbo(1, buffers_.ssbo_bnpr_scan_block_sum_);
      sub.bind_ubo(0, buffers_.ubo_bnpr_tree_scan_infos_);

      sub.dispatch(int3(buffers_.ubo_bnpr_tree_scan_infos_.num_thread_groups, 1, 1));
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
    }
  }
}
