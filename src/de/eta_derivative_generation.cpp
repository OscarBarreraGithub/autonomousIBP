#include "amflow/de/eta_derivative_generation.hpp"

#include <set>
#include <stdexcept>
#include <string>

namespace amflow {

GeneratedDerivativeVariable GenerateEtaDerivativeVariable(
    const ParsedMasterList& master_basis,
    const AuxiliaryFamilyTransformResult& auxiliary_family) {
  const std::string family_name = auxiliary_family.transformed_spec.family.name;
  const std::size_t propagator_count =
      auxiliary_family.transformed_spec.family.propagators.size();

  if (master_basis.family != family_name) {
    throw std::runtime_error("eta derivative generation requires ParsedMasterList.family \"" +
                             family_name + "\", found \"" + master_basis.family + "\"");
  }

  GeneratedDerivativeVariable generated_variable;
  generated_variable.variable.name =
      auxiliary_family.eta_symbol.empty() ? "eta" : auxiliary_family.eta_symbol;
  generated_variable.variable.kind = DifferentiationVariableKind::Eta;

  std::set<std::string> seen_targets;
  generated_variable.rows.reserve(master_basis.masters.size());
  for (const auto& master : master_basis.masters) {
    if (master.family != family_name) {
      throw std::runtime_error("eta derivative generation requires master family \"" +
                               family_name + "\", found \"" + master.family + "\"");
    }
    if (master.indices.size() != propagator_count) {
      throw std::runtime_error("eta derivative generation requires master index count to match "
                               "the transformed family propagator count");
    }

    GeneratedDerivativeRow row;
    row.source_master = master;
    for (const std::size_t propagator_index :
         auxiliary_family.rewritten_propagator_indices) {
      if (propagator_index >= propagator_count) {
        throw std::runtime_error("eta derivative generation encountered out-of-range rewritten "
                                 "propagator index: " +
                                 std::to_string(propagator_index));
      }

      const int exponent = master.indices[propagator_index];
      if (exponent == 0) {
        continue;
      }

      GeneratedDerivativeTerm term;
      term.coefficient = std::to_string(-exponent);
      term.target.family = master.family;
      term.target.indices = master.indices;
      ++term.target.indices[propagator_index];
      row.terms.push_back(term);

      const std::string label = term.target.Label();
      if (seen_targets.insert(label).second) {
        generated_variable.reduction_targets.push_back(term.target);
      }
    }
    generated_variable.rows.push_back(std::move(row));
  }

  return generated_variable;
}

}  // namespace amflow
