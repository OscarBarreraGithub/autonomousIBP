#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "amflow/runtime/b61n_coefficient_target_graph.hpp"
#include "amflow/runtime/b61n_finite_start_coefficients.hpp"
#include "amflow/runtime/b61n_laurent_matrix_evaluator.hpp"
#include "amflow/runtime/complex_contour_propagator.hpp"

namespace amflow {

struct B61nCoefficientStateTransportOptions {
  ComplexContourPropagationOptions propagation_options;
  bool coefficient_state_endpoint_matching_enabled = true;
};

struct B61nCoefficientStateTransportAudit {
  bool success = false;
  bool sample_space_coupled_row_transport_applied = false;
  bool coefficient_state_transport_applied = false;
  bool coefficient_state_endpoint_matcher_applied = false;
  bool target_coefficients_published_from_coefficient_state = false;
  bool target_coefficients_reconstructed_from_epsilon_samples = false;
  bool endpoint_coefficient_state_unflattened = false;
  std::size_t master_dimension = 0;
  std::size_t augmented_dimension = 0;
  std::size_t waypoint_count = 0;
  std::size_t target_graph_node_count = 0;
  std::size_t public_target_node_count = 0;
  std::size_t materialized_node_count = 0;
  std::size_t matrix_coefficient_count = 0;
  std::size_t endpoint_anchor_node_count = 0;
  std::size_t endpoint_free_node_count = 0;
  std::size_t endpoint_matcher_recurrence_count = 0;
  std::size_t endpoint_matcher_recurrence_order = 0;
  std::size_t segment_count = 0;
  std::size_t adaptive_step_count = 0;
  std::size_t adaptive_rejected_step_count = 0;
  std::size_t pole_pinch_step_count = 0;
  int min_state_eps_order = 0;
  int max_state_eps_order = 0;
  int min_matrix_eps_order = 0;
  int max_matrix_eps_order = 0;
  std::string failure_code;
  std::string matrix_fingerprint;
  std::string finite_start_fingerprint;
  std::string contour_fingerprint;
  std::string endpoint_fingerprint;
  std::string endpoint_matcher_residual_abs;
  std::string endpoint_matcher_boundary_condition_residual_abs;
  std::string endpoint_matcher_boundary_condition_solve;
  std::string propagation_summary;
  std::string summary;
};

struct B61nCoefficientStateTransportResult {
  bool success = false;
  std::vector<ComplexContourVector> endpoint_coefficient_vectors;
  B61nCoefficientStateTransportAudit audit;
};

B61nCoefficientStateTransportResult PropagateB61nCoefficientState(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const B61nLaurentMatrixEvaluator& laurent_matrix_evaluator,
    const B61nFiniteStartCoefficientData& finite_start_coefficients,
    const std::vector<ComplexContourNumber>& waypoints,
    const B61nCoefficientStateTransportOptions& options = {});

}  // namespace amflow
