#pragma once

#include <cstddef>
#include <string>

#include "amflow/de/lightlike_linear_derivative_generation.hpp"
#include "amflow/kira/kira_backend.hpp"

namespace amflow {

struct LightlikeLinearAuxiliaryDerivativeReductionPreparation {
  LightlikeLinearAuxiliaryTransformResult auxiliary_family;
  GeneratedDerivativeVariable generated_variable;
  BackendPreparation backend_preparation;
};

LightlikeLinearAuxiliaryDerivativeReductionPreparation
PrepareReviewedLightlikeLinearAuxiliaryDerivativeReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::string& x_symbol = "x");

}  // namespace amflow
