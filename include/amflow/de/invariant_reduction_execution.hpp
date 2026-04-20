#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "amflow/de/invariant_reduction_preparation.hpp"
#include "amflow/de/reduction_assembly.hpp"

namespace amflow {

struct InvariantGeneratedReductionExecution {
  InvariantGeneratedReductionPreparation preparation;
  CommandExecutionResult execution_result;
  std::optional<ParsedReductionResult> parsed_reduction_result;
  std::optional<DESystem> assembled_system;
};

DESystem BuildInvariantGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable);

DESystem BuildInvariantGeneratedDESystemList(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& invariant_names,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable);

InvariantGeneratedReductionExecution RunInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable);

InvariantGeneratedReductionExecution RunInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const InvariantDerivativeSeed& seed,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable);

}  // namespace amflow
