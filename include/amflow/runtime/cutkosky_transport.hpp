#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct CutkoskyEtaZeroTransportAudit {
  bool reviewed_surface = false;
  bool live_coefficients_available = false;
  bool retained_solution_samples_used = false;
  std::string family;
  std::string reviewed_endpoint_model;
  std::vector<std::size_t> cut_propagator_indices;
  std::vector<int> cut_powers;
  std::vector<std::string> phase_volume_loop_momenta;
  std::size_t phase_volume_loop_count = 0;
  std::string provider_strategy;
  std::string eta_contour_direction;
  std::string cutkosky_prefactor;
  std::string endpoint_selection_rule;
  std::string coefficient_gap;
  std::vector<std::string> branch_ledger_entries;
};

CutkoskyEtaZeroTransportAudit BuildCutkoskyEtaZeroTransportScaffold(
    const ProblemSpec& spec);

}  // namespace amflow
