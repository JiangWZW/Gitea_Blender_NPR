/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BKE_node_runtime.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_fn_scale_matrix3x3_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Matrix3x3>(N_("Matrix"));
  b.add_input<decl::Vector>(N_("Scale"));
  b.add_output<decl::Matrix3x3>(N_("Matrix"));
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SI_SO<float3x3, float3, float3x3> scale_matrix_fn{
      "scale_matrix", [](const float3x3 &mat, const float3 &scale) {
        float3x3 result;
        mul_v3_v3fl(result.values[0], mat[0], scale[0]);
        mul_v3_v3fl(result.values[1], mat[1], scale[1]);
        mul_v3_v3fl(result.values[2], mat[2], scale[2]);
        return result;
      }};
  builder.set_matching_fn(&scale_matrix_fn);
}

}  // namespace blender::nodes::node_fn_scale_matrix3x3_cc

void register_node_type_fn_scale_matrix_3x3(void)
{
  namespace file_ns = blender::nodes::node_fn_scale_matrix3x3_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SCALE_MATRIX_3X3, "Scale 3x3 Matrix", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  nodeRegisterType(&ntype);
}
