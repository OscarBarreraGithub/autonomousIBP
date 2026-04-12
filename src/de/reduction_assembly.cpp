#include "amflow/de/reduction_assembly.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace amflow {

namespace {

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

std::string VariableContext(const ReducedDerivativeVariableInput& input,
                            const std::size_t index) {
  if (!input.variable.name.empty()) {
    return "variable \"" + input.variable.name + "\"";
  }
  return "variable[" + std::to_string(index) + "]";
}

std::string VariableContext(const GeneratedDerivativeVariableReductionInput& input,
                            const std::size_t index) {
  if (!input.generated_variable.variable.name.empty()) {
    return "variable \"" + input.generated_variable.variable.name + "\"";
  }
  return "variable[" + std::to_string(index) + "]";
}

std::string RowContext(const std::string& variable_context, const std::size_t row_index) {
  return variable_context + " row " + std::to_string(row_index);
}

MasterIntegral ToMasterIntegral(const TargetIntegral& integral) {
  MasterIntegral master;
  master.family = integral.family;
  master.indices = integral.indices;
  master.label = integral.Label();
  return master;
}

std::map<std::string, std::size_t> BuildMasterIndex(const ParsedMasterList& master_basis) {
  std::map<std::string, std::size_t> index;
  for (std::size_t position = 0; position < master_basis.masters.size(); ++position) {
    const std::string label = master_basis.masters[position].Label();
    const auto [_, inserted] = index.emplace(label, position);
    if (!inserted) {
      throw std::runtime_error("DE assembly master basis contains duplicate label: " + label);
    }
  }
  return index;
}

void ValidateMatchingMasterBasis(const ParsedMasterList& expected,
                                 const ParsedMasterList& actual,
                                 const std::string& context) {
  if (actual.masters.size() != expected.masters.size()) {
    throw std::runtime_error(context + " reduction master basis size does not match assembly "
                             "master basis");
  }
  for (std::size_t index = 0; index < expected.masters.size(); ++index) {
    const std::string expected_label = expected.masters[index].Label();
    const std::string actual_label = actual.masters[index].Label();
    if (actual_label != expected_label) {
      throw std::runtime_error(context +
                               " reduction master basis does not match assembly master basis at "
                               "position " +
                               std::to_string(index) + ": expected " + expected_label +
                               ", found " + actual_label);
    }
  }
}

std::map<std::string, const ParsedReductionRule*> BuildRuleIndex(
    const ParsedReductionResult& reduction_result,
    const std::string& context) {
  if (reduction_result.explicit_rule_count > reduction_result.rules.size()) {
    throw std::runtime_error(context +
                             " reduction result reports explicit_rule_count beyond the "
                             "available rule list");
  }
  std::map<std::string, const ParsedReductionRule*> index;
  for (std::size_t rule_index = 0; rule_index < reduction_result.explicit_rule_count;
       ++rule_index) {
    const ParsedReductionRule& rule = reduction_result.rules[rule_index];
    const std::string label = rule.target.Label();
    const auto [_, inserted] = index.emplace(label, &rule);
    if (!inserted) {
      throw std::runtime_error(context +
                               " reduction result contains duplicate rules for target: " +
                               label);
    }
  }
  return index;
}

std::string ComposeProduct(const std::string& generated_coefficient,
                           const std::string& reduction_coefficient) {
  return "(" + generated_coefficient + ")*(" + reduction_coefficient + ")";
}

std::string JoinContributions(const std::vector<std::string>& contributions) {
  if (contributions.empty()) {
    return "0";
  }
  std::ostringstream out;
  for (std::size_t index = 0; index < contributions.size(); ++index) {
    if (index > 0) {
      out << " + ";
    }
    out << contributions[index];
  }
  return out.str();
}

}  // namespace

DESystem AssembleReducedDESystem(
    const ParsedMasterList& master_basis,
    const std::vector<ReducedDerivativeVariableInput>& variable_inputs) {
  if (master_basis.masters.empty()) {
    throw std::runtime_error("DE assembly master basis must not be empty");
  }
  if (variable_inputs.empty()) {
    throw std::runtime_error("DE assembly requires at least one variable input");
  }

  DESystem system;
  system.masters.reserve(master_basis.masters.size());
  for (const auto& master : master_basis.masters) {
    system.masters.push_back(ToMasterIntegral(master));
  }

  const std::map<std::string, std::size_t> master_index = BuildMasterIndex(master_basis);
  std::set<std::string> seen_variables;

  for (std::size_t variable_index = 0; variable_index < variable_inputs.size(); ++variable_index) {
    const ReducedDerivativeVariableInput& input = variable_inputs[variable_index];
    const std::string context = VariableContext(input, variable_index);

    if (!seen_variables.insert(input.variable.name).second) {
      throw std::runtime_error("DE assembly encountered duplicate differentiation variable "
                               "name: " +
                               input.variable.name);
    }
    if (input.row_bindings.size() != master_basis.masters.size()) {
      throw std::runtime_error(context +
                               " row binding count must match assembly master count");
    }

    ValidateMatchingMasterBasis(master_basis, input.reduction_result.master_list, context);
    const std::map<std::string, const ParsedReductionRule*> rule_index =
        BuildRuleIndex(input.reduction_result, context);

    std::set<std::string> seen_reduced_targets;
    std::vector<std::vector<std::string>> matrix;
    matrix.reserve(input.row_bindings.size());
    for (std::size_t row_index = 0; row_index < input.row_bindings.size(); ++row_index) {
      const ReducedDerivativeRowBinding& row_binding = input.row_bindings[row_index];
      const std::string row_context = RowContext(context, row_index);
      const std::string expected_source_label = master_basis.masters[row_index].Label();
      const std::string source_label = row_binding.source_master.Label();
      if (source_label != expected_source_label) {
        throw std::runtime_error(row_context +
                                 " source master does not match assembly master basis: expected " +
                                 expected_source_label + ", found " + source_label);
      }

      const std::string target_label = row_binding.reduced_target.Label();
      if (!seen_reduced_targets.insert(target_label).second) {
        throw std::runtime_error(context +
                                 " contains duplicate derivative target: " + target_label);
      }

      const auto rule_it = rule_index.find(target_label);
      if (rule_it == rule_index.end()) {
        throw std::runtime_error(context +
                                 " is missing a reduction rule for derivative target: " +
                                 target_label);
      }

      std::vector<std::string> row(master_basis.masters.size(), "0");
      std::set<std::string> seen_term_masters;
      for (const auto& term : rule_it->second->terms) {
        const std::string master_label = term.master.Label();
        const auto column_it = master_index.find(master_label);
        if (column_it == master_index.end()) {
          throw std::runtime_error(context +
                                   " reduction rule references a master outside the assembly "
                                   "basis: " +
                                   master_label);
        }
        if (!seen_term_masters.insert(master_label).second) {
          throw std::runtime_error(context +
                                   " reduction rule contains duplicate coefficients for "
                                   "master: " +
                                   master_label);
        }
        row[column_it->second] = term.coefficient;
      }
      matrix.push_back(std::move(row));
    }

    system.variables.push_back(input.variable);
    system.coefficient_matrices.emplace(input.variable.name, std::move(matrix));
  }

  const std::vector<std::string> validation_messages = ValidateDESystem(system);
  if (!validation_messages.empty()) {
    throw std::runtime_error("assembled DESystem is invalid: " +
                             JoinMessages(validation_messages));
  }

  return system;
}

DESystem AssembleGeneratedDerivativeDESystem(
    const ParsedMasterList& master_basis,
    const std::vector<GeneratedDerivativeVariableReductionInput>& variable_inputs) {
  if (master_basis.masters.empty()) {
    throw std::runtime_error("DE assembly master basis must not be empty");
  }
  if (variable_inputs.empty()) {
    throw std::runtime_error("DE assembly requires at least one variable input");
  }

  DESystem system;
  system.masters.reserve(master_basis.masters.size());
  for (const auto& master : master_basis.masters) {
    system.masters.push_back(ToMasterIntegral(master));
  }

  const std::map<std::string, std::size_t> master_index = BuildMasterIndex(master_basis);
  std::set<std::string> seen_variables;

  for (std::size_t variable_index = 0; variable_index < variable_inputs.size(); ++variable_index) {
    const GeneratedDerivativeVariableReductionInput& input = variable_inputs[variable_index];
    const std::string context = VariableContext(input, variable_index);

    if (!seen_variables.insert(input.generated_variable.variable.name).second) {
      throw std::runtime_error("DE assembly encountered duplicate differentiation variable "
                               "name: " +
                               input.generated_variable.variable.name);
    }
    if (input.generated_variable.rows.size() != master_basis.masters.size()) {
      throw std::runtime_error(context +
                               " generated row count must match assembly master count");
    }

    ValidateMatchingMasterBasis(master_basis, input.reduction_result.master_list, context);
    if (!input.generated_variable.reduction_targets.empty() &&
        input.reduction_result.explicit_rule_count == 0) {
      throw std::runtime_error(context + " requires explicit reduction rules for generated targets");
    }
    const std::map<std::string, const ParsedReductionRule*> rule_index =
        BuildRuleIndex(input.reduction_result, context);

    std::vector<std::vector<std::string>> matrix;
    matrix.reserve(input.generated_variable.rows.size());
    for (std::size_t row_index = 0; row_index < input.generated_variable.rows.size();
         ++row_index) {
      const GeneratedDerivativeRow& generated_row = input.generated_variable.rows[row_index];
      const std::string row_context = RowContext(context, row_index);
      const std::string expected_source_label = master_basis.masters[row_index].Label();
      const std::string source_label = generated_row.source_master.Label();
      if (source_label != expected_source_label) {
        throw std::runtime_error(row_context +
                                 " source master does not match assembly master basis: expected " +
                                 expected_source_label + ", found " + source_label);
      }

      std::vector<std::vector<std::string>> cell_contributions(master_basis.masters.size());
      for (const auto& generated_term : generated_row.terms) {
        const std::string target_label = generated_term.target.Label();
        const auto rule_it = rule_index.find(target_label);
        if (rule_it == rule_index.end()) {
          throw std::runtime_error(context +
                                   " is missing a reduction rule for generated target: " +
                                   target_label);
        }

        for (const auto& reduction_term : rule_it->second->terms) {
          const std::string master_label = reduction_term.master.Label();
          const auto column_it = master_index.find(master_label);
          if (column_it == master_index.end()) {
            throw std::runtime_error(context +
                                     " reduction rule references a master outside the assembly "
                                     "basis: " +
                                     master_label);
          }
          cell_contributions[column_it->second].push_back(
              ComposeProduct(generated_term.coefficient, reduction_term.coefficient));
        }
      }

      std::vector<std::string> row(master_basis.masters.size(), "0");
      for (std::size_t column_index = 0; column_index < cell_contributions.size();
           ++column_index) {
        row[column_index] = JoinContributions(cell_contributions[column_index]);
      }
      matrix.push_back(std::move(row));
    }

    system.variables.push_back(input.generated_variable.variable);
    system.coefficient_matrices.emplace(input.generated_variable.variable.name,
                                        std::move(matrix));
  }

  const std::vector<std::string> validation_messages = ValidateDESystem(system);
  if (!validation_messages.empty()) {
    throw std::runtime_error("assembled DESystem is invalid: " +
                             JoinMessages(validation_messages));
  }

  return system;
}

}  // namespace amflow
