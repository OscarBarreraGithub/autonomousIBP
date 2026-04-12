#pragma once

#include <map>
#include <string>
#include <vector>

namespace amflow {

enum class DifferentiationVariableKind {
  Eta,
  Invariant,
  Dimension,
  Auxiliary
};

struct MasterIntegral {
  std::string family;
  std::vector<int> indices;
  std::string label;
};

struct DifferentiationVariable {
  std::string name;
  DifferentiationVariableKind kind = DifferentiationVariableKind::Invariant;
};

struct DESystem {
  std::vector<MasterIntegral> masters;
  std::vector<DifferentiationVariable> variables;
  std::map<std::string, std::vector<std::vector<std::string>>> coefficient_matrices;
  std::vector<std::string> singular_points;
};

std::vector<std::string> ValidateDESystem(const DESystem& system);
std::string DescribeDESystem(const DESystem& system);

}  // namespace amflow
