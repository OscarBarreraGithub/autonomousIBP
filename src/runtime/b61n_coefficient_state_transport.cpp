#include "amflow/runtime/b61n_coefficient_state_transport.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <boost/math/special_functions/fpclassify.hpp>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

bool IsFiniteFloat(const ComplexContourFloat& value) {
  return boost::math::isfinite(value);
}

bool IsFiniteComplex(const ComplexContourNumber& value) {
  return IsFiniteFloat(value.real()) && IsFiniteFloat(value.imag());
}

std::string CompactFloat(const ComplexContourFloat& raw_value,
                         const int precision_digits = 50) {
  if (raw_value == ComplexContourFloat(0)) {
    return "0";
  }
  std::ostringstream stream;
  stream << std::setprecision(precision_digits) << raw_value;
  return stream.str();
}

std::string CompactComplex(const ComplexContourNumber& value,
                           const int precision_digits = 50) {
  return "(" + CompactFloat(value.real(), precision_digits) + "," +
         CompactFloat(value.imag(), precision_digits) + ")";
}

std::string JoinNodeList(const std::vector<B61nCoefficientNode>& nodes,
                         const std::vector<std::string>& master_labels) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << FormatB61nCoefficientNode(nodes[index], master_labels);
  }
  out << "]";
  return out.str();
}

void ValidateCoefficientVectors(
    const std::vector<std::string>& master_labels,
    const B61nFiniteStartCoefficientData& finite_start_coefficients) {
  if (finite_start_coefficients.min_state_eps_order >
      finite_start_coefficients.max_state_eps_order) {
    throw std::invalid_argument(
        "b61n coefficient-state transport requires a valid epsilon-order range");
  }
  const std::size_t expected_order_count = static_cast<std::size_t>(
      static_cast<long long>(finite_start_coefficients.max_state_eps_order) -
      static_cast<long long>(finite_start_coefficients.min_state_eps_order) +
      1LL);
  if (finite_start_coefficients.coefficient_vectors.size() !=
      expected_order_count) {
    throw std::invalid_argument(
        "b61n coefficient-state transport received a finite-start vector count "
        "that does not match the epsilon-order range");
  }
  for (const ComplexContourVector& coefficient_vector :
       finite_start_coefficients.coefficient_vectors) {
    if (coefficient_vector.size() != master_labels.size()) {
      throw std::invalid_argument(
          "b61n coefficient-state transport finite-start vectors must use the "
          "full retained-master dimension");
    }
    for (const ComplexContourNumber& value : coefficient_vector) {
      if (!IsFiniteComplex(value)) {
        throw std::invalid_argument(
            "b61n coefficient-state transport received a nonfinite finite-start "
            "coefficient");
      }
    }
  }
}

void ValidateTargetGraphForStateWindow(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const B61nFiniteStartCoefficientData& finite_start_coefficients) {
  if (!target_graph.blocked_edges.empty()) {
    throw std::invalid_argument(
        "b61n coefficient-state transport requires a closed target graph with "
        "no blocked coefficient dependencies");
  }
  if (target_graph.closed_nodes.empty()) {
    throw std::invalid_argument(
        "b61n coefficient-state transport requires a nonempty target graph");
  }
  for (const B61nCoefficientNode& node : target_graph.closed_nodes) {
    if (node.master_index >= master_labels.size()) {
      throw std::invalid_argument(
          "b61n coefficient-state transport target graph references an "
          "unknown master");
    }
    if (node.eps_order < finite_start_coefficients.min_state_eps_order ||
        node.eps_order > finite_start_coefficients.max_state_eps_order) {
      throw std::invalid_argument(
          "b61n coefficient-state transport target graph node is outside the "
          "materialized finite-start epsilon window");
    }
  }
  for (const B61nCoefficientNode& node : target_graph.public_targets) {
    if (node.master_index >= master_labels.size() ||
        node.eps_order < finite_start_coefficients.min_state_eps_order ||
        node.eps_order > finite_start_coefficients.max_state_eps_order) {
      throw std::invalid_argument(
          "b61n coefficient-state transport public target is outside the "
          "materialized finite-start epsilon window");
    }
  }
}

ComplexContourPropagationOptions NormalizePropagationOptions(
    ComplexContourPropagationOptions options,
    const B61nLaurentMatrixEvaluator& laurent_matrix_evaluator) {
  if (options.matrix_fingerprint.empty()) {
    options.matrix_fingerprint = laurent_matrix_evaluator.audit.fingerprint;
  }
  if (options.endpoint_local_model_kind.empty() ||
      options.endpoint_local_model_kind == "regular-taylor-r0-pending-laurent-fit") {
    options.endpoint_local_model_kind =
        "b61n-coefficient-state-transport-before-endpoint-matcher";
  }
  if (options.branch_policy.empty() ||
      options.branch_policy == "NegIm lower-half-plane contour supplied by caller") {
    options.branch_policy =
        "NegIm lower-half-plane b61n coefficient-state transport";
  }
  return options;
}

std::string SerializeEndpointForFingerprint(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const B61nFiniteStartCoefficientData& finite_start_coefficients,
    const B61nLaurentMatrixEvaluator& laurent_matrix_evaluator,
    const B61nCoefficientStateTransportAudit& audit,
    const std::vector<ComplexContourVector>& endpoint_coefficients) {
  std::ostringstream out;
  out << "kind=b61n-coefficient-state-endpoint\n";
  out << "matrix_fingerprint=" << laurent_matrix_evaluator.audit.fingerprint
      << "\n";
  out << "finite_start_fingerprint="
      << finite_start_coefficients.fingerprint << "\n";
  out << "min_state_eps_order=" << finite_start_coefficients.min_state_eps_order
      << "\n";
  out << "max_state_eps_order=" << finite_start_coefficients.max_state_eps_order
      << "\n";
  out << "min_matrix_eps_order=" << audit.min_matrix_eps_order << "\n";
  out << "max_matrix_eps_order=" << audit.max_matrix_eps_order << "\n";
  out << "target_nodes=" << JoinNodeList(target_graph.closed_nodes, master_labels)
      << "\n";
  for (std::size_t order_index = 0; order_index < endpoint_coefficients.size();
       ++order_index) {
    const int eps_order = finite_start_coefficients.min_state_eps_order +
                          static_cast<int>(order_index);
    for (std::size_t master = 0; master < endpoint_coefficients[order_index].size();
         ++master) {
      out << "endpoint[" << eps_order << "," << master << "]="
          << CompactComplex(endpoint_coefficients[order_index][master], 70)
          << "\n";
    }
  }
  return out.str();
}

std::string DiagnosticFieldSummary(
    const B61nCoefficientStateTransportAudit& audit) {
  return "sample_space_coupled_row_transport_applied=" +
         std::string(audit.sample_space_coupled_row_transport_applied ? "true"
                                                                      : "false") +
         "; coefficient_state_transport_applied=" +
         std::string(audit.coefficient_state_transport_applied ? "true"
                                                               : "false") +
         "; coefficient_state_endpoint_matcher_applied=" +
         std::string(audit.coefficient_state_endpoint_matcher_applied ? "true"
                                                                      : "false") +
         "; target_coefficients_published_from_coefficient_state=" +
         std::string(audit.target_coefficients_published_from_coefficient_state
                         ? "true"
                         : "false") +
         "; target_coefficients_reconstructed_from_epsilon_samples=" +
         std::string(audit.target_coefficients_reconstructed_from_epsilon_samples
                         ? "true"
                         : "false");
}

B61nCoefficientStateTransportResult FailureResult(
    B61nCoefficientStateTransportAudit audit,
    const std::string& failure_code,
    const std::string& reason) {
  audit.success = false;
  audit.failure_code = failure_code;
  audit.summary =
      "b61n coefficient-state transport blocked: " + reason +
      "; failure_code=" + failure_code +
      "; master_dimension=" + std::to_string(audit.master_dimension) +
      "; augmented_dimension=" + std::to_string(audit.augmented_dimension) +
      "; waypoint_count=" + std::to_string(audit.waypoint_count) +
      "; target_graph_node_count=" +
      std::to_string(audit.target_graph_node_count) +
      "; public_target_node_count=" +
      std::to_string(audit.public_target_node_count) +
      "; materialized_node_count=" +
      std::to_string(audit.materialized_node_count) +
      "; matrix_coefficient_count=" +
      std::to_string(audit.matrix_coefficient_count) +
      "; min_state_eps_order=" + std::to_string(audit.min_state_eps_order) +
      "; max_state_eps_order=" + std::to_string(audit.max_state_eps_order) +
      "; min_matrix_eps_order=" + std::to_string(audit.min_matrix_eps_order) +
      "; max_matrix_eps_order=" + std::to_string(audit.max_matrix_eps_order) +
      "; " + DiagnosticFieldSummary(audit) +
      "; final_solution_samples_used_as_input=false; full_eta_zero_contour_applied=false";
  return {false, {}, std::move(audit)};
}

}  // namespace

B61nCoefficientStateTransportResult PropagateB61nCoefficientState(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const B61nLaurentMatrixEvaluator& laurent_matrix_evaluator,
    const B61nFiniteStartCoefficientData& finite_start_coefficients,
    const std::vector<ComplexContourNumber>& waypoints,
    const B61nCoefficientStateTransportOptions& options) {
  B61nCoefficientStateTransportAudit audit;
  audit.master_dimension = master_labels.size();
  audit.waypoint_count = waypoints.size();
  audit.target_graph_node_count = target_graph.closed_nodes.size();
  audit.public_target_node_count = target_graph.public_targets.size();
  audit.materialized_node_count =
      finite_start_coefficients.materialized_nodes.size();
  audit.matrix_coefficient_count =
      laurent_matrix_evaluator.audit.matrix_coefficient_count;
  audit.min_state_eps_order = finite_start_coefficients.min_state_eps_order;
  audit.max_state_eps_order = finite_start_coefficients.max_state_eps_order;
  audit.min_matrix_eps_order =
      laurent_matrix_evaluator.audit.min_matrix_eps_order;
  audit.max_matrix_eps_order =
      laurent_matrix_evaluator.audit.max_matrix_eps_order;
  audit.matrix_fingerprint = laurent_matrix_evaluator.audit.fingerprint;
  audit.finite_start_fingerprint = finite_start_coefficients.fingerprint;

  try {
    if (master_labels.empty()) {
      throw std::invalid_argument(
          "b61n coefficient-state transport requires retained master labels");
    }
    if (!laurent_matrix_evaluator.evaluator) {
      throw std::invalid_argument(
          "b61n coefficient-state transport requires a Laurent matrix evaluator");
    }
    if (waypoints.size() < 2) {
      throw std::invalid_argument(
          "b61n coefficient-state transport requires at least two contour "
          "waypoints");
    }
    ValidateCoefficientVectors(master_labels, finite_start_coefficients);
    ValidateTargetGraphForStateWindow(master_labels,
                                      target_graph,
                                      finite_start_coefficients);

    const ComplexContourVector initial_state =
        FlattenComplexContourCoefficientState(
            finite_start_coefficients.coefficient_vectors);
    const std::vector<ComplexContourMatrix> initial_matrix_coefficients =
        laurent_matrix_evaluator.evaluator(waypoints.front());
    const ComplexContourMatrix initial_augmented_matrix =
        BuildComplexContourCoefficientStateMatrix(
            initial_matrix_coefficients,
            master_labels.size(),
            audit.min_matrix_eps_order,
            audit.min_state_eps_order,
            audit.max_state_eps_order);
    audit.augmented_dimension = initial_augmented_matrix.size();

    const ComplexContourMatrixEvaluator coefficient_state_evaluator =
        MakeComplexContourCoefficientStateMatrixEvaluator(
            laurent_matrix_evaluator.evaluator,
            master_labels.size(),
            audit.min_matrix_eps_order,
            audit.min_state_eps_order,
            audit.max_state_eps_order);

    const ComplexContourPropagationOptions propagation_options =
        NormalizePropagationOptions(options.propagation_options,
                                    laurent_matrix_evaluator);
    const ComplexContourPropagationResult propagation_result =
        PropagateComplexContourVector(initial_state,
                                      waypoints,
                                      coefficient_state_evaluator,
                                      propagation_options);
    audit.coefficient_state_transport_applied =
        propagation_result.diagnostics.ode_propagation_applied;
    audit.segment_count = propagation_result.diagnostics.segment_count;
    audit.adaptive_step_count =
        propagation_result.diagnostics.adaptive_step_count;
    audit.adaptive_rejected_step_count =
        propagation_result.diagnostics.adaptive_rejected_step_count;
    audit.pole_pinch_step_count =
        propagation_result.diagnostics.pole_pinch_step_count;
    audit.contour_fingerprint =
        propagation_result.diagnostics.contour_fingerprint;
    audit.propagation_summary = propagation_result.diagnostics.summary;

    if (!propagation_result.success ||
        propagation_result.endpoint_values.size() != initial_state.size()) {
      return FailureResult(
          std::move(audit),
          propagation_result.diagnostics.failure_code.empty()
              ? std::string("coefficient-state-propagation-failed")
              : propagation_result.diagnostics.failure_code,
          "coefficient-state ODE propagation failed closed; propagator_summary=" +
              propagation_result.diagnostics.summary);
    }

    std::vector<ComplexContourVector> endpoint_coefficients =
        UnflattenComplexContourCoefficientState(propagation_result.endpoint_values,
                                                master_labels.size(),
                                                audit.min_state_eps_order,
                                                audit.max_state_eps_order);
    audit.endpoint_coefficient_state_unflattened = true;
    audit.endpoint_fingerprint =
        ComputeArtifactFingerprint(SerializeEndpointForFingerprint(
            master_labels,
            target_graph,
            finite_start_coefficients,
            laurent_matrix_evaluator,
            audit,
            endpoint_coefficients));
    audit.success = true;
    audit.summary =
        "b61n coefficient-state transport propagated the closed coefficient "
        "state through padded retained-master epsilon-order vectors; "
        "master_dimension=" +
        std::to_string(audit.master_dimension) +
        "; augmented_dimension=" + std::to_string(audit.augmented_dimension) +
        "; waypoint_count=" + std::to_string(audit.waypoint_count) +
        "; target_graph_node_count=" +
        std::to_string(audit.target_graph_node_count) +
        "; public_target_node_count=" +
        std::to_string(audit.public_target_node_count) +
        "; materialized_node_count=" +
        std::to_string(audit.materialized_node_count) +
        "; matrix_coefficient_count=" +
        std::to_string(audit.matrix_coefficient_count) +
        "; min_state_eps_order=" + std::to_string(audit.min_state_eps_order) +
        "; max_state_eps_order=" + std::to_string(audit.max_state_eps_order) +
        "; min_matrix_eps_order=" + std::to_string(audit.min_matrix_eps_order) +
        "; max_matrix_eps_order=" + std::to_string(audit.max_matrix_eps_order) +
        "; endpoint_coefficient_state_unflattened=true; " +
        DiagnosticFieldSummary(audit) +
        "; matrix_fingerprint=" + audit.matrix_fingerprint +
        "; finite_start_fingerprint=" + audit.finite_start_fingerprint +
        "; contour_fingerprint=" + audit.contour_fingerprint +
        "; endpoint_fingerprint=" + audit.endpoint_fingerprint +
        "; coefficient_publication=false; final_solution_samples_used_as_input=false; "
        "full_eta_zero_contour_applied=false";
    return {true, std::move(endpoint_coefficients), std::move(audit)};
  } catch (const std::exception& error) {
    return FailureResult(std::move(audit),
                         "coefficient-state-transport-invalid-input",
                         error.what());
  }
}

}  // namespace amflow
