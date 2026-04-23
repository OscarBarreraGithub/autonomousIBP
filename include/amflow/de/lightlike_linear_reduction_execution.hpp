#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "amflow/de/lightlike_linear_reduction_preparation.hpp"

namespace amflow {

struct LightlikeLinearAuxiliaryReductionExecution {
  LightlikeLinearAuxiliaryReductionPreparation preparation;
  CommandExecutionResult execution_result;
  std::optional<ParsedReductionResult> parsed_reduction_result;
};

LightlikeLinearAuxiliaryReductionExecution RunReviewedLightlikeLinearAuxiliaryReduction(
    const ProblemSpec& spec,
    std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& x_symbol = "x");

}  // namespace amflow
