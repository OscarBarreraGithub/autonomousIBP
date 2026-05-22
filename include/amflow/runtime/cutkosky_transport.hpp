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

struct CutkoskyResidueComplexCoefficient {
  std::string real = "0";
  std::string imaginary = "0";
};

struct CutkoskyResiduePrecisionDiagnostics {
  int requested_precision_digits = 0;
  int working_precision_digits = 0;
  std::string arithmetic_backend;
  std::string summary;
};

struct CutkoskyResidueProvenance {
  std::string source;
  std::string derivation;
  std::string fixture_id;
  bool synthetic_fixture = false;
  bool retained_solution_samples_used = false;
  bool coefficient_published = false;
};

struct CutkoskyResidueSeriesTerm {
  int eps_order = 0;
  int eta_power = 0;
  int log_power = 0;
  std::string region_key = "integer";
  std::string coefficient_label;
  CutkoskyResidueComplexCoefficient coefficient;
  CutkoskyResiduePrecisionDiagnostics precision;
  CutkoskyResidueProvenance provenance;
};

struct CutkoskyResidueSeries {
  std::string series_label;
  std::string expansion_variable = "eps";
  std::string eta_variable = "eta";
  int min_eps_order = 0;
  int max_eps_order = 0;
  int requested_precision_digits = 0;
  int working_precision_digits = 0;
  std::string precision_diagnostics;
  std::vector<CutkoskyResidueSeriesTerm> terms;
};

struct CutkoskyPrefactorSeriesTerm {
  int eps_order = 0;
  std::string real;
  std::string imaginary = "0";
};

struct CutkoskyPrefactorSeries {
  std::size_t loop_count = 0;
  int min_eps_order = 0;
  int max_eps_order = 0;
  int requested_precision_digits = 0;
  int working_precision_digits = 0;
  std::string formula;
  std::string expansion_variable = "eps";
  std::string normalization_factor;
  std::string exponential_scale;
  std::string source_reference;
  std::string precision_diagnostics;
  std::vector<CutkoskyPrefactorSeriesTerm> terms;
};

struct CutkoskyBranchLedgerPrescription {
  std::string target;
  FeynmanPrescription prescription = FeynmanPrescription::None;
  std::string source;
};

struct CutkoskyBranchLedgerEntry {
  std::string summary;
  std::vector<CutkoskyBranchLedgerPrescription> prescriptions;
  std::vector<std::size_t> cut_support;
  std::string eta_half_plane;
  std::string branch_provenance;
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
  std::vector<CutkoskyBranchLedgerEntry> branch_ledger;
  std::vector<std::string> branch_ledger_entries;
  std::vector<std::string> residue_variables;
  std::vector<CutkoskyResidueEndpointPole> endpoint_poles;
  std::vector<CutkoskyEtaContourWaypoint> eta_contour_waypoints;
};

struct CutkoskyResidueCoefficientAudit {
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  std::string master_label;
  std::string runtime_application;
  std::string transport_scope;
  std::string residue_model_kind;
  std::string endpoint_local_model_kind;
  std::string contour_fingerprint;
  std::string extraction_fingerprint;
  std::string eta_zero_selection_audit;
  std::string summary;
};

struct CutkoskySymbolicIntegrandFactor {
  std::string denominator_id;
  std::size_t denominator_index;
  int propagator_power;
  std::string propagator_expression;
  std::string role;
  std::string structural_form;
};

struct CutkoskySymbolicIntegrand {
  std::string surface_label;
  std::string model_kind;
  std::string phase_space_parameterization;
  std::string physical_integration_domain;
  std::vector<std::string> residue_variables;
  std::vector<CutkoskySymbolicIntegrandFactor> factors;
  std::string coefficient_policy;
};

struct CutkoskySymbolicSubintegralFactor {
  std::string ledger_handle;
  std::string ledger_sign;
  std::string denominator_id;
  std::size_t denominator_index{};
  int propagator_power{};
  std::string propagator_expression;
  std::string role;
  std::string structural_form;
};

struct CutkoskySymbolicSubintegral {
  std::string ledger_handle;
  std::string loop_momentum;
  FeynmanPrescription prescription = FeynmanPrescription::None;
  std::string ledger_sign;
  std::string prescription_source;
  std::vector<CutkoskySymbolicSubintegralFactor> factors;
};

struct CutkoskySymbolicSubintegralAssembly {
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  std::string surface_label;
  std::string model_kind;
  std::string phase_space_parameterization;
  std::string physical_integration_domain;
  std::vector<std::size_t> cut_denominator_indices;
  std::vector<std::string> residue_variables;
  std::string branch_ledger_summary;
  std::vector<CutkoskySymbolicSubintegral> subintegrals;
  std::string coefficient_policy;
};

struct CutkoskyWeightedResidueEvaluationPlan {
  bool reviewed_surface = false;
  bool coefficient_free = false;
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  bool requires_moment_reduction = false;
  bool requires_branch_ledger_validation = false;
  bool requires_endpoint_laurent_series = false;
  bool requires_external_cas_validation = false;
  bool requires_feynman_conjugate_validation = false;
  std::string surface_label;
  std::string residue_model_kind;
  std::string conjugate_residue_model_kind;
  std::string phase_space_parameterization;
  std::string physical_integration_domain;
  std::vector<std::size_t> cut_denominator_indices;
  std::vector<std::size_t> uncut_denominator_indices;
  std::vector<std::string> uncut_denominator_roles;
  std::vector<std::string> residue_variables;
  std::string branch_ledger_summary;
  std::string coefficient_policy;
};

struct CutkoskyWeightedResidueMomentSeed {
  bool reviewed_surface = false;
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  std::string surface_label;
  std::string residue_model_kind;
  std::string selected_weight_denominator;
  std::size_t selected_weight_denominator_index = 0;
  int selected_weight_power = 0;
  std::string selected_weight_role;
  std::string selected_weight_structural_form;
  std::string coefficient_policy;
  std::string publication_gate_status;
  CutkoskyResidueSeries residue_series;
  CutkoskyEtaZeroSelectionResult eta_zero_selection;
};

CutkoskyResidueEndpointModel BuildCutkoskyResidueEndpointModel(
    const ProblemSpec& spec);
std::vector<CutkoskyResidueEndpointPole> ExtractCutkoskyResidueEndpointPoles(
    const CutkoskyResidueEndpointModel& model);
std::vector<CutkoskyEtaContourWaypoint> PlanCutkoskyEtaZeroContour(
    const CutkoskyResidueEndpointModel& model);
CutkoskyEtaZeroSelectionResult PickCutkoskyEtaZeroTerm(
    const std::vector<CutkoskyEtaZeroTerm>& terms);
// Reviewed AMFlow Cutkosky convention, see docs/theory/b63n-runtime-lane.md
// and AMFlow.m:941-950: K_r(eps)=2*(-1)^(r+1)*
// (Pi^(2-eps)*(2*Pi)^(2*eps-4))^r.
CutkoskyPrefactorSeries BuildCutkoskyPrefactorEpsilonSeries(
    std::size_t loop_count,
    int min_eps_order,
    int max_eps_order,
    int requested_precision_digits = 80);
std::vector<CutkoskyPrefactorSeriesTerm>
MultiplyCutkoskyPrefactorIntoLaurentSeries(
    const CutkoskyPrefactorSeries& prefactor,
    const std::vector<CutkoskyPrefactorSeriesTerm>& residue_series,
    int min_eps_order,
    int max_eps_order);
CutkoskyResidueSeries MultiplyCutkoskyPrefactorIntoResidueSeries(
    const CutkoskyPrefactorSeries& prefactor,
    const CutkoskyResidueSeries& residue_series,
    int min_eps_order,
    int max_eps_order);
std::vector<CutkoskyEtaZeroTerm> ProjectCutkoskyResidueSeriesToEtaZeroTerms(
    const CutkoskyResidueSeries& series,
    int eps_order);
void ValidateCutkoskyResiduePublicationGate(
    const CutkoskyResidueSeries& series);
std::string SerializeCutkoskyBranchLedgerEntrySummary(
    const CutkoskyBranchLedgerEntry& entry);
std::vector<std::string> SerializeCutkoskyBranchLedgerSummaries(
    const std::vector<CutkoskyBranchLedgerEntry>& ledger);
CutkoskyEtaZeroTransportAudit BuildCutkoskyEtaZeroTransportScaffold(
    const ProblemSpec& spec);
CutkoskyResidueCoefficientAudit BuildAutomaticPhaseSpaceFirstCutkoskyCoefficientAudit(
    const ProblemSpec& spec);
CutkoskySymbolicIntegrand BuildAutomaticPhaseSpaceSymbolicIntegrand(
    const ProblemSpec& spec);
std::string SerializeCutkoskySymbolicIntegrandAudit(
    const CutkoskySymbolicIntegrand& integrand);
CutkoskySymbolicSubintegralAssembly
BuildFeynmanPrescriptionSymbolicSubintegralAssembly(const ProblemSpec& spec);
std::string SerializeCutkoskySymbolicSubintegralAssemblyAudit(
    const CutkoskySymbolicSubintegralAssembly& assembly);
CutkoskyWeightedResidueEvaluationPlan
BuildCutkoskyWeightedResidueEvaluationPlan(const ProblemSpec& spec);
std::string SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
    const CutkoskyWeightedResidueEvaluationPlan& plan);
CutkoskyWeightedResidueMomentSeed
BuildAutomaticPhaseSpaceWeightedResidueMomentSeed(
    const ProblemSpec& spec,
    std::size_t uncut_denominator_index,
    int max_eps_order,
    int requested_precision_digits = 80);
std::string SerializeCutkoskyWeightedResidueMomentSeedAudit(
    const CutkoskyWeightedResidueMomentSeed& seed);

}  // namespace amflow
