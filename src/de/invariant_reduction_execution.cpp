#include "amflow/de/invariant_reduction_execution.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

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

std::optional<std::string> ResolveProblemSpecExactDimension(
    const ProblemSpec& spec) {
  try {
    return EvaluateCoefficientExpression(spec.dimension, NumericEvaluationPoint{})
        .ToString();
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

void ApplyExactDimensionExecutionMode(
    BackendPreparation& preparation,
    const ArtifactLayout& layout,
    const std::optional<std::string>& exact_dimension_override) {
  preparation.command_arguments.clear();
  if (exact_dimension_override.has_value()) {
    preparation.command_arguments.push_back("-sd=" + *exact_dimension_override);
  }

  std::ostringstream command;
  command << "FERMATPATH=<fermat-executable> kira";
  for (const std::string& argument : preparation.command_arguments) {
    command << " " << argument;
  }
  command << " " << (layout.generated_config_dir / "jobs.yaml").string();
  preparation.commands = {command.str()};
}

void ApplyProblemSpecExactDimensionExecutionMode(BackendPreparation& preparation,
                                                 const ArtifactLayout& layout,
                                                 const ProblemSpec& spec) {
  ApplyExactDimensionExecutionMode(preparation,
                                   layout,
                                   ResolveProblemSpecExactDimension(spec));
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

std::string BatchContext(const InvariantGeneratedReductionBatchPreparation& preparation) {
  if (!preparation.generated_variables.empty()) {
    return VariableContext(preparation.generated_variables.front(), 0);
  }
  return "generated derivative batch";
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
  ThrowIfParsedMasterSetDrift(master_basis,
                              *execution.parsed_reduction_result,
                              VariableContext(execution.preparation.generated_variable, 0));

  GeneratedDerivativeVariableReductionInput variable_input;
  variable_input.generated_variable = execution.preparation.generated_variable;
  variable_input.reduction_result = *execution.parsed_reduction_result;
  execution.assembled_system = AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
  return execution;
}

DESystem BuildInvariantGeneratedDESystemFromBatchPreparation(
    const ParsedMasterList& master_basis,
    InvariantGeneratedReductionBatchPreparation preparation,
    const std::string& family_name,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  ParsedReductionResult parsed_reduction_result;
  if (preparation.reduction_result_override.has_value()) {
    parsed_reduction_result = *preparation.reduction_result_override;
  } else {
    KiraBackend backend;
    const CommandExecutionResult execution_result =
        backend.ExecutePrepared(preparation.backend_preparation,
                                layout,
                                kira_executable,
                                fermat_executable);
    if (!execution_result.Succeeded()) {
      std::ostringstream message;
      message << "automatic invariant DE construction requires successful reducer execution; "
              << "status=" << StatusToString(execution_result.status)
              << "; exit_code=" << execution_result.exit_code
              << "; stderr_log=" << execution_result.stderr_log_path.string();
      if (!execution_result.error_message.empty()) {
        message << "; error=" << execution_result.error_message;
      }
      throw std::runtime_error(message.str());
    }
    if (execution_result.working_directory.empty()) {
      throw std::runtime_error("successful invariant-generated reduction execution did not record "
                               "a working-directory artifact root");
    }
    parsed_reduction_result =
        backend.ParseReductionResult(execution_result.working_directory, family_name);
    ThrowIfParsedMasterSetDrift(master_basis, parsed_reduction_result, BatchContext(preparation));
  }

  std::vector<GeneratedDerivativeVariableReductionInput> variable_inputs;
  variable_inputs.reserve(preparation.generated_variables.size());
  for (auto& generated_variable : preparation.generated_variables) {
    GeneratedDerivativeVariableReductionInput variable_input;
    variable_input.generated_variable = std::move(generated_variable);
    variable_input.reduction_result = parsed_reduction_result;
    variable_inputs.push_back(std::move(variable_input));
  }

  return AssembleGeneratedDerivativeDESystem(master_basis, variable_inputs);
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

DESystem BuildInvariantGeneratedDESystemList(
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

  InvariantGeneratedReductionBatchPreparation preparation =
      PrepareInvariantGeneratedReductionList(spec, master_basis, invariant_names, options, layout);
  ApplyProblemSpecExactDimensionExecutionMode(preparation.backend_preparation, layout, spec);

  return BuildInvariantGeneratedDESystemFromBatchPreparation(master_basis,
                                                            std::move(preparation),
                                                            spec.family.name,
                                                            layout,
                                                            kira_executable,
                                                            fermat_executable);
}

InvariantGeneratedReductionExecution RunInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) {
  InvariantGeneratedReductionPreparation preparation =
      PrepareInvariantGeneratedReduction(spec, master_basis, invariant_name, options, layout);
  ApplyProblemSpecExactDimensionExecutionMode(preparation.backend_preparation, layout, spec);
  return ExecuteInvariantGeneratedReduction(
      master_basis,
      std::move(preparation),
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
  InvariantGeneratedReductionPreparation preparation =
      PrepareInvariantGeneratedReduction(spec, master_basis, seed, options, layout);
  ApplyProblemSpecExactDimensionExecutionMode(preparation.backend_preparation, layout, spec);
  return ExecuteInvariantGeneratedReduction(
      master_basis,
      std::move(preparation),
      spec.family.name,
      layout,
      kira_executable,
      fermat_executable);
}

}  // namespace amflow
