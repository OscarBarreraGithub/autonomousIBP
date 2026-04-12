#pragma once

#include <map>
#include <string>
#include <vector>

namespace amflow {

enum class PropagatorKind {
  Standard,
  Cut,
  Linear,
  Auxiliary
};

std::string ToString(PropagatorKind kind);

struct Propagator {
  std::string expression;
  std::string mass = "0";
  PropagatorKind kind = PropagatorKind::Standard;
  int prescription = -1;
};

struct ScalarProductRule {
  std::string left;
  std::string right;
};

struct TargetIntegral {
  std::string family;
  std::vector<int> indices;

  std::string Label() const;
};

struct FamilyDefinition {
  std::string name;
  std::vector<std::string> loop_momenta;
  std::vector<int> top_level_sectors;
  std::vector<Propagator> propagators;
  std::vector<std::string> preferred_masters;
};

struct Kinematics {
  std::vector<std::string> incoming_momenta;
  std::vector<std::string> outgoing_momenta;
  std::string momentum_conservation;
  std::vector<std::string> invariants;
  std::vector<ScalarProductRule> scalar_product_rules;
  std::map<std::string, std::string> numeric_substitutions;
};

struct ProblemSpec {
  FamilyDefinition family;
  Kinematics kinematics;
  std::vector<TargetIntegral> targets;
  std::string dimension = "4 - 2*eps";
  bool complex_mode = false;
  std::string notes;
};

std::vector<std::string> ValidateProblemSpec(const ProblemSpec& spec);
std::string SerializeProblemSpecYaml(const ProblemSpec& spec);

}  // namespace amflow
