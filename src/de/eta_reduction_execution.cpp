#include "amflow/de/eta_reduction_execution.hpp"

#include <sstream>
#include <stdexcept>

#include "amflow/solver/coefficient_evaluator.hpp"

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

std::string VariableContext(const GeneratedDerivativeVariable& generated_variable,
                            const std::size_t index) {
  if (!generated_variable.variable.name.empty()) {
    return "variable \"" + generated_variable.variable.name + "\"";
  }
  return "variable[" + std::to_string(index) + "]";
}

std::optional<std::string> NormalizeExactDimensionOverride(
    const std::optional<std::string>& exact_dimension_override) {
  if (!exact_dimension_override.has_value()) {
    return std::nullopt;
  }

  try {
    return EvaluateCoefficientExpression(*exact_dimension_override, NumericEvaluationPoint{})
        .ToString();
  } catch (const std::exception&) {
    throw std::invalid_argument("eta-generated exact dimension override must evaluate exactly "
                                "without additional symbols");
  }
}

void ApplyExactDimensionOverride(BackendPreparation& preparation,
                                 const ArtifactLayout& layout,
                                 const std::optional<std::string>& exact_dimension_override) {
  const std::optional<std::string> normalized_override =
      NormalizeExactDimensionOverride(exact_dimension_override);
  preparation.command_arguments.clear();
  if (normalized_override.has_value()) {
    preparation.command_arguments.push_back("-sd=" + *normalized_override);
  }

  std::ostringstream command;
  command << "FERMATPATH=<fermat-executable> kira";
  for (const std::string& argument : preparation.command_arguments) {
    command << " " << argument;
  }
  command << " " << (layout.generated_config_dir / "jobs.yaml").string();
  preparation.commands = {command.str()};
}

void ThrowIfParsedMasterSetDrift(const ParsedMasterList& master_basis,
                                 const ParsedReductionResult& reduction_result,
                                 const std::string& context) {
  const ParsedMasterList& reduced_master_basis = reduction_result.master_list;
  if (reduced_master_basis.masters.size() != master_basis.masters.size()) {
    throw MasterSetInstabilityError(context +
                                    " reduction master basis size does not match assembly "
                                    "master basis");
  }

  for (std::size_t index = 0; index < master_basis.masters.size(); ++index) {
    const std::string expected_label = master_basis.masters[index].Label();
    const std::string actual_label = reduced_master_basis.masters[index].Label();
    if (actual_label != expected_label) {
      throw MasterSetInstabilityError(
          context + " reduction master basis does not match assembly master basis at position " +
          std::to_string(index) + ": expected " + expected_label + ", found " + actual_label);
    }
  }
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
  return BuildEtaGeneratedDESystem(spec,
                                   master_basis,
                                   decision,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   eta_symbol,
                                   std::nullopt);
}

DESystem BuildEtaGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  EtaGeneratedReductionExecution execution =
      RunEtaGeneratedReduction(spec,
                               master_basis,
                               decision,
                               options,
                               layout,
                               kira_executable,
                               fermat_executable,
                               eta_symbol,
                               exact_dimension_override);
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
  return RunEtaGeneratedReduction(spec,
                                  master_basis,
                                  decision,
                                  options,
                                  layout,
                                  kira_executable,
                                  fermat_executable,
                                  eta_symbol,
                                  std::nullopt);
}

EtaGeneratedReductionExecution RunEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  EtaGeneratedReductionExecution execution;
  execution.preparation =
      PrepareEtaGeneratedReduction(spec, master_basis, decision, options, layout, eta_symbol);
  ApplyExactDimensionOverride(
      execution.preparation.backend_preparation, layout, exact_dimension_override);

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
  ThrowIfParsedMasterSetDrift(master_basis,
                              *execution.parsed_reduction_result,
                              VariableContext(execution.preparation.generated_variable, 0));

  GeneratedDerivativeVariableReductionInput variable_input;
  variable_input.generated_variable = execution.preparation.generated_variable;
  variable_input.reduction_result = *execution.parsed_reduction_result;
  execution.assembled_system = AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
  return execution;
}

}  // namespace amflow
