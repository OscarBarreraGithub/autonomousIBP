#include "amflow/de/eta_reduction_execution.hpp"

#include <sstream>
#include <stdexcept>

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

}  // namespace

DESystem BuildEtaGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol) {
  EtaGeneratedReductionExecution execution =
      RunEtaGeneratedReduction(spec,
                               master_basis,
                               decision,
                               options,
                               layout,
                               kira_executable,
                               fermat_executable,
                               eta_symbol);
  if (!execution.execution_result.Succeeded()) {
    std::ostringstream message;
    message << "eta-generated DE construction requires successful reducer execution; "
            << "status=" << StatusToString(execution.execution_result.status)
            << "; exit_code=" << execution.execution_result.exit_code
            << "; stderr_log=" << execution.execution_result.stderr_log_path.string();
    if (!execution.execution_result.error_message.empty()) {
      message << "; error=" << execution.execution_result.error_message;
    }
    throw std::runtime_error(message.str());
  }
  if (!execution.assembled_system.has_value()) {
    throw std::runtime_error("eta-generated DE construction completed without an assembled "
                             "DESystem");
  }
  return *execution.assembled_system;
}

EtaGeneratedReductionExecution RunEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol) {
  EtaGeneratedReductionExecution execution;
  execution.preparation =
      PrepareEtaGeneratedReduction(spec, master_basis, decision, options, layout, eta_symbol);

  KiraBackend backend;
  execution.execution_result = backend.ExecutePrepared(execution.preparation.backend_preparation,
                                                       layout,
                                                       kira_executable,
                                                       fermat_executable);
  if (!execution.execution_result.Succeeded()) {
    return execution;
  }

  if (execution.execution_result.working_directory.empty()) {
    throw std::runtime_error("successful eta-generated reduction execution did not record a "
                             "working-directory artifact root");
  }

  execution.parsed_reduction_result =
      backend.ParseReductionResult(execution.execution_result.working_directory,
                                   execution.preparation.auxiliary_family.transformed_spec.family
                                       .name);

  GeneratedDerivativeVariableReductionInput variable_input;
  variable_input.generated_variable = execution.preparation.generated_variable;
  variable_input.reduction_result = *execution.parsed_reduction_result;
  execution.assembled_system =
      AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
  return execution;
}

}  // namespace amflow
