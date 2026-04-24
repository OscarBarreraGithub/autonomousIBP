#include "amflow/de/lightlike_linear_derivative_reduction_preparation.hpp"

#include <stdexcept>

namespace amflow {

LightlikeLinearAuxiliaryDerivativeReductionPreparation
PrepareReviewedLightlikeLinearAuxiliaryDerivativeReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::string& x_symbol) {
  LightlikeLinearAuxiliaryDerivativeReductionPreparation preparation;
  preparation.auxiliary_family =
      ApplyReviewedLightlikeLinearAuxiliaryTransform(spec, propagator_index, x_symbol);
  preparation.generated_variable = GenerateReviewedLightlikeLinearAuxiliaryDerivativeVariable(
      master_basis, preparation.auxiliary_family);
  if (preparation.generated_variable.reduction_targets.empty()) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary derivative reduction preparation requires at "
        "least one generated reduction target");
  }

  KiraBackend backend;
  preparation.backend_preparation =
      backend.PrepareForTargets(preparation.auxiliary_family.transformed_spec,
                                options,
                                layout,
                                preparation.generated_variable.reduction_targets);
  return preparation;
}

}  // namespace amflow
