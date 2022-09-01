/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"
#include "NOD_multi_function.hh"
#include "NOD_node_declaration.hh"

#include "BLI_map.hh"

#include "DNA_ID.h"

#include "BKE_geometry_set.hh"
#include "BKE_type_conversions.hh"

#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_graph_executor.hh"

namespace blender::nodes {

using fn::ValueOrField;
using fn::ValueOrFieldCPPType;
using namespace fn::multi_function_types;

static const CPPType *get_socket_cpp_type(const bNodeSocketType &typeinfo)
{
  const CPPType *type = typeinfo.geometry_nodes_cpp_type;
  if (type == nullptr) {
    return nullptr;
  }
  /* The evaluator only supports types that have special member functions. */
  if (!type->has_special_member_functions()) {
    return nullptr;
  }
  return type;
}

static const CPPType *get_socket_cpp_type(const bNodeSocket &socket)
{
  return get_socket_cpp_type(*socket.typeinfo);
}

static const CPPType *get_vector_type(const CPPType &type)
{
  if (type.is<GeometrySet>()) {
    return &CPPType::get<Vector<GeometrySet>>();
  }
  if (type.is<ValueOrField<std::string>>()) {
    return &CPPType::get<Vector<ValueOrField<std::string>>>();
  }
  return nullptr;
}

static void lazy_function_interface_from_node(const bNode &node,
                                              Vector<const bNodeSocket *> &r_used_inputs,
                                              Vector<const bNodeSocket *> &r_used_outputs,
                                              Vector<lf::Input> &r_inputs,
                                              Vector<lf::Output> &r_outputs)
{
  const bool is_muted = node.is_muted();
  const bool supports_lazyness = node.typeinfo->geometry_node_execute_supports_laziness ||
                                 node.is_group();
  const lf::ValueUsage input_usage = supports_lazyness ? lf::ValueUsage::Maybe :
                                                         lf::ValueUsage::Used;
  for (const bNodeSocket *socket : node.input_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    if (socket->is_multi_input() && !is_muted) {
      type = get_vector_type(*type);
    }
    /* TODO: Name may not be static. */
    r_inputs.append({socket->identifier, *type, input_usage});
    r_used_inputs.append(socket);
  }
  for (const bNodeSocket *socket : node.output_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    r_outputs.append({socket->identifier, *type});
    r_used_outputs.append(socket);
  }
}

/**
 * Used for most normal geometry nodes like Subdivision Surface and Set Position.
 */
class LazyFunctionForGeometryNode : public LazyFunction {
 private:
  const bNode &node_;

 public:
  LazyFunctionForGeometryNode(const bNode &node,
                              Vector<const bNodeSocket *> &r_used_inputs,
                              Vector<const bNodeSocket *> &r_used_outputs)
      : node_(node)
  {
    static_name_ = node.name;
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodeExecParams geo_params{node_, params, context};
    BLI_assert(node_.typeinfo->geometry_node_execute != nullptr);

    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);

    geo_eval_log::TimePoint start_time = geo_eval_log::Clock::now();
    node_.typeinfo->geometry_node_execute(geo_params);
    geo_eval_log::TimePoint end_time = geo_eval_log::Clock::now();

    geo_eval_log::GeoTreeLogger *tree_logger =
        &user_data->modifier_data->eval_log->get_local_tree_logger(*user_data->context_stack);
    if (tree_logger != nullptr) {
      tree_logger->node_execution_times.append_as(node_.name, start_time, end_time);
    }
  }
};

/**
 * Used to gather all inputs of a multi-input socket.
 */
class LazyFunctionForMultiInput : public LazyFunction {
 public:
  LazyFunctionForMultiInput(const bNodeSocket &socket)
  {
    static_name_ = "Multi Input";
    const CPPType *type = get_socket_cpp_type(socket);
    BLI_assert(type != nullptr);
    BLI_assert(socket.is_multi_input());
    for ([[maybe_unused]] const int i : socket.directly_linked_links().index_range()) {
      inputs_.append({"Input", *type});
    }
    const CPPType *vector_type = get_vector_type(*type);
    BLI_assert(vector_type != nullptr);
    outputs_.append({"Output", *vector_type});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    const CPPType &base_type = *inputs_[0].type;
    base_type.to_static_type_tag<GeometrySet, ValueOrField<std::string>>([&](auto type_tag) {
      using T = typename decltype(type_tag)::type;
      if constexpr (std::is_void_v<T>) {
        /* This type is not support in this node for now. */
        BLI_assert_unreachable();
      }
      else {
        void *output_ptr = params.get_output_data_ptr(0);
        Vector<T> &values = *new (output_ptr) Vector<T>();
        for (const int i : inputs_.index_range()) {
          values.append(params.get_input<T>(i));
        }
        params.output_set(0);
      }
    });
  }
};

/**
 * Simple lazy-function that just forwards the input.
 */
class LazyFunctionForRerouteNode : public LazyFunction {
 public:
  LazyFunctionForRerouteNode(const CPPType &type)
  {
    static_name_ = "Reroute";
    inputs_.append({"Input", type});
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    void *input_value = params.try_get_input_data_ptr(0);
    void *output_value = params.get_output_data_ptr(0);
    BLI_assert(input_value != nullptr);
    BLI_assert(output_value != nullptr);
    const CPPType &type = *inputs_[0].type;
    type.move_construct(input_value, output_value);
    params.output_set(0);
  }
};

static void execute_multi_function_on_value_or_field(
    const MultiFunction &fn,
    const std::shared_ptr<MultiFunction> &owned_fn,
    const Span<const ValueOrFieldCPPType *> input_types,
    const Span<const ValueOrFieldCPPType *> output_types,
    const Span<const void *> input_values,
    const Span<void *> output_values)
{
  BLI_assert(fn.param_amount() == input_types.size() + output_types.size());
  BLI_assert(input_types.size() == input_values.size());
  BLI_assert(output_types.size() == output_values.size());

  bool any_input_is_field = false;
  for (const int i : input_types.index_range()) {
    const ValueOrFieldCPPType &type = *input_types[i];
    const void *value_or_field = input_values[i];
    if (type.is_field(value_or_field)) {
      any_input_is_field = true;
      break;
    }
  }

  if (any_input_is_field) {
    Vector<GField> input_fields;
    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const void *value_or_field = input_values[i];
      input_fields.append(type.as_field(value_or_field));
    }

    std::shared_ptr<fn::FieldOperation> operation;
    if (owned_fn) {
      operation = std::make_shared<fn::FieldOperation>(owned_fn, std::move(input_fields));
    }
    else {
      operation = std::make_shared<fn::FieldOperation>(fn, std::move(input_fields));
    }

    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      void *value_or_field = output_values[i];
      type.construct_from_field(value_or_field, GField{operation, i});
    }
  }
  else {
    MFParamsBuilder params{fn, 1};
    MFContextBuilder context;

    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const CPPType &base_type = type.base_type();
      const void *value_or_field = input_values[i];
      const void *value = type.get_value_ptr(value_or_field);
      params.add_readonly_single_input(GVArray::ForSingleRef(base_type, 1, value));
    }
    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      const CPPType &base_type = type.base_type();
      void *value_or_field = output_values[i];
      type.default_construct(value_or_field);
      void *value = type.get_value_ptr(value_or_field);
      base_type.destruct(value);
      params.add_uninitialized_single_output(GMutableSpan{base_type, value, 1});
    }
    fn.call(IndexRange(1), params, context);
  }
}

class LazyFunctionForMutedNode : public LazyFunction {
 private:
  Array<int> input_by_output_index_;

 public:
  LazyFunctionForMutedNode(const bNode &node,
                           Vector<const bNodeSocket *> &r_used_inputs,
                           Vector<const bNodeSocket *> &r_used_outputs)
  {
    static_name_ = "Muted";
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Maybe;
    }

    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Unused;
    }

    input_by_output_index_.reinitialize(outputs_.size());
    input_by_output_index_.fill(-1);
    for (const bNodeLink *internal_link : node.internal_links_span()) {
      const int input_i = r_used_inputs.first_index_of_try(internal_link->fromsock);
      const int output_i = r_used_outputs.first_index_of_try(internal_link->tosock);
      if (ELEM(-1, input_i, output_i)) {
        continue;
      }
      input_by_output_index_[output_i] = input_i;
      inputs_[input_i].usage = lf::ValueUsage::Maybe;
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    for (const int output_i : outputs_.index_range()) {
      if (params.output_was_set(output_i)) {
        continue;
      }
      const CPPType &output_type = *outputs_[output_i].type;
      void *output_value = params.get_output_data_ptr(output_i);
      const int input_i = input_by_output_index_[output_i];
      if (input_i == -1) {
        output_type.value_initialize(output_value);
        params.output_set(output_i);
        continue;
      }
      const void *input_value = params.try_get_input_data_ptr_or_request(input_i);
      if (input_value == nullptr) {
        continue;
      }
      const CPPType &input_type = *inputs_[input_i].type;
      if (input_type == output_type) {
        input_type.copy_construct(input_value, output_value);
        params.output_set(output_i);
        continue;
      }
      const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
      const auto *from_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&input_type);
      const auto *to_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&output_type);
      if (from_field_type != nullptr && to_field_type != nullptr) {
        const CPPType &from_base_type = from_field_type->base_type();
        const CPPType &to_base_type = to_field_type->base_type();
        if (conversions.is_convertible(from_base_type, to_base_type)) {
          const MultiFunction &multi_fn = *conversions.get_conversion_multi_function(
              MFDataType::ForSingle(from_base_type), MFDataType::ForSingle(to_base_type));
          execute_multi_function_on_value_or_field(
              multi_fn, {}, {from_field_type}, {to_field_type}, {input_value}, {output_value});
        }
        params.output_set(output_i);
        continue;
      }
      output_type.value_initialize(output_value);
      params.output_set(output_i);
    }
  }
};

class LazyFunctionForMultiFunctionConversion : public LazyFunction {
 private:
  const MultiFunction &fn_;
  const ValueOrFieldCPPType &from_type_;
  const ValueOrFieldCPPType &to_type_;

 public:
  LazyFunctionForMultiFunctionConversion(const MultiFunction &fn,
                                         const ValueOrFieldCPPType &from,
                                         const ValueOrFieldCPPType &to)
      : fn_(fn), from_type_(from), to_type_(to)
  {
    static_name_ = "Convert";
    inputs_.append({"From", from});
    outputs_.append({"To", to});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    const void *from_value = params.try_get_input_data_ptr(0);
    void *to_value = params.get_output_data_ptr(0);
    BLI_assert(from_value != nullptr);
    BLI_assert(to_value != nullptr);

    execute_multi_function_on_value_or_field(
        fn_, {}, {&from_type_}, {&to_type_}, {from_value}, {to_value});

    params.output_set(0);
  }
};

class LazyFunctionForMultiFunctionNode : public LazyFunction {
 private:
  const bNode &node_;
  const NodeMultiFunctions::Item fn_item_;
  Vector<const ValueOrFieldCPPType *> input_types_;
  Vector<const ValueOrFieldCPPType *> output_types_;
  Vector<const bNodeSocket *> output_sockets_;

 public:
  LazyFunctionForMultiFunctionNode(const bNode &node,
                                   NodeMultiFunctions::Item fn_item,
                                   Vector<const bNodeSocket *> &r_used_inputs,
                                   Vector<const bNodeSocket *> &r_used_outputs)
      : node_(node), fn_item_(std::move(fn_item))
  {
    BLI_assert(fn_item_.fn != nullptr);
    static_name_ = node.name;
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (const lf::Input &fn_input : inputs_) {
      input_types_.append(dynamic_cast<const ValueOrFieldCPPType *>(fn_input.type));
    }
    for (const lf::Output &fn_output : outputs_) {
      output_types_.append(dynamic_cast<const ValueOrFieldCPPType *>(fn_output.type));
    }
    output_sockets_ = r_used_outputs;
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);
    const ContextStack *context_stack = user_data->context_stack;
    BLI_assert(context_stack != nullptr);
    geo_eval_log::GeoTreeLogger &logger =
        user_data->modifier_data->eval_log->get_local_tree_logger(*context_stack);

    Vector<const void *> input_values(inputs_.size());
    Vector<void *> output_values(outputs_.size());
    for (const int i : inputs_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr(i);
    }
    for (const int i : outputs_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    execute_multi_function_on_value_or_field(
        *fn_item_.fn, fn_item_.owned_fn, input_types_, output_types_, input_values, output_values);
    for (const int i : outputs_.index_range()) {
      const CPPType &type = *this->outputs_[i].type;
      logger.log_value(node_, *output_sockets_[i], {type, output_values[i]});
      params.output_set(i);
    }
  }
};

class LazyFunctionForComplexInput : public LazyFunction {
 private:
  std::function<void(void *)> init_fn_;

 public:
  LazyFunctionForComplexInput(const CPPType &type, std::function<void(void *)> init_fn)
      : init_fn_(std::move(init_fn))
  {
    static_name_ = "Input";
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    void *value = params.get_output_data_ptr(0);
    init_fn_(value);
    params.output_set(0);
  }
};

class LazyFunctionForGroupNode : public LazyFunction {
 private:
  const bNode &group_node_;
  GeometryNodesLazyFunctionResources resources_;
  LazyFunctionGraph graph_;
  std::optional<lf::LazyFunctionGraphExecutor> graph_executor_;

 public:
  LazyFunctionForGroupNode(const bNode &group_node,
                           Vector<const bNodeSocket *> &r_used_inputs,
                           Vector<const bNodeSocket *> &r_used_outputs)
      : group_node_(group_node)
  {
    /* Todo: No static name. */
    static_name_ = group_node.name;
    lazy_function_interface_from_node(
        group_node, r_used_inputs, r_used_outputs, inputs_, outputs_);

    GeometryNodeLazyFunctionMapping mapping;

    bNodeTree *btree = reinterpret_cast<bNodeTree *>(group_node_.id);
    BLI_assert(btree != nullptr); /* Todo. */
    btree->ensure_topology_cache();
    geometry_nodes_to_lazy_function_graph(*btree, graph_, resources_, mapping);
    graph_.update_node_indices();

    Vector<const lf::OutputSocket *> graph_inputs;
    for (const lf::OutputSocket *socket : mapping.group_input_sockets) {
      if (socket != nullptr) {
        graph_inputs.append(socket);
      }
    }
    Vector<const lf::InputSocket *> graph_outputs;
    for (const bNode *node : btree->nodes_by_type("NodeGroupOutput")) {
      for (const bNodeSocket *bsocket : node->input_sockets()) {
        const lf::Socket *socket = mapping.dummy_socket_map.lookup_default(bsocket, nullptr);
        if (socket != nullptr) {
          graph_outputs.append(&socket->as_input());
        }
      }
      break;
    }
    graph_executor_.emplace(graph_, std::move(graph_inputs), std::move(graph_outputs));
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);
    NodeGroupContextStack context_stack{
        user_data->context_stack, group_node_.name, group_node_.id->name + 2};
    GeoNodesLFUserData group_user_data = *user_data;
    group_user_data.context_stack = &context_stack;

    lf::Context group_context = context;
    group_context.user_data = &group_user_data;

    graph_executor_->execute(params, group_context);
  }

  void *init_storage(LinearAllocator<> &allocator) const
  {
    return graph_executor_->init_storage(allocator);
  }

  void destruct_storage(void *storage) const
  {
    graph_executor_->destruct_storage(storage);
  }
};

static lf::OutputSocket *insert_type_conversion(LazyFunctionGraph &graph,
                                                lf::OutputSocket &from_socket,
                                                const CPPType &to_type,
                                                const bke::DataTypeConversions &conversions,
                                                GeometryNodesLazyFunctionResources &resources)
{
  const CPPType &from_type = from_socket.type();
  if (from_type == to_type) {
    return &from_socket;
  }
  const auto *from_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&from_type);
  const auto *to_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&to_type);
  if (from_field_type != nullptr && to_field_type != nullptr) {
    const CPPType &from_base_type = from_field_type->base_type();
    const CPPType &to_base_type = to_field_type->base_type();
    if (conversions.is_convertible(from_base_type, to_base_type)) {
      const MultiFunction &multi_fn = *conversions.get_conversion_multi_function(
          MFDataType::ForSingle(from_base_type), MFDataType::ForSingle(to_base_type));
      auto fn = std::make_unique<LazyFunctionForMultiFunctionConversion>(
          multi_fn, *from_field_type, *to_field_type);
      lf::Node &conversion_node = graph.add_function(*fn);
      resources.functions.append(std::move(fn));
      graph.add_link(from_socket, conversion_node.input(0));
      return &conversion_node.output(0);
    }
  }
  return nullptr;
}

static GMutablePointer get_socket_default_value(LinearAllocator<> &allocator,
                                                const bNodeSocket &bsocket)
{
  const bNodeSocketType &typeinfo = *bsocket.typeinfo;
  const CPPType *type = get_socket_cpp_type(typeinfo);
  if (type == nullptr) {
    return {};
  }
  void *buffer = allocator.allocate(type->size(), type->alignment());
  typeinfo.get_geometry_nodes_cpp_value(bsocket, buffer);
  return {type, buffer};
}

static void prepare_socket_default_value(lf::InputSocket &socket,
                                         const bNodeSocket &bsocket,
                                         GeometryNodesLazyFunctionResources &resources)
{
  GMutablePointer value = get_socket_default_value(resources.allocator, bsocket);
  if (value.get() == nullptr) {
    return;
  }
  socket.set_default_value(value.get());
  if (!value.type()->is_trivially_destructible()) {
    resources.values_to_destruct.append(value);
  }
}

static void create_init_func_if_necessary(lf::InputSocket &socket,
                                          const bNodeSocket &bsocket,
                                          LazyFunctionGraph &graph,
                                          GeometryNodesLazyFunctionResources &resources)
{
  const bNode &bnode = bsocket.owner_node();
  const nodes::NodeDeclaration *node_declaration = bnode.declaration();
  if (node_declaration == nullptr) {
    return;
  }
  const nodes::SocketDeclaration &socket_declaration =
      *node_declaration->inputs()[bsocket.index()];
  const CPPType &type = socket.type();
  std::function<void(void *)> init_fn;
  if (socket_declaration.input_field_type() == nodes::InputSocketFieldType::Implicit) {
    const bNodeSocketType &socktype = *bsocket.typeinfo;
    if (socktype.type == SOCK_VECTOR) {
      if (bnode.type == GEO_NODE_SET_CURVE_HANDLES) {
        StringRef side = ((NodeGeometrySetCurveHandlePositions *)bnode.storage)->mode ==
                                 GEO_NODE_CURVE_HANDLE_LEFT ?
                             "handle_left" :
                             "handle_right";
        init_fn = [side](void *r_value) {
          new (r_value) ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>(side));
        };
      }
      else if (bnode.type == GEO_NODE_EXTRUDE_MESH) {
        init_fn = [](void *r_value) {
          new (r_value)
              ValueOrField<float3>(Field<float3>(std::make_shared<bke::NormalFieldInput>()));
        };
      }
      else {
        init_fn = [](void *r_value) {
          new (r_value) ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>("position"));
        };
      }
    }
    else if (socktype.type == SOCK_INT) {
      if (ELEM(bnode.type, FN_NODE_RANDOM_VALUE, GEO_NODE_INSTANCE_ON_POINTS)) {
        init_fn = [](void *r_value) {
          new (r_value)
              ValueOrField<int>(Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
        };
      }
      else {
        init_fn = [](void *r_value) {
          new (r_value) ValueOrField<int>(Field<int>(std::make_shared<fn::IndexFieldInput>()));
        };
      }
    }
  }
  if (!init_fn) {
    return;
  }
  auto fn = std::make_unique<LazyFunctionForComplexInput>(type, init_fn);
  lf::Node &node = graph.add_function(*fn);
  resources.functions.append(std::move(fn));
  graph.add_link(node.output(0), socket);
}

void geometry_nodes_to_lazy_function_graph(const bNodeTree &btree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources,
                                           GeometryNodeLazyFunctionMapping &mapping)
{
  MultiValueMap<const bNodeSocket *, lf::InputSocket *> input_socket_map;
  Map<const bNodeSocket *, lf::OutputSocket *> output_socket_map;
  Map<const bNodeSocket *, lf::Node *> multi_input_socket_nodes;

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();

  resources.node_multi_functions.append(std::make_unique<NodeMultiFunctions>(btree));
  const NodeMultiFunctions &node_multi_functions = *resources.node_multi_functions.last();

  Vector<const CPPType *> group_input_types;
  Vector<int> group_input_indices;
  LISTBASE_FOREACH (const bNodeSocket *, socket, &btree.inputs) {
    const CPPType *type = get_socket_cpp_type(*socket->typeinfo);
    if (type != nullptr) {
      const int index = group_input_types.append_and_get_index(type);
      group_input_indices.append(index);
    }
    else {
      group_input_indices.append(-1);
    }
  }
  lf::DummyNode &group_input_node = graph.add_dummy({}, group_input_types);
  for (const int i : group_input_indices.index_range()) {
    const int index = group_input_indices[i];
    if (index == -1) {
      mapping.group_input_sockets.append(nullptr);
    }
    else {
      mapping.group_input_sockets.append(&group_input_node.output(index));
    }
  }

  for (const bNode *bnode : btree.all_nodes()) {
    const bNodeType *node_type = bnode->typeinfo;
    if (node_type == nullptr) {
      continue;
    }
    if (bnode->is_muted()) {
      Vector<const bNodeSocket *> used_inputs;
      Vector<const bNodeSocket *> used_outputs;
      auto fn = std::make_unique<LazyFunctionForMutedNode>(*bnode, used_inputs, used_outputs);
      lf::Node &node = graph.add_function(*fn);
      resources.functions.append(std::move(fn));
      for (const int i : used_inputs.index_range()) {
        const bNodeSocket &bsocket = *used_inputs[i];
        input_socket_map.add(&bsocket, &node.input(i));
        prepare_socket_default_value(node.input(i), bsocket, resources);
      }
      for (const int i : used_outputs.index_range()) {
        const bNodeSocket &bsocket = *used_outputs[i];
        output_socket_map.add_new(&bsocket, &node.output(i));
      }
      continue;
    }
    switch (node_type->type) {
      case NODE_FRAME: {
        /* Ignored. */
        break;
      }
      case NODE_REROUTE: {
        const CPPType *type = get_socket_cpp_type(bnode->input_socket(0));
        if (type != nullptr) {
          auto fn = std::make_unique<LazyFunctionForRerouteNode>(*type);
          lf::Node &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));
          input_socket_map.add(&bnode->input_socket(0), &node.input(0));
          output_socket_map.add_new(&bnode->output_socket(0), &node.output(0));
          prepare_socket_default_value(node.input(0), bnode->input_socket(0), resources);
        }
        break;
      }
      case NODE_GROUP_INPUT: {
        for (const int i : group_input_indices.index_range()) {
          const int index = group_input_indices[i];
          if (index != -1) {
            const bNodeSocket &bsocket = bnode->output_socket(i);
            lf::OutputSocket &socket = group_input_node.output(i);
            output_socket_map.add_new(&bsocket, &socket);
            mapping.dummy_socket_map.add_new(&bsocket, &socket);
          }
        }
        break;
      }
      case NODE_GROUP_OUTPUT: {
        Vector<const CPPType *> types;
        Vector<int> indices;
        LISTBASE_FOREACH (const bNodeSocket *, socket, &btree.outputs) {
          const CPPType *type = get_socket_cpp_type(*socket->typeinfo);
          if (type != nullptr) {
            const int index = types.append_and_get_index(type);
            indices.append(index);
          }
          else {
            indices.append(-1);
          }
        }
        lf::DummyNode &group_output_node = graph.add_dummy(types, {});
        for (const int i : indices.index_range()) {
          const int index = indices[i];
          if (index != -1) {
            const bNodeSocket &bsocket = bnode->input_socket(i);
            lf::InputSocket &socket = group_output_node.input(i);
            input_socket_map.add(&bsocket, &socket);
            mapping.dummy_socket_map.add(&bsocket, &socket);
            prepare_socket_default_value(socket, bsocket, resources);
          }
        }
        break;
      }
      case NODE_GROUP: {
        const bool inline_group = false;
        if (inline_group) {
          GeometryNodeLazyFunctionMapping group_mapping;
          bNodeTree *group_btree = reinterpret_cast<bNodeTree *>(bnode->id);
          geometry_nodes_to_lazy_function_graph(*group_btree, graph, resources, group_mapping);
          const Span<const bNode *> group_output_bnodes = group_btree->nodes_by_type(
              "NodeGroupOutput");
          if (group_output_bnodes.size() == 1) {
            const bNode &group_output_bnode = *group_output_bnodes[0];
            for (const int i : group_output_bnode.input_sockets().index_range().drop_back(1)) {
              const bNodeSocket &group_output_bsocket = group_output_bnode.input_socket(i);
              const bNodeSocket &outside_group_output_bsocket = bnode->output_socket(i);
              lf::InputSocket &group_output_socket =
                  group_mapping.dummy_socket_map.lookup(&group_output_bsocket)->as_input();
              const CPPType &type = group_output_socket.type();
              lf::OutputSocket *group_output_origin = group_output_socket.origin();
              if (group_output_origin == nullptr) {
                auto fn = std::make_unique<LazyFunctionForRerouteNode>(type);
                lf::Node &node = graph.add_function(*fn);
                resources.functions.append(std::move(fn));
                output_socket_map.add(&outside_group_output_bsocket, &node.output(0));
                prepare_socket_default_value(node.input(0), group_output_bsocket, resources);
              }
              else {
                graph.remove_link(*group_output_origin, group_output_socket);
                if (group_output_origin->node().is_dummy()) {
                  const int input_index = group_mapping.group_input_sockets.first_index_of(
                      group_output_origin);
                  auto fn = std::make_unique<LazyFunctionForRerouteNode>(type);
                  lf::Node &node = graph.add_function(*fn);
                  resources.functions.append(std::move(fn));
                  output_socket_map.add(&outside_group_output_bsocket, &node.output(0));
                  prepare_socket_default_value(
                      node.input(0), bnode->input_socket(input_index), resources);
                }
                else {
                  output_socket_map.add(&outside_group_output_bsocket, group_output_origin);
                }
              }
            }
          }
          else {
            /* TODO */
          }
          for (const int i : group_mapping.group_input_sockets.index_range()) {
            const bNodeSocket &outside_group_input_bsocket = bnode->input_socket(i);
            lf::OutputSocket &group_input_socket = *group_mapping.group_input_sockets[i];
            const Array<lf::InputSocket *> group_input_targets = group_input_socket.targets();
            for (lf::InputSocket *group_input_target : group_input_targets) {
              graph.remove_link(group_input_socket, *group_input_target);
              input_socket_map.add(&outside_group_input_bsocket, group_input_target);
              prepare_socket_default_value(
                  *group_input_target, outside_group_input_bsocket, resources);
            }
          }
        }
        else {
          Vector<const bNodeSocket *> used_inputs;
          Vector<const bNodeSocket *> used_outputs;
          auto fn = std::make_unique<LazyFunctionForGroupNode>(*bnode, used_inputs, used_outputs);
          lf::Node &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));
          for (const int i : used_inputs.index_range()) {
            const bNodeSocket &bsocket = *used_inputs[i];
            BLI_assert(!bsocket.is_multi_input());
            input_socket_map.add(&bsocket, &node.input(i));
            prepare_socket_default_value(node.input(i), bsocket, resources);
          }
          for (const int i : used_outputs.index_range()) {
            const bNodeSocket &bsocket = *used_outputs[i];
            output_socket_map.add_new(&bsocket, &node.output(i));
          }
        }
        break;
      }
      default: {
        if (node_type->geometry_node_execute) {
          Vector<const bNodeSocket *> used_inputs;
          Vector<const bNodeSocket *> used_outputs;
          auto fn = std::make_unique<LazyFunctionForGeometryNode>(
              *bnode, used_inputs, used_outputs);
          lf::Node &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));

          for (const int i : used_inputs.index_range()) {
            const bNodeSocket &bsocket = *used_inputs[i];
            lf::InputSocket &socket = node.input(i);

            if (bsocket.is_multi_input()) {
              auto fn = std::make_unique<LazyFunctionForMultiInput>(bsocket);
              lf::Node &multi_input_node = graph.add_function(*fn);
              resources.functions.append(std::move(fn));
              graph.add_link(multi_input_node.output(0), socket);
              multi_input_socket_nodes.add(&bsocket, &multi_input_node);
              for (lf::InputSocket *multi_input : multi_input_node.inputs()) {
                prepare_socket_default_value(*multi_input, bsocket, resources);
              }
            }
            else {
              input_socket_map.add(&bsocket, &socket);
              prepare_socket_default_value(socket, bsocket, resources);
              const Span<const bNodeLink *> links = bsocket.directly_linked_links();
              if (links.is_empty() || (links.size() == 1 && links[0]->is_muted())) {
                create_init_func_if_necessary(socket, bsocket, graph, resources);
              }
            }
          }
          for (const int i : used_outputs.index_range()) {
            output_socket_map.add_new(used_outputs[i], &node.output(i));
          }
          break;
        }
        const NodeMultiFunctions::Item &fn_item = node_multi_functions.try_get(*bnode);
        if (fn_item.fn != nullptr) {
          Vector<const bNodeSocket *> used_inputs;
          Vector<const bNodeSocket *> used_outputs;
          auto fn = std::make_unique<LazyFunctionForMultiFunctionNode>(
              *bnode, fn_item, used_inputs, used_outputs);
          lf::Node &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));

          for (const int i : used_inputs.index_range()) {
            lf::InputSocket &socket = node.input(i);
            const bNodeSocket &bsocket = *used_inputs[i];
            BLI_assert(!bsocket.is_multi_input());
            input_socket_map.add(&bsocket, &socket);
            prepare_socket_default_value(socket, bsocket, resources);
            const Span<const bNodeLink *> links = bsocket.directly_linked_links();
            if (links.is_empty() || (links.size() == 1 && links[0]->is_muted())) {
              create_init_func_if_necessary(socket, bsocket, graph, resources);
            }
          }
          for (const int i : used_outputs.index_range()) {
            const bNodeSocket &bsocket = *used_outputs[i];
            output_socket_map.add(&bsocket, &node.output(i));
          }
        }
        break;
      }
    }
  }

  for (const auto item : output_socket_map.items()) {
    const bNodeSocket &from_bsocket = *item.key;
    lf::OutputSocket &from = *item.value;
    const Span<const bNodeLink *> links_from_socket = from_bsocket.directly_linked_links();

    struct TypeWithLinks {
      const CPPType *type;
      Vector<const bNodeLink *> links;
    };

    Vector<TypeWithLinks> types_with_links;
    for (const bNodeLink *link : links_from_socket) {
      if (link->is_muted()) {
        continue;
      }
      const bNodeSocket &to_socket = *link->tosock;
      if (!to_socket.is_available()) {
        continue;
      }
      const CPPType *to_type = get_socket_cpp_type(to_socket);
      if (to_type == nullptr) {
        continue;
      }
      bool inserted = false;
      for (TypeWithLinks &type_with_links : types_with_links) {
        if (type_with_links.type == to_type) {
          type_with_links.links.append(link);
          inserted = true;
        }
      }
      if (inserted) {
        continue;
      }
      types_with_links.append({to_type, {link}});
    }

    for (const TypeWithLinks &type_with_links : types_with_links) {
      const CPPType &to_type = *type_with_links.type;
      const Span<const bNodeLink *> links = type_with_links.links;
      lf::OutputSocket *final_from_socket = insert_type_conversion(
          graph, from, to_type, conversions, resources);

      auto make_input_link_or_set_default = [&](lf::InputSocket &to_socket) {
        if (final_from_socket != nullptr) {
          graph.add_link(*final_from_socket, to_socket);
        }
        else {
          const void *default_value = to_type.default_value();
          to_socket.set_default_value(default_value);
        }
      };

      for (const bNodeLink *blink : links) {
        const bNodeSocket &to_bsocket = *blink->tosock;
        if (to_bsocket.is_multi_input()) {
          /* TODO: Use stored link index, but need to validate it. */
          const int link_index = to_bsocket.directly_linked_links().first_index_try(blink);
          if (to_bsocket.owner_node().is_muted()) {
            if (link_index == 0) {
              for (lf::InputSocket *to : input_socket_map.lookup(&to_bsocket)) {
                make_input_link_or_set_default(*to);
              }
            }
          }
          else {
            lf::Node *multi_input_node = multi_input_socket_nodes.lookup_default(&to_bsocket,
                                                                                 nullptr);
            if (multi_input_node == nullptr) {
              continue;
            }
            make_input_link_or_set_default(multi_input_node->input(link_index));
          }
        }
        else {
          for (lf::InputSocket *to : input_socket_map.lookup(&to_bsocket)) {
            make_input_link_or_set_default(*to);
          }
        }
      }
    }
  }
}

}  // namespace blender::nodes
