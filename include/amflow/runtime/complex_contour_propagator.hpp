#pragma once

#include <cstddef>
#include <complex>
#include <functional>
#include <string>
#include <vector>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include "amflow/runtime/continuation_path.hpp"

namespace amflow {

using ComplexContourFloat = boost::multiprecision::cpp_dec_float_100;
using ComplexContourNumber = std::complex<ComplexContourFloat>;
using ComplexContourVector = std::vector<ComplexContourNumber>;
using ComplexContourMatrix = std::vector<std::vector<ComplexContourNumber>>;
using ComplexContourMatrixEvaluator =
    std::function<ComplexContourMatrix(const ComplexContourNumber& eta)>;
using ComplexContourLaurentMatrixEvaluator =
    std::function<std::vector<ComplexContourMatrix>(
        const ComplexContourNumber& eta)>;

enum class ComplexContourIntegrator {
  DormandPrinceRk45,
  FehlbergRk78,
};

struct ComplexContourPropagationOptions {
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower;
  ComplexContourIntegrator integrator =
      ComplexContourIntegrator::DormandPrinceRk45;
  std::size_t steps_per_segment = 32;
  std::size_t refinement_doublings = 1;
  std::size_t max_refinement_doublings = 4;
  std::size_t working_precision_digits = 100;
  ComplexContourFloat refinement_error_tolerance = ComplexContourFloat("1e-60");
  ComplexContourFloat refinement_relative_error_tolerance = 0;
  std::string matrix_fingerprint;
  std::string endpoint_integral_id;
  std::string endpoint_local_model_kind = "regular-taylor-r0-pending-laurent-fit";
  std::string branch_policy = "NegIm lower-half-plane contour supplied by caller";
  std::size_t max_adaptive_steps_per_segment = 65536;
  ComplexContourFloat pole_step_safety_factor = ComplexContourFloat("0.25");
  std::vector<ComplexContourNumber> contour_poles;
  std::vector<ComplexContourNumber> diagnostic_poles;
  std::vector<std::size_t> coupled_frobenius_anchor_row_indices;
  ComplexContourVector coupled_frobenius_anchor_endpoint_values;
};

struct ComplexContourScalarFrobeniusSeriesPatch {
  ComplexContourNumber center;
  ComplexContourFloat sample_radius = 0;
  std::size_t order = 0;
  std::size_t sample_count = 0;
  ComplexContourNumber indicial_exponent;
  std::vector<ComplexContourNumber> regular_tail_coefficients;
  std::vector<ComplexContourNumber> series_coefficients;
  std::string indicial_equation;
  std::string summary;
};

struct ComplexContourPropagationDiagnostics {
  bool success = false;
  bool ode_propagation_applied = false;
  bool coefficient_publication = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  bool endpoint_extraction_applied = false;
  bool scalar_frobenius_endpoint_patch_applied = false;
  bool coupled_frobenius_endpoint_matcher_applied = false;
  std::size_t dimension = 0;
  std::size_t waypoint_count = 0;
  std::size_t segment_count = 0;
  std::size_t coarse_step_count = 0;
  std::size_t refined_step_count = 0;
  std::size_t refinement_doublings_used = 0;
  std::size_t working_precision_digits = 0;
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower;
  bool eta_zero_endpoint_reached = false;
  std::string runtime_application;
  std::string transport_scope;
  std::string branch_policy;
  std::string endpoint_target;
  std::string endpoint_integral_id;
  std::string endpoint_local_model_kind;
  std::string matrix_fingerprint;
  std::string contour_fingerprint;
  std::string endpoint_vector_norm_abs;
  std::string refinement_error_abs;
  std::string refinement_error_tolerance_abs;
  std::string refinement_error_tolerance_rel;
  std::string refinement_effective_tolerance_abs;
  std::size_t refinement_error_peak_segment_index = 0;
  std::string refinement_error_peak_eta;
  std::string refinement_error_peak_waypoint_error_abs;
  std::string refinement_error_peak_state_norm_abs;
  std::string refinement_error_peak_rhs_norm_abs;
  std::string refinement_error_peak_matrix_max_entry_abs;
  std::string refinement_error_peak_matrix_max_row_l1_abs;
  std::string refinement_error_peak_matrix_min_lu_pivot_abs;
  std::string refinement_error_peak_matrix_pivot_ratio_abs;
  std::string refinement_error_peak_nearest_pole;
  std::string refinement_error_peak_nearest_pole_distance_abs;
  std::string failure_code;
  std::string summary;
  std::size_t adaptive_step_count = 0;
  std::size_t adaptive_rejected_step_count = 0;
  std::size_t pole_pinch_step_count = 0;
  std::string integrator;
  std::string max_embedded_error_abs;
  std::size_t max_embedded_error_segment_index = 0;
  std::string max_embedded_error_eta;
  std::string max_embedded_error_state_norm_abs;
  std::string max_embedded_error_rhs_norm_abs;
  std::string max_embedded_error_matrix_max_entry_abs;
  std::string max_embedded_error_matrix_max_row_l1_abs;
  std::string max_embedded_error_matrix_min_lu_pivot_abs;
  std::string max_embedded_error_matrix_pivot_ratio_abs;
  std::string max_embedded_error_nearest_pole;
  std::string max_embedded_error_nearest_pole_distance_abs;
};

struct ComplexContourPropagationResult {
  bool success = false;
  ComplexContourVector endpoint_values;
  ComplexContourPropagationDiagnostics diagnostics;
};

struct ComplexContourIndicialEquation {
  bool success = false;
  bool indicial_roots_available = false;
  std::size_t dimension = 0;
  ComplexContourNumber residue_probe_eta = {0, 0};
  ComplexContourFloat residue_tolerance = 0;
  std::string failure_code;
  std::string triangular_form;
  std::string characteristic_polynomial_convention =
      "det(rho*I - sampled_eta_times_A_eta)=0";
  std::string sampled_residue_stability_abs;
  std::string sampled_residue_effective_tolerance_abs;
  ComplexContourMatrix residue_matrix;
  std::vector<ComplexContourNumber> characteristic_polynomial_coefficients_descending;
  std::vector<ComplexContourNumber> indicial_roots;
  std::string summary;
};

struct ComplexContourFrobeniusRecurrenceOptions {
  std::size_t selected_root_index = 0;
  std::size_t order = 0;
  ComplexContourVector leading_coefficients;
  ComplexContourNumber residue_probe_eta = {0, ComplexContourFloat("-1e-40")};
  ComplexContourFloat residue_tolerance = ComplexContourFloat("1e-28");
  ComplexContourFloat tail_fit_tolerance = ComplexContourFloat("1e-24");
  ComplexContourFloat tail_sample_radius = 0;
  std::size_t tail_sample_count = 0;
};

struct ComplexContourFrobeniusRecurrence {
  bool success = false;
  std::size_t dimension = 0;
  std::size_t selected_root_index = 0;
  std::size_t order = 0;
  ComplexContourNumber indicial_root = {0, 0};
  ComplexContourNumber residue_probe_eta = {0, 0};
  ComplexContourFloat residue_tolerance = 0;
  ComplexContourFloat tail_fit_tolerance = 0;
  std::string failure_code;
  std::string recurrence_convention =
      "((rho+n)I - R)c_n = sum_{k=0}^{n-1} A_k*c_{n-1-k}";
  std::string tail_fit_residual_abs;
  ComplexContourMatrix residue_matrix;
  std::vector<ComplexContourMatrix> regular_tail_matrices;
  std::vector<ComplexContourVector> coefficients;
  std::string summary;
};

struct ComplexContourFrobeniusEndpointEvaluation {
  bool success = false;
  bool endpoint_value_available = false;
  std::string failure_code;
  std::string limit_classification;
  ComplexContourVector endpoint_value;
  std::string summary;
};

struct ComplexContourScalarReducibleEndpointOptions {
  ComplexContourNumber endpoint = {0, 0};
  ComplexContourNumber residue_probe_eta = {0, ComplexContourFloat("-1e-40")};
  ComplexContourFloat residue_tolerance = ComplexContourFloat("1e-28");
  ComplexContourFloat tail_fit_tolerance = ComplexContourFloat("1e-24");
  ComplexContourFloat coupling_tolerance = ComplexContourFloat("1e-24");
  ComplexContourFloat sample_radius = ComplexContourFloat("0.0625");
  std::size_t tail_order = 4;
  std::size_t frobenius_order = 32;
  std::size_t sample_count = 0;
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower;
};

struct ComplexContourScalarReducibleEndpointRows {
  bool success = false;
  std::size_t dimension = 0;
  ComplexContourNumber endpoint = {0, 0};
  ComplexContourNumber match_eta = {0, 0};
  ComplexContourNumber residue_probe_eta = {0, 0};
  ComplexContourFloat residue_tolerance = 0;
  ComplexContourFloat tail_fit_tolerance = 0;
  ComplexContourFloat coupling_tolerance = 0;
  std::size_t tail_order = 0;
  std::size_t frobenius_order = 0;
  std::size_t sample_count = 0;
  std::string failure_code;
  std::string tail_fit_residual_abs;
  std::vector<ComplexContourNumber> indicial_roots;
  std::vector<std::size_t> scalar_reducible_row_indices;
  std::vector<std::size_t> irreducible_row_indices;
  std::vector<std::size_t> endpoint_value_row_indices;
  std::vector<std::size_t> deferred_endpoint_row_indices;
  std::vector<bool> endpoint_value_available;
  ComplexContourVector endpoint_values;
  std::string summary;
};

ComplexContourIndicialEquation ComputeComplexContourEtaZeroIndicialEquation(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    std::size_t dimension);

ComplexContourIndicialEquation ComputeComplexContourEtaZeroIndicialEquation(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    std::size_t dimension,
    ComplexContourNumber residue_probe_eta,
    ComplexContourFloat residue_tolerance);

ComplexContourFrobeniusRecurrence
ComputeComplexContourEtaZeroFrobeniusRecurrence(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    std::size_t dimension,
    const ComplexContourFrobeniusRecurrenceOptions& options);

ComplexContourFrobeniusEndpointEvaluation
EvaluateComplexContourFrobeniusEtaZeroEndpoint(
    const ComplexContourFrobeniusRecurrence& recurrence);

ComplexContourScalarReducibleEndpointRows
ApplyComplexContourScalarReducibleFrobeniusEndpointRows(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourNumber& match_eta,
    const ComplexContourVector& match_values,
    const ComplexContourScalarReducibleEndpointOptions& options = {});

ComplexContourVector FlattenComplexContourCoefficientState(
    const std::vector<ComplexContourVector>& coefficient_vectors);

std::vector<ComplexContourVector> UnflattenComplexContourCoefficientState(
    const ComplexContourVector& coefficient_state,
    std::size_t master_dimension,
    int min_state_eps_order,
    int max_state_eps_order);

ComplexContourMatrix BuildComplexContourCoefficientStateMatrix(
    const std::vector<ComplexContourMatrix>& matrix_coefficients,
    std::size_t master_dimension,
    int min_matrix_eps_order,
    int min_state_eps_order,
    int max_state_eps_order);

ComplexContourMatrixEvaluator MakeComplexContourCoefficientStateMatrixEvaluator(
    const ComplexContourLaurentMatrixEvaluator& matrix_evaluator,
    std::size_t master_dimension,
    int min_matrix_eps_order,
    int min_state_eps_order,
    int max_state_eps_order);

ComplexContourPropagationResult PropagateComplexContourVector(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options = {});

ComplexContourPropagationResult PropagateComplexContourVectorAlongWaypoints(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options = {});

ComplexContourScalarFrobeniusSeriesPatch
GenerateScalarComplexFrobeniusEndpointPatch(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourNumber& endpoint,
    const ComplexContourFloat& sample_radius,
    std::size_t order,
    std::size_t sample_count = 0);

ComplexContourNumber EvaluateScalarComplexFrobeniusSeries(
    const ComplexContourScalarFrobeniusSeriesPatch& patch,
    const ComplexContourNumber& eta,
    EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower);

ComplexContourNumber MatchScalarComplexFrobeniusEndpointCoefficient(
    const ComplexContourScalarFrobeniusSeriesPatch& patch,
    const ComplexContourNumber& match_eta,
    const ComplexContourNumber& match_value,
    EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower);

}  // namespace amflow
