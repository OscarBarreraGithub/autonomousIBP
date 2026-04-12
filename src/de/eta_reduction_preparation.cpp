#include "amflow/de/eta_reduction_preparation.hpp"

#include <stdexcept>

namespace amflow {

EtaGeneratedReductionPreparation PrepareEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::string& eta_symbol) {
  EtaGeneratedReductionPreparation preparation;
  preparation.auxiliary_family = ApplyEtaInsertion(spec, decision, eta_symbol);
  preparation.generated_variable =
      GenerateEtaDerivativeVariable(master_basis, preparation.auxiliary_family);
  if (preparation.generated_variable.reduction_targets.empty()) {
    throw std::runtime_error(
        "eta generated reduction preparation requires at least one generated reduction target");
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
