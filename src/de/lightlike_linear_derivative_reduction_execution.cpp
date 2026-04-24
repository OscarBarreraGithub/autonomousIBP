#include "amflow/de/lightlike_linear_derivative_reduction_execution.hpp"

#include <sstream>
#include <stdexcept>

#include "amflow/de/reduction_assembly.hpp"

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

void ThrowIfParsedMasterSetDrift(const ParsedMasterList& master_basis,
                                 const ParsedReductionResult& reduction_result,
                                 const std::string& variable_name) {
  const std::string context =
      variable_name.empty() ? "variable[0]" : "variable \"" + variable_name + "\"";
  const ParsedMasterList& reduced_master_basis = reduction_result.master_list;
  if (reduced_master_basis.masters.size() != master_basis.masters.size()) {
    throw MasterSetInstabilityError(context + " reduction master basis size does not match "
                                    "assembly master basis");
  }

  for (std::size_t index = 0; index < master_basis.masters.size(); ++index) {
    const std::string expected_label = master_basis.masters[index].Label();
    const std::string actual_label = reduced_master_basis.masters[index].Label();
    if (actual_label != expected_label) {
      throw MasterSetInstabilityError(context +
                                      " reduction master basis does not match assembly master "
                                      "basis at position " +
                                      std::to_string(index) + ": expected " + expected_label +
                                      ", found " + actual_label);
    }
  }
}

}  // namespace

LightlikeLinearAuxiliaryDerivativeReductionExecution
RunReviewedLightlikeLinearAuxiliaryDerivativeReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& x_symbol) {
  LightlikeLinearAuxiliaryDerivativeReductionExecution execution;
  execution.preparation = PrepareReviewedLightlikeLinearAuxiliaryDerivativeReduction(
      spec, master_basis, propagator_index, options, layout, x_symbol);

  KiraBackend backend;
  execution.execution_result = backend.ExecutePrepared(execution.preparation.backend_preparation,
                                                       layout,
                                                       kira_executable,
                                                       fermat_executable);
  if (!execution.execution_result.Succeeded()) {
    return execution;
  }

  if (execution.execution_result.working_directory.empty()) {
    throw std::runtime_error(
        "successful lightlike-linear auxiliary derivative reduction execution did not record "
        "a working-directory artifact root");
  }

  execution.parsed_reduction_result = backend.ParseReductionResult(
      execution.execution_result.working_directory,
      execution.preparation.auxiliary_family.transformed_spec.family.name);
  return execution;
}

DESystem BuildReviewedLightlikeLinearAuxiliaryDerivativeDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& x_symbol) {
  const LightlikeLinearAuxiliaryDerivativeReductionExecution execution =
      RunReviewedLightlikeLinearAuxiliaryDerivativeReduction(spec,
                                                             master_basis,
                                                             propagator_index,
                                                             options,
                                                             layout,
                                                             kira_executable,
                                                             fermat_executable,
                                                             x_symbol);
  if (!execution.execution_result.Succeeded()) {
    std::ostringstream message;
    message << "reviewed lightlike-linear auxiliary derivative DE construction requires "
            << "successful reducer execution; status="
            << StatusToString(execution.execution_result.status)
            << "; exit_code=" << execution.execution_result.exit_code
            << "; stderr_log=" << execution.execution_result.stderr_log_path.string();
    if (!execution.execution_result.error_message.empty()) {
      message << "; error=" << execution.execution_result.error_message;
    }
    throw std::runtime_error(message.str());
  }
  if (!execution.parsed_reduction_result.has_value()) {
    throw std::runtime_error(
        "reviewed lightlike-linear auxiliary derivative DE construction completed without "
        "parsed reduction results");
  }

  ThrowIfParsedMasterSetDrift(master_basis,
                              *execution.parsed_reduction_result,
                              execution.preparation.generated_variable.variable.name);

  GeneratedDerivativeVariableReductionInput variable_input;
  variable_input.generated_variable = execution.preparation.generated_variable;
  variable_input.reduction_result = *execution.parsed_reduction_result;
  return AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
}

}  // namespace amflow
