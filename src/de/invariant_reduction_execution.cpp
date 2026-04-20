#include "amflow/de/invariant_reduction_execution.hpp"

#include <iomanip>
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

std::string SanitizeInvariantLayoutComponent(const std::string& invariant_name) {
  std::string sanitized;
  sanitized.reserve(invariant_name.size());
  for (const unsigned char current : invariant_name) {
    const bool safe = (current >= 'a' && current <= 'z') || (current >= 'A' && current <= 'Z') ||
                      (current >= '0' && current <= '9') || current == '-' || current == '_';
    sanitized.push_back(safe ? static_cast<char>(current) : '-');
  }
  if (sanitized.empty()) {
    return "invariant";
  }
  return sanitized;
}

ArtifactLayout MakeInvariantIterationLayout(const ArtifactLayout& parent,
                                            const std::size_t ordinal,
                                            const std::string& invariant_name) {
  std::ostringstream label;
  label << "invariant-" << std::setw(4) << std::setfill('0') << (ordinal + 1) << "-"
        << SanitizeInvariantLayoutComponent(invariant_name);
  return EnsureArtifactLayout(parent.root / label.str());
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

std::vector<DESystem> BuildInvariantGeneratedDESystemList(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& invariant_names,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  if (invariant_names.empty()) {
    throw std::runtime_error(
        "automatic invariant DE construction list requires at least one invariant name");
  }

  std::vector<DESystem> systems;
  systems.reserve(invariant_names.size());
  for (std::size_t index = 0; index < invariant_names.size(); ++index) {
    systems.push_back(BuildInvariantGeneratedDESystem(spec,
                                                     master_basis,
                                                     invariant_names[index],
                                                     options,
                                                     MakeInvariantIterationLayout(
                                                         layout, index, invariant_names[index]),
                                                     kira_executable,
                                                     fermat_executable));
  }
  return systems;
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
