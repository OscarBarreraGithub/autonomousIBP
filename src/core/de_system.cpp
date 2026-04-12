#include "amflow/core/de_system.hpp"

#include <set>
#include <sstream>

namespace amflow {

std::vector<std::string> ValidateDESystem(const DESystem& system) {
  std::vector<std::string> messages;
  std::set<std::string> declared_variables;

  if (system.masters.empty()) {
    messages.emplace_back("DESystem.masters must not be empty");
  }
  if (system.variables.empty()) {
    messages.emplace_back("DESystem.variables must not be empty");
  }
  for (const auto& variable : system.variables) {
    if (!declared_variables.insert(variable.name).second) {
      messages.emplace_back("duplicate differentiation variable name: " + variable.name);
    }

    const auto matrix_it = system.coefficient_matrices.find(variable.name);
    if (matrix_it == system.coefficient_matrices.end()) {
      messages.emplace_back("missing coefficient matrix for variable: " + variable.name);
      continue;
    }

    const auto& matrix = matrix_it->second;
    if (matrix.size() != system.masters.size()) {
      messages.emplace_back("coefficient matrix row count mismatch for variable: " +
                            variable.name);
    }
    for (std::size_t row_index = 0; row_index < matrix.size(); ++row_index) {
      if (matrix[row_index].size() != system.masters.size()) {
        messages.emplace_back("coefficient matrix column count mismatch for variable: " +
                              variable.name + " at row " + std::to_string(row_index));
      }
    }
  }
  for (const auto& [name, _] : system.coefficient_matrices) {
    if (declared_variables.find(name) == declared_variables.end()) {
      messages.emplace_back("extra coefficient matrix for undeclared variable: " + name);
    }
  }

  return messages;
}

std::string DescribeDESystem(const DESystem& system) {
  std::ostringstream out;
  out << "masters=" << system.masters.size()
      << ", variables=" << system.variables.size()
      << ", singular_points=" << system.singular_points.size();
  return out.str();
}

}  // namespace amflow
