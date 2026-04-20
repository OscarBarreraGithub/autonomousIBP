#pragma once

#include <optional>
#include <string>
#include <vector>

#include "amflow/de/invariant_derivative_generation.hpp"
#include "amflow/kira/kira_backend.hpp"

namespace amflow {

struct InvariantGeneratedReductionPreparation {
  GeneratedDerivativeVariable generated_variable;
  BackendPreparation backend_preparation;
};

struct InvariantGeneratedReductionBatchPreparation {
  std::vector<GeneratedDerivativeVariable> generated_variables;
  BackendPreparation backend_preparation;
  std::optional<ParsedReductionResult> reduction_result_override;
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

InvariantGeneratedReductionBatchPreparation PrepareInvariantGeneratedReductionList(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& invariant_names,
    const ReductionOptions& options,
    const ArtifactLayout& layout);

}  // namespace amflow
