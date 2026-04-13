#include "amflow/runtime/auxiliary_family.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace amflow {

namespace {

std::string Trim(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(begin, end - begin);
}

bool ContainsInvariant(const std::vector<std::string>& invariants,
                       const std::string& value) {
  return std::find(invariants.begin(), invariants.end(), value) != invariants.end();
}

}  // namespace

AuxiliaryFamilyTransformResult ApplyEtaInsertion(const ProblemSpec& spec,
                                                const EtaInsertionDecision& decision,
                                                const std::string& eta_symbol) {
  if (decision.selected_propagator_indices.empty()) {
    throw std::runtime_error("eta insertion requires at least one selected propagator index");
  }
  if (Trim(eta_symbol).empty()) {
    throw std::runtime_error("eta insertion symbol must not be empty");
  }

  AuxiliaryFamilyTransformResult result;
  result.transformed_spec = spec;
  result.eta_symbol = eta_symbol;

  std::set<std::size_t> seen_indices;
  const std::size_t propagator_count = spec.family.propagators.size();
  for (const std::size_t index : decision.selected_propagator_indices) {
    if (index >= propagator_count) {
      throw std::runtime_error("eta insertion propagator index out of range: " +
                               std::to_string(index));
    }
    if (!seen_indices.insert(index).second) {
      throw std::runtime_error("duplicate eta insertion propagator index: " +
                               std::to_string(index));
    }

    const Propagator& original = spec.family.propagators[index];
    if (original.kind == PropagatorKind::Auxiliary) {
      throw std::runtime_error("eta insertion cannot target auxiliary propagator index: " +
                               std::to_string(index));
    }
    Propagator& transformed = result.transformed_spec.family.propagators[index];
    transformed.expression = "(" + original.expression + ") + " + eta_symbol;
    transformed.mass = Trim(original.mass);
    result.rewritten_propagator_indices.push_back(index);
  }

  if (!ContainsInvariant(result.transformed_spec.kinematics.invariants, eta_symbol)) {
    result.transformed_spec.kinematics.invariants.push_back(eta_symbol);
  }
  return result;
}

}  // namespace amflow
