#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "amflow/runtime/b61n_coefficient_target_graph.hpp"
#include "amflow/runtime/complex_contour_propagator.hpp"

namespace amflow {

enum class B61nFiniteStartCoefficientSource {
  EtaInfinityCoefficientRecurrence,
  ReviewedSourceAnchorCoefficientStateTransport,
};

struct B61nFiniteStartCoefficientValue {
  B61nCoefficientNode node;
  ComplexContourNumber value;
  std::string provenance;
};

struct B61nFiniteStartAvailableCoefficient {
  B61nCoefficientNode node;
  B61nFiniteStartCoefficientSource source =
      B61nFiniteStartCoefficientSource::EtaInfinityCoefficientRecurrence;
  std::string provenance;
};

struct B61nFiniteStartCoefficientAudit {
  bool finite_start_coefficients_available = false;
  bool coefficient_data_materialized = false;
  bool finite_start_samples_present = false;
  bool populated_from_finite_start_samples = false;
  bool finite_start_samples_used = false;
  bool solve_vandermonde_fit_used = false;
  bool source_anchor_reverse_coefficient_transport_required = false;
  bool source_anchor_reverse_coefficient_transport_available = false;
  std::size_t required_node_count = 0;
  std::size_t available_node_count = 0;
  std::size_t missing_node_count = 0;
  std::size_t eta_infinity_recurrence_available_count = 0;
  std::size_t reviewed_source_anchor_transport_available_count = 0;
  int min_state_eps_order = 0;
  int max_state_eps_order = 0;
  std::vector<B61nCoefficientNode> required_nodes;
  std::vector<B61nFiniteStartAvailableCoefficient> available_nodes;
  std::vector<B61nCoefficientNode> missing_nodes;
  std::string finite_start_eta;
  std::string fingerprint;
  std::string summary;
};

struct B61nFiniteStartCoefficientData {
  int min_state_eps_order = 0;
  int max_state_eps_order = 0;
  std::vector<ComplexContourVector> coefficient_vectors;
  std::vector<B61nCoefficientNode> materialized_nodes;
  std::string fingerprint;
  std::string summary;
};

std::string B61nFiniteStartCoefficientSourceLabel(
    B61nFiniteStartCoefficientSource source);

B61nFiniteStartCoefficientAudit AuditB61nFiniteStartCoefficientData(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const std::vector<B61nFiniteStartCoefficientValue>& eta_infinity_coefficients,
    const std::vector<B61nFiniteStartCoefficientValue>&
        reviewed_source_anchor_coefficients,
    bool finite_start_samples_present,
    const std::string& finite_start_eta = {});

B61nFiniteStartCoefficientData BuildB61nFiniteStartCoefficientData(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const std::vector<B61nFiniteStartCoefficientValue>& eta_infinity_coefficients,
    const std::vector<B61nFiniteStartCoefficientValue>&
        reviewed_source_anchor_coefficients,
    bool finite_start_samples_present,
    const std::string& finite_start_eta = {});

}  // namespace amflow
