#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "amflow/de/lightlike_linear_derivative_reduction_preparation.hpp"

namespace amflow {

struct LightlikeLinearAuxiliaryDerivativeReductionExecution {
  LightlikeLinearAuxiliaryDerivativeReductionPreparation preparation;
  CommandExecutionResult execution_result;
  std::optional<ParsedReductionResult> parsed_reduction_result;
};

LightlikeLinearAuxiliaryDerivativeReductionExecution
RunReviewedLightlikeLinearAuxiliaryDerivativeReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& x_symbol = "x");

}  // namespace amflow
