#pragma once

#include <cstddef>
#include <string>

#include "amflow/kira/kira_backend.hpp"
#include "amflow/runtime/auxiliary_family.hpp"

namespace amflow {

struct LightlikeLinearAuxiliaryReductionPreparation {
  LightlikeLinearAuxiliaryTransformResult auxiliary_family;
  BackendPreparation backend_preparation;
};

LightlikeLinearAuxiliaryReductionPreparation PrepareReviewedLightlikeLinearAuxiliaryReduction(
    const ProblemSpec& spec,
    std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::string& x_symbol = "x");

}  // namespace amflow
