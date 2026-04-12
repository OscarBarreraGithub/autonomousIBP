#pragma once

#include <string>

#include "amflow/de/invariant_derivative_generation.hpp"
#include "amflow/kira/kira_backend.hpp"

namespace amflow {

struct InvariantGeneratedReductionPreparation {
  GeneratedDerivativeVariable generated_variable;
  BackendPreparation backend_preparation;
};

InvariantGeneratedReductionPreparation PrepareInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout);

InvariantGeneratedReductionPreparation PrepareInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const InvariantDerivativeSeed& seed,
    const ReductionOptions& options,
    const ArtifactLayout& layout);

}  // namespace amflow
