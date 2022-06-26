/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"
#include "FN_lazy_function.hh"
#include "FN_multi_function_builder.hh"

#include "BKE_attribute_access.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

struct Depsgraph;
struct ModifierData;

namespace blender::nodes {

using bke::AnonymousAttributeFieldInput;
using bke::AttributeFieldInput;
using bke::AttributeIDRef;
using bke::GeometryComponentFieldContext;
using bke::GeometryFieldInput;
using bke::OutputAttribute;
using bke::OutputAttribute_Typed;
using bke::ReadAttributeLookup;
using bke::StrongAnonymousAttributeID;
using bke::WeakAnonymousAttributeID;
using bke::WriteAttributeLookup;
using fn::Field;
using fn::FieldContext;
using fn::FieldEvaluator;
using fn::FieldInput;
using fn::FieldOperation;
using fn::GField;
using fn::LFParams;
using fn::ValueOrField;
using geo_eval_log::NamedAttributeUsage;
using geo_eval_log::NodeWarningType;

class GeoNodeExecParams {
 private:
  const NodeRef &node_;
  LFParams &params_;
  const fn::LFContext &lf_context_;

 public:
  GeoNodeExecParams(const NodeRef &node, LFParams &params, const fn::LFContext &lf_context)
      : node_(node), params_(params), lf_context_(lf_context)
  {
  }

  template<typename T>
  static inline constexpr bool is_field_base_type_v =
      is_same_any_v<T, float, int, bool, ColorGeometry4f, float3, std::string>;

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * This method can only be called once for each identifier.
   */
  template<typename T> T extract_input(StringRef identifier)
  {
    if constexpr (is_field_base_type_v<T>) {
      ValueOrField<T> value_or_field = this->extract_input<ValueOrField<T>>(identifier);
      return value_or_field.as_value();
    }
    else if constexpr (fn::is_field_v<T>) {
      using BaseType = typename T::base_type;
      ValueOrField<BaseType> value_or_field = this->extract_input<ValueOrField<BaseType>>(
          identifier);
      return value_or_field.as_field();
    }
    else {
#ifdef DEBUG
      this->check_input_access(identifier, &CPPType::get<T>());
#endif
      const int index = this->get_input_index(identifier);
      T value = params_.extract_input<T>(index);
      if constexpr (std::is_same_v<T, GeometrySet>) {
        this->check_input_geometry_set(identifier, value);
      }
      return value;
    }
  }

  void check_input_geometry_set(StringRef identifier, const GeometrySet &geometry_set) const;

  /**
   * Get the input value for the input socket with the given identifier.
   */
  template<typename T> T get_input(StringRef identifier) const
  {
    if constexpr (is_field_base_type_v<T>) {
      ValueOrField<T> value_or_field = this->get_input<ValueOrField<T>>(identifier);
      return value_or_field.as_value();
    }
    else if constexpr (fn::is_field_v<T>) {
      using BaseType = typename T::base_type;
      ValueOrField<BaseType> value_or_field = this->get_input<ValueOrField<BaseType>>(identifier);
      return value_or_field.as_field();
    }
    else {
#ifdef DEBUG
      this->check_input_access(identifier, &CPPType::get<T>());
#endif
      const int index = this->get_input_index(identifier);
      const T &value = params_.get_input<T>(index);
      if constexpr (std::is_same_v<T, GeometrySet>) {
        this->check_input_geometry_set(identifier, value);
      }
      return value;
    }
  }

  /**
   * Store the output value for the given socket identifier.
   */
  template<typename T> void set_output(StringRef identifier, T &&value)
  {
    using StoredT = std::decay_t<T>;
    if constexpr (is_field_base_type_v<StoredT>) {
      this->set_output(identifier, ValueOrField<StoredT>(std::forward<T>(value)));
    }
    else if constexpr (fn::is_field_v<StoredT>) {
      using BaseType = typename StoredT::base_type;
      this->set_output(identifier, ValueOrField<BaseType>(std::forward<T>(value)));
    }
    else {
#ifdef DEBUG
      const CPPType &type = CPPType::get<StoredT>();
      this->check_output_access(identifier, type);
#endif
      const int index = this->get_output_index(identifier);
      const bNodeSocket *socket = node_.output_by_identifier(identifier).bsocket();

      geo_eval_log::GeoNodesTreeEvalLog &tree_log = this->get_local_log();
      tree_log.log_socket_value({socket}, &value);

      params_.set_output(index, std::forward<T>(value));
    }
  }

  geo_eval_log::GeoNodesTreeEvalLog &get_local_log() const
  {
    GeoNodesLFUserData *user_data = this->user_data();
    BLI_assert(user_data != nullptr);
    const ContextStack *context_stack = user_data->context_stack;
    BLI_assert(context_stack != nullptr);
    return user_data->modifier_data->eval_log->get_local_log(*context_stack);
  }

  /**
   * Tell the evaluator that a specific input won't be used anymore.
   */
  void set_input_unused(StringRef identifier)
  {
    const int index = this->get_input_index(identifier);
    params_.set_input_unused(index);
  }

  /**
   * Returns true when the output has to be computed.
   * Nodes that support laziness could use the #lazy_output_is_required variant to possibly avoid
   * some computations.
   */
  bool output_is_required(StringRef identifier) const
  {
    const int index = this->get_output_index(identifier);
    return params_.get_output_usage(index) != fn::ValueUsage::Unused;
  }

  /**
   * Tell the evaluator that a specific input is required.
   * This returns true when the input will only be available in the next execution.
   * False is returned if the input is available already.
   * This can only be used when the node supports laziness.
   */
  bool lazy_require_input(StringRef identifier)
  {
    const int index = this->get_input_index(identifier);
    return params_.try_get_input_data_ptr_or_request(index) == nullptr;
  }

  /**
   * Asks the evaluator if a specific output is required right now. If this returns false, the
   * value might still need to be computed later.
   * This can only be used when the node supports laziness.
   */
  bool lazy_output_is_required(StringRef identifier)
  {
    const int index = this->get_output_index(identifier);
    return params_.get_output_usage(index) == fn::ValueUsage::Used;
  }

  /**
   * Get the node that is currently being executed.
   */
  const bNode &node() const
  {
    return *node_.bnode();
  }

  const Object *self_object() const
  {
    if (const auto *data = this->user_data()) {
      if (data->modifier_data) {
        return data->modifier_data->self_object;
      }
    }
    return nullptr;
  }

  Depsgraph *depsgraph() const
  {
    if (const auto *data = this->user_data()) {
      if (data->modifier_data) {
        return data->modifier_data->depsgraph;
      }
    }
    return nullptr;
  }

  GeoNodesLFUserData *user_data() const
  {
    return dynamic_cast<GeoNodesLFUserData *>(lf_context_.user_data);
  }

  /**
   * Add an error message displayed at the top of the node when displaying the node tree,
   * and potentially elsewhere in Blender.
   */
  void error_message_add(const NodeWarningType type, std::string message) const;

  std::string attribute_producer_name() const;

  void set_default_remaining_outputs();

  void used_named_attribute(std::string attribute_name, NamedAttributeUsage usage);

 private:
  /* Utilities for detecting common errors at when using this class. */
  void check_input_access(StringRef identifier, const CPPType *requested_type = nullptr) const;
  void check_output_access(StringRef identifier, const CPPType &value_type) const;

  /* Find the active socket with the input name (not the identifier). */
  const bNodeSocket *find_available_socket(const StringRef name) const;

  int get_input_index(const StringRef identifier) const
  {
    int counter = 0;
    for (const InputSocketRef *socket : node_.inputs()) {
      if (!socket->is_available()) {
        continue;
      }
      if (socket->identifier() == identifier) {
        return counter;
      }
      counter++;
    }
    BLI_assert_unreachable();
    return -1;
  }

  int get_output_index(const StringRef identifier) const
  {
    int counter = 0;
    for (const OutputSocketRef *socket : node_.outputs()) {
      if (!socket->is_available()) {
        continue;
      }
      if (socket->identifier() == identifier) {
        return counter;
      }
      counter++;
    }
    BLI_assert_unreachable();
    return -1;
  }
};

}  // namespace blender::nodes
