#pragma once

#include "amflow/de/eta_derivative_generation.hpp"
#include "amflow/runtime/auxiliary_family.hpp"

namespace amflow {

GeneratedDerivativeVariable GenerateReviewedLightlikeLinearAuxiliaryDerivativeVariable(
    const ParsedMasterList& master_basis,
    const LightlikeLinearAuxiliaryTransformResult& auxiliary_family);

}  // namespace amflow
