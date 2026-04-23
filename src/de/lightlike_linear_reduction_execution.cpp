#include "amflow/de/lightlike_linear_reduction_execution.hpp"

#include <stdexcept>

namespace amflow {

LightlikeLinearAuxiliaryReductionExecution RunReviewedLightlikeLinearAuxiliaryReduction(
    const ProblemSpec& spec,
    const std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& x_symbol) {
  LightlikeLinearAuxiliaryReductionExecution execution;
  execution.preparation = PrepareReviewedLightlikeLinearAuxiliaryReduction(
      spec, propagator_index, options, layout, x_symbol);

  KiraBackend backend;
  execution.execution_result = backend.ExecutePrepared(execution.preparation.backend_preparation,
                                                       layout,
                                                       kira_executable,
                                                       fermat_executable);
  if (!execution.execution_result.Succeeded()) {
    return execution;
  }

  if (execution.execution_result.working_directory.empty()) {
    throw std::runtime_error("successful lightlike-linear auxiliary reduction execution did not "
                             "record a working-directory artifact root");
  }

  execution.parsed_reduction_result = backend.ParseReductionResult(
      execution.execution_result.working_directory,
      execution.preparation.auxiliary_family.transformed_spec.family.name);
  return execution;
}

}  // namespace amflow
