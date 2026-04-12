#include "amflow/de/invariant_reduction_execution.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace amflow {

namespace {

std::string StatusToString(const CommandExecutionStatus status) {
  switch (status) {
    case CommandExecutionStatus::NotRun:
      return "not-run";
    case CommandExecutionStatus::Completed:
      return "completed";
    case CommandExecutionStatus::FailedToStart:
      return "failed-to-start";
    case CommandExecutionStatus::InvalidConfiguration:
      return "invalid-configuration";
    case CommandExecutionStatus::Signaled:
      return "signaled";
  }
  return "unknown";
}

InvariantGeneratedReductionExecution ExecuteInvariantGeneratedReduction(
    const ParsedMasterList& master_basis,
    InvariantGeneratedReductionPreparation preparation,
    const std::string& family_name,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  InvariantGeneratedReductionExecution execution;
  execution.preparation = std::move(preparation);

  KiraBackend backend;
  execution.execution_result = backend.ExecutePrepared(execution.preparation.backend_preparation,
                                                       layout,
                                                       kira_executable,
                                                       fermat_executable);
  if (!execution.execution_result.Succeeded()) {
    return execution;
  }

  if (execution.execution_result.working_directory.empty()) {
    throw std::runtime_error("successful invariant-generated reduction execution did not record "
                             "a working-directory artifact root");
  }

  execution.parsed_reduction_result =
      backend.ParseReductionResult(execution.execution_result.working_directory, family_name);

  GeneratedDerivativeVariableReductionInput variable_input;
  variable_input.generated_variable = execution.preparation.generated_variable;
  variable_input.reduction_result = *execution.parsed_reduction_result;
  execution.assembled_system =
      AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
  return execution;
}

}  // namespace

DESystem BuildInvariantGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  InvariantGeneratedReductionExecution execution =
      RunInvariantGeneratedReduction(spec,
                                     master_basis,
                                     invariant_name,
                                     options,
                                     layout,
                                     kira_executable,
                                     fermat_executable);
  if (!execution.execution_result.Succeeded()) {
    std::ostringstream message;
    message << "automatic invariant DE construction requires successful reducer execution; "
            << "status=" << StatusToString(execution.execution_result.status)
            << "; exit_code=" << execution.execution_result.exit_code
            << "; stderr_log=" << execution.execution_result.stderr_log_path.string();
    if (!execution.execution_result.error_message.empty()) {
      message << "; error=" << execution.execution_result.error_message;
    }
    throw std::runtime_error(message.str());
  }
  if (!execution.assembled_system.has_value()) {
    throw std::runtime_error(
        "automatic invariant DE construction completed without an assembled DESystem");
  }
  return *execution.assembled_system;
}

InvariantGeneratedReductionExecution RunInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  return ExecuteInvariantGeneratedReduction(
      master_basis,
      PrepareInvariantGeneratedReduction(spec, master_basis, invariant_name, options, layout),
      spec.family.name,
      layout,
      kira_executable,
      fermat_executable);
}

InvariantGeneratedReductionExecution RunInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const InvariantDerivativeSeed& seed,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  return ExecuteInvariantGeneratedReduction(
      master_basis,
      PrepareInvariantGeneratedReduction(spec, master_basis, seed, options, layout),
      spec.family.name,
      layout,
      kira_executable,
      fermat_executable);
}

}  // namespace amflow
