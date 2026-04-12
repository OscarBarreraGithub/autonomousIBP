#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "amflow/de/eta_reduction_preparation.hpp"
#include "amflow/de/reduction_assembly.hpp"

namespace amflow {

struct EtaGeneratedReductionExecution {
  EtaGeneratedReductionPreparation preparation;
  CommandExecutionResult execution_result;
  std::optional<ParsedReductionResult> parsed_reduction_result;
  std::optional<DESystem> assembled_system;
};

DESystem BuildEtaGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol = "eta");

EtaGeneratedReductionExecution RunEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol = "eta");

}  // namespace amflow
