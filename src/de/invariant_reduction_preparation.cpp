#include "amflow/de/invariant_reduction_preparation.hpp"

#include <stdexcept>

namespace amflow {

InvariantGeneratedReductionPreparation PrepareInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout) {
  return PrepareInvariantGeneratedReduction(
      spec, master_basis, BuildInvariantDerivativeSeed(spec, invariant_name), options, layout);
}

InvariantGeneratedReductionPreparation PrepareInvariantGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const InvariantDerivativeSeed& seed,
    const ReductionOptions& options,
    const ArtifactLayout& layout) {
  if (seed.family != spec.family.name) {
    throw std::runtime_error(
        "invariant-generated reduction preparation requires seed.family to match "
        "spec.family.name");
  }
  if (seed.propagator_derivatives.size() != spec.family.propagators.size()) {
    throw std::runtime_error(
        "invariant-generated reduction preparation requires propagator derivative count to "
        "match spec.family.propagators size");
  }

  InvariantGeneratedReductionPreparation preparation;
  preparation.generated_variable = GenerateInvariantDerivativeVariable(master_basis, seed);
  if (preparation.generated_variable.reduction_targets.empty()) {
    throw std::runtime_error("invariant-generated reduction preparation requires at least one "
                             "generated reduction target");
  }

  KiraBackend backend;
  preparation.backend_preparation = backend.PrepareForTargets(
      spec, options, layout, preparation.generated_variable.reduction_targets);
  return preparation;
}

}  // namespace amflow
