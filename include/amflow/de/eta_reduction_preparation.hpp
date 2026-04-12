#pragma once

#include <string>

#include "amflow/de/eta_derivative_generation.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/runtime/auxiliary_family.hpp"
#include "amflow/runtime/eta_mode.hpp"

namespace amflow {

struct EtaGeneratedReductionPreparation {
  AuxiliaryFamilyTransformResult auxiliary_family;
  GeneratedDerivativeVariable generated_variable;
  BackendPreparation backend_preparation;
};

EtaGeneratedReductionPreparation PrepareEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::string& eta_symbol = "eta");

}  // namespace amflow
