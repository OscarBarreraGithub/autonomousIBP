#include "amflow/de/lightlike_linear_reduction_preparation.hpp"

namespace amflow {

LightlikeLinearAuxiliaryReductionPreparation PrepareReviewedLightlikeLinearAuxiliaryReduction(
    const ProblemSpec& spec,
    const std::size_t propagator_index,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::string& x_symbol) {
  LightlikeLinearAuxiliaryReductionPreparation preparation;
  preparation.auxiliary_family =
      ApplyReviewedLightlikeLinearAuxiliaryTransform(spec, propagator_index, x_symbol);

  KiraBackend backend;
  preparation.backend_preparation =
      backend.Prepare(preparation.auxiliary_family.transformed_spec, options, layout);
  return preparation;
}

}  // namespace amflow
