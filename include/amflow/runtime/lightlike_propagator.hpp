#pragma once

#include <cstddef>
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

struct LightlikeGaugeLinkTransportAudit {
  bool reviewed_surface = false;
  bool live_coefficients_available = false;
  bool runtime_scaffold_consumes_retained_solution_samples = false;
  bool retained_solution_samples_available = false;
  bool full_eta_zero_contour_applied = false;
  bool ir_subtraction_applied = false;
  bool boundary_data_available = false;
  bool diffeq_masters_cover_reduction_masters = false;
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
  std::vector<std::size_t> affected_propagator_indices;
  std::vector<std::string> generated_square_propagators;
  std::vector<LightlikeGaugeLinkTargetNormalization> target_normalizations;
  std::vector<std::string> pole_candidates;
  std::vector<std::string> boundary_file_names;
  std::size_t epsilon_sample_count = 0;
  std::size_t master_count = 0;
  std::size_t reduction_master_count = 0;
  std::size_t target_count = 0;
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

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkRetainedStateScaffold(
    const LightlikeGaugeLinkRuntimeState& state);

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkTransportScaffold(
    const ProblemSpec& spec,
    const LightlikeGaugeLinkRuntimeState& state);

}  // namespace amflow
