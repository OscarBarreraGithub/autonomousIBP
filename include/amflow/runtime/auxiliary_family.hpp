#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/eta_mode.hpp"

namespace amflow {

struct AuxiliaryFamilyTransformResult {
  ProblemSpec transformed_spec;
  std::string eta_symbol = "eta";
  std::vector<std::size_t> rewritten_propagator_indices;
};

Propagator BuildReviewedLightlikeLinearAuxiliaryPropagator(
    const ProblemSpec& spec,
    std::size_t propagator_index,
    const std::string& x_symbol = "x");

AuxiliaryFamilyTransformResult ApplyEtaInsertion(
    const ProblemSpec& spec,
    const EtaInsertionDecision& decision,
    const std::string& eta_symbol = "eta");

}  // namespace amflow
