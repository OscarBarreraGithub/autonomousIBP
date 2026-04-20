#include "amflow/de/invariant_reduction_preparation.hpp"

#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace amflow {

namespace {

constexpr const char* kEmptyGeneratedTargetsMessage =
    "invariant-generated reduction preparation requires at least one generated reduction "
    "target";

void ValidateInvariantGeneratedReductionSeed(const ProblemSpec& spec,
                                             const InvariantDerivativeSeed& seed) {
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
}

ParsedReductionResult MakeSyntheticReductionResult(const ParsedMasterList& master_basis) {
  ParsedReductionResult result;
  result.master_list = master_basis;
  result.explicit_rule_count = 0;
  result.status = ParsedReductionStatus::IdentityFallback;
  return result;
}

InvariantGeneratedReductionBatchPreparation PrepareInvariantGeneratedReductionBatch(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<InvariantDerivativeSeed>& seeds,
    const ReductionOptions& options,
    const ArtifactLayout& layout) {
  if (seeds.empty()) {
    throw std::runtime_error(
        "automatic invariant-generated reduction preparation list requires at least one "
        "invariant name");
  }

  InvariantGeneratedReductionBatchPreparation preparation;
  preparation.generated_variables.reserve(seeds.size());

  std::vector<TargetIntegral> reduction_targets;
  std::set<std::string> seen_targets;
  for (const auto& seed : seeds) {
    ValidateInvariantGeneratedReductionSeed(spec, seed);

    GeneratedDerivativeVariable generated_variable =
        GenerateInvariantDerivativeVariable(master_basis, seed);
    for (const auto& target : generated_variable.reduction_targets) {
      if (seen_targets.insert(target.Label()).second) {
        reduction_targets.push_back(target);
      }
    }
    preparation.generated_variables.push_back(std::move(generated_variable));
  }

  if (reduction_targets.empty()) {
    preparation.reduction_result_override = MakeSyntheticReductionResult(master_basis);
    return preparation;
  }

  KiraBackend backend;
  preparation.backend_preparation =
      backend.PrepareForTargets(spec, options, layout, reduction_targets);
  return preparation;
}

}  // namespace

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
  InvariantGeneratedReductionBatchPreparation batch_preparation =
      PrepareInvariantGeneratedReductionBatch(spec, master_basis, {seed}, options, layout);
  if (batch_preparation.generated_variables.front().reduction_targets.empty()) {
    throw std::runtime_error(kEmptyGeneratedTargetsMessage);
  }

  InvariantGeneratedReductionPreparation preparation;
  preparation.generated_variable = std::move(batch_preparation.generated_variables.front());
  preparation.backend_preparation = std::move(batch_preparation.backend_preparation);
  return preparation;
}

InvariantGeneratedReductionBatchPreparation PrepareInvariantGeneratedReductionList(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& invariant_names,
    const ReductionOptions& options,
    const ArtifactLayout& layout) {
  if (invariant_names.empty()) {
    throw std::runtime_error(
        "automatic invariant-generated reduction preparation list requires at least one "
        "invariant name");
  }

  std::vector<InvariantDerivativeSeed> seeds;
  seeds.reserve(invariant_names.size());
  for (const auto& invariant_name : invariant_names) {
    seeds.push_back(BuildInvariantDerivativeSeed(spec, invariant_name));
  }
  return PrepareInvariantGeneratedReductionBatch(spec, master_basis, seeds, options, layout);
}

}  // namespace amflow
