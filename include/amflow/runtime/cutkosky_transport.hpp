#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct CutkoskyResidueEndpointPole {
  std::string variable;
  std::string location;
  std::string classification;
  std::string source;
  bool on_physical_contour = false;
};

struct CutkoskyEtaContourWaypoint {
  std::string eta;
  std::string purpose;
};

struct CutkoskyResidueEndpointModel {
  bool parsed = false;
  std::string model_kind;
  std::string phase_space_parameterization;
  std::string physical_integration_domain;
  std::string kallen_discriminant;
  std::string contour_half_plane;
  std::string contour_fingerprint;
  std::string endpoint_local_model_kind;
  std::string ir_pole_classification;
  std::string eta_zero_selection_rule;
  std::string coefficient_gap;
  std::vector<std::string> residue_variables;
  std::vector<std::size_t> uncut_denominator_indices;
  std::vector<std::string> uncut_denominator_roles;
  std::vector<std::string> residue_factors;
  std::vector<CutkoskyResidueEndpointPole> endpoint_poles;
  std::vector<CutkoskyEtaContourWaypoint> eta_contour_waypoints;
};

struct CutkoskyEtaZeroTerm {
  std::string region_key = "integer";
  int eta_power = 0;
  int log_power = 0;
  std::string coefficient_label;
};

struct CutkoskyEtaZeroSelectionResult {
  bool success = false;
  std::string failure_code;
  std::string selected_coefficient_label;
  std::vector<std::string> dropped_singular_terms;
  std::string summary;
};

struct CutkoskyEtaZeroTransportAudit {
  bool reviewed_surface = false;
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  std::string family;
  std::string runtime_application;
  std::string transport_scope;
  std::string reviewed_endpoint_model;
  std::string residue_model_kind;
  std::string phase_space_parameterization;
  std::string physical_integration_domain;
  std::string contour_fingerprint;
  std::string endpoint_local_model_kind;
  std::string ir_pole_classification;
  std::vector<std::size_t> cut_propagator_indices;
  std::vector<int> cut_powers;
  std::vector<std::size_t> uncut_denominator_indices;
  std::vector<std::string> uncut_denominator_roles;
  std::vector<std::string> phase_volume_loop_momenta;
  std::size_t phase_volume_loop_count = 0;
  std::string provider_strategy;
  std::string eta_contour_direction;
  std::string cutkosky_prefactor;
  std::string endpoint_selection_rule;
  std::string eta_zero_selection_audit;
  std::string coefficient_gap;
  std::string failure_code_contract;
  std::string summary;
  std::vector<std::string> branch_ledger_entries;
  std::vector<std::string> residue_variables;
  std::vector<CutkoskyResidueEndpointPole> endpoint_poles;
  std::vector<CutkoskyEtaContourWaypoint> eta_contour_waypoints;
};

CutkoskyResidueEndpointModel BuildCutkoskyResidueEndpointModel(
    const ProblemSpec& spec);
std::vector<CutkoskyResidueEndpointPole> ExtractCutkoskyResidueEndpointPoles(
    const CutkoskyResidueEndpointModel& model);
std::vector<CutkoskyEtaContourWaypoint> PlanCutkoskyEtaZeroContour(
    const CutkoskyResidueEndpointModel& model);
CutkoskyEtaZeroSelectionResult PickCutkoskyEtaZeroTerm(
    const std::vector<CutkoskyEtaZeroTerm>& terms);
CutkoskyEtaZeroTransportAudit BuildCutkoskyEtaZeroTransportScaffold(
    const ProblemSpec& spec);

}  // namespace amflow
