#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct LightlikeGaugeLinkRuntimeState {
  bool amflow_state_input = false;
  bool solution_sample_cache_enabled = false;
  std::string benchmark_id;
  std::string family;
  std::string integral_kind;
  std::string variable;
  std::string start_location;
  std::string target_location;
  std::string boundary_state_kind;
  std::string boundary_point;
  std::vector<std::string> singular_points;
  std::vector<std::string> boundary_file_names;
  std::map<std::string, std::string> boundary_file_raws;
  std::vector<std::string> diffeq_variables;
  std::vector<std::string> epsilon_samples;
  std::vector<MasterIntegral> masters;
  std::vector<MasterIntegral> reduction_masters;
  std::vector<MasterIntegral> diffeq_masters;
  std::vector<TargetIntegral> targets;
};

struct LightlikeGaugeLinkSquareFamilyResult {
  ProblemSpec transformed_spec;
  std::string variable = "gaugex";
  std::vector<std::size_t> affected_propagator_indices;
  std::vector<std::string> generated_square_propagators;
};

struct LightlikeGaugeLinkTargetNormalization {
  std::string target_label;
  int affected_power_sum = 0;
  std::string normalization_factor;
};

struct LightlikeGaugeLinkFinitePartTerm {
  std::string region_key = "integer";
  int power = 0;
  int log_power = 0;
  std::string coefficient;
};

struct LightlikeGaugeLinkFinitePartResult {
  bool success = false;
  bool ir_subtraction_applied = false;
  std::string failure_code;
  std::string finite_part_coefficient;
  std::vector<std::string> dropped_singular_terms;
  std::string summary;
};

struct LightlikeGaugeLinkSixMasterEndpointTerms {
  std::string master_label;
  std::vector<LightlikeGaugeLinkFinitePartTerm> endpoint_terms;
};

struct LightlikeGaugeLinkTargetReductionTerm {
  std::string target_label;
  std::string master_label;
  int gaugex_power_shift = LightlikeGaugeLinkFinitePartTerm{}.power;
  int log_power = LightlikeGaugeLinkFinitePartTerm{}.log_power;
  std::string coefficient;
};

struct LightlikeGaugeLinkReducedFinitePartTarget {
  bool success = false;
  bool ir_subtraction_applied = false;
  std::string target_label;
  int affected_power_sum = LightlikeGaugeLinkFinitePartTerm{}.power;
  std::string normalization_factor;
  std::string finite_part_coefficient;
  std::vector<LightlikeGaugeLinkFinitePartTerm> reduced_endpoint_terms;
  std::vector<std::string> dropped_singular_terms;
  std::string failure_code;
  std::string summary;
};

struct LightlikeGaugeLinkReducedFinitePartFailure {
  std::string target_label;
  std::string failure_code;
  std::string summary;
};

struct LightlikeGaugeLinkReducedFinitePartResult {
  bool success = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  bool ir_subtraction_applied = false;
  std::string runtime_application;
  std::vector<LightlikeGaugeLinkReducedFinitePartTarget> targets;
  std::vector<LightlikeGaugeLinkReducedFinitePartFailure> failures;
  std::string summary;
};

struct LightlikeGaugeLinkPoleAudit {
  std::string value;
  int multiplicity = 0;
  std::vector<std::string> sources;
};

struct LightlikeGaugeLinkContourPlanAudit {
  bool success = false;
  bool endpoint_is_singular = false;
  std::size_t matrix_row_count = 0;
  std::size_t matrix_column_count = 0;
  std::size_t matrix_nonzero_cell_count = 0;
  std::vector<LightlikeGaugeLinkPoleAudit> poles;
  std::vector<std::string> waypoints;
  std::string variable = "gaugex";
  std::string desolver_local_variable = "eta";
  std::string boundary_point;
  std::string target_point;
  std::string half_plane;
  std::string nearest_nonzero_pole;
  std::string boundary_point_selection_rule;
  std::string endpoint_local_model_kind;
  std::string dropped_term_audit;
  std::string contour_fingerprint;
  std::string minimum_nonendpoint_pole_distance_to_contour;
  std::string pole_summary;
  std::string waypoint_summary;
};

struct LightlikeGaugeLinkTransportAudit {
  bool reviewed_surface = false;
  bool live_coefficients_available = false;
  bool runtime_scaffold_consumes_retained_solution_samples = false;
  bool retained_solution_samples_available = false;
  bool full_eta_zero_contour_applied = false;
  bool ir_subtraction_applied = false;
  bool boundary_data_available = false;
  bool diffeq_masters_cover_reduction_masters = false;
  bool diffeq_matrix_parsed = false;
  std::string family;
  std::string variable;
  std::string desolver_local_variable;
  std::string boundary_point;
  std::string target_point;
  std::string singular_endpoint;
  std::string runtime_application;
  std::string provider_strategy;
  std::string endpoint_selection_rule;
  std::string coefficient_gap;
  std::string failure_code_contract;
  std::string contour_half_plane;
  std::string contour_fingerprint;
  std::string endpoint_local_model_kind;
  std::string dropped_term_audit;
  std::string pole_summary;
  std::string waypoint_summary;
  std::string minimum_nonendpoint_pole_distance_to_contour;
  std::vector<std::size_t> affected_propagator_indices;
  std::vector<std::string> generated_square_propagators;
  std::vector<LightlikeGaugeLinkTargetNormalization> target_normalizations;
  std::vector<LightlikeGaugeLinkPoleAudit> diffeq_poles;
  std::vector<std::string> contour_waypoints;
  std::vector<std::string> pole_candidates;
  std::vector<std::string> boundary_file_names;
  std::size_t epsilon_sample_count = 0;
  std::size_t master_count = 0;
  std::size_t reduction_master_count = 0;
  std::size_t target_count = 0;
  std::size_t diffeq_matrix_row_count = 0;
  std::size_t diffeq_matrix_column_count = 0;
  std::size_t diffeq_matrix_nonzero_cell_count = 0;
  std::string summary;
};

struct LightlikeGaugeLinkSelectedCoefficientAudit {
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  bool ir_subtraction_applied = false;
  std::string master_label;
  std::string runtime_application;
  std::string transport_scope;
  std::string endpoint_local_model_kind;
  std::string contour_fingerprint;
  std::string extraction_fingerprint;
  std::string eta_zero_selection_audit;
  std::string summary;
};

bool IsLightlikeGaugeLinkEtaZeroRuntimeState(
    const LightlikeGaugeLinkRuntimeState& state);

ProblemSpec MakeReviewedLightlikeGaugeLinkProblemSpec();

LightlikeGaugeLinkSquareFamilyResult BuildLightlikeGaugeLinkSquareFamily(
    const ProblemSpec& spec,
    const std::string& variable = "gaugex");

std::vector<LightlikeGaugeLinkTargetNormalization>
ApplyLightlikeGaugeLinkPowerNormalization(
    const std::vector<TargetIntegral>& targets,
    const std::vector<std::size_t>& affected_propagator_indices,
    const std::string& variable = "gaugex");

LightlikeGaugeLinkFinitePartResult ExtractLightlikeGaugeLinkEndpointFinitePart(
    const std::vector<LightlikeGaugeLinkFinitePartTerm>& terms);

LightlikeGaugeLinkReducedFinitePartResult
EvaluateLightlikeGaugeLinkReducedFiniteParts(
    const std::vector<TargetIntegral>& targets,
    const std::vector<LightlikeGaugeLinkSixMasterEndpointTerms>& endpoint_terms,
    const std::vector<LightlikeGaugeLinkTargetReductionTerm>& target_reduction_terms,
    const std::string& variable = "gaugex");

std::vector<std::vector<std::string>> ParseLightlikeGaugeLinkDiffeqMatrixRaw(
    const std::string& diffeq_raw);

LightlikeGaugeLinkContourPlanAudit BuildLightlikeGaugeLinkContourPlanAudit(
    const std::vector<std::vector<std::string>>& diffeq_matrix,
    const std::vector<std::string>& epsilon_samples,
    const std::string& variable = "gaugex",
    const std::string& boundary_point = "gaugex -> 1/40",
    const std::string& target_point = "gaugex=0");

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkRetainedStateScaffold(
    const LightlikeGaugeLinkRuntimeState& state);

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkTransportScaffold(
    const ProblemSpec& spec,
    const LightlikeGaugeLinkRuntimeState& state);

LightlikeGaugeLinkSelectedCoefficientAudit
BuildLightlikeGaugeLinkFirstEndpointCoefficientAudit(
    const LightlikeGaugeLinkRuntimeState& state);

}  // namespace amflow
