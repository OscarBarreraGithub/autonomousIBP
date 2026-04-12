#include "amflow/core/problem_spec.hpp"

#include <sstream>

namespace amflow {

namespace {

std::string Quote(const std::string& value) {
  return "\"" + value + "\"";
}

std::string Indent(int depth) {
  return std::string(static_cast<std::size_t>(depth * 2), ' ');
}

std::string Join(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << Quote(values[index]);
  }
  return out.str();
}

}  // namespace

std::string ToString(PropagatorKind kind) {
  switch (kind) {
    case PropagatorKind::Standard:
      return "standard";
    case PropagatorKind::Cut:
      return "cut";
    case PropagatorKind::Linear:
      return "linear";
    case PropagatorKind::Auxiliary:
      return "auxiliary";
  }
  return "standard";
}

std::string TargetIntegral::Label() const {
  std::ostringstream out;
  out << family << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index > 0) {
      out << ",";
    }
    out << indices[index];
  }
  out << "]";
  return out.str();
}

std::vector<std::string> ValidateProblemSpec(const ProblemSpec& spec) {
  std::vector<std::string> messages;

  if (spec.family.name.empty()) {
    messages.emplace_back("family.name must not be empty");
  }
  if (spec.family.loop_momenta.empty()) {
    messages.emplace_back("family.loop_momenta must not be empty");
  }
  if (spec.family.propagators.empty()) {
    messages.emplace_back("family.propagators must not be empty");
  }
  if (spec.kinematics.invariants.empty()) {
    messages.emplace_back("kinematics.invariants must not be empty");
  }
  if (spec.targets.empty()) {
    messages.emplace_back("targets must not be empty");
  }

  for (const auto& target : spec.targets) {
    if (target.family != spec.family.name) {
      messages.emplace_back("target family does not match family.name: " + target.Label());
    }
  }

  return messages;
}

std::string SerializeProblemSpecYaml(const ProblemSpec& spec) {
  std::ostringstream out;
  out << "family:\n";
  out << Indent(1) << "name: " << Quote(spec.family.name) << "\n";
  out << Indent(1) << "loop_momenta: [" << Join(spec.family.loop_momenta) << "]\n";
  out << Indent(1) << "top_level_sectors: [";
  for (std::size_t index = 0; index < spec.family.top_level_sectors.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << spec.family.top_level_sectors[index];
  }
  out << "]\n";
  if (!spec.family.preferred_masters.empty()) {
    out << Indent(1) << "preferred_masters:\n";
    for (const auto& preferred : spec.family.preferred_masters) {
      out << Indent(2) << "- " << Quote(preferred) << "\n";
    }
  }
  out << Indent(1) << "propagators:\n";
  for (const auto& propagator : spec.family.propagators) {
    out << Indent(2) << "- expression: " << Quote(propagator.expression) << "\n";
    out << Indent(3) << "mass: " << Quote(propagator.mass) << "\n";
    out << Indent(3) << "kind: " << Quote(ToString(propagator.kind)) << "\n";
    out << Indent(3) << "prescription: " << propagator.prescription << "\n";
  }

  out << "kinematics:\n";
  out << Indent(1) << "incoming_momenta: [" << Join(spec.kinematics.incoming_momenta) << "]\n";
  out << Indent(1) << "outgoing_momenta: [" << Join(spec.kinematics.outgoing_momenta) << "]\n";
  out << Indent(1) << "momentum_conservation: " << Quote(spec.kinematics.momentum_conservation) << "\n";
  out << Indent(1) << "invariants: [" << Join(spec.kinematics.invariants) << "]\n";
  if (!spec.kinematics.scalar_product_rules.empty()) {
    out << Indent(1) << "scalar_product_rules:\n";
    for (const auto& rule : spec.kinematics.scalar_product_rules) {
      out << Indent(2) << "- left: " << Quote(rule.left) << "\n";
      out << Indent(3) << "right: " << Quote(rule.right) << "\n";
    }
  }
  if (!spec.kinematics.numeric_substitutions.empty()) {
    out << Indent(1) << "numeric_substitutions:\n";
    for (const auto& [name, value] : spec.kinematics.numeric_substitutions) {
      out << Indent(2) << name << ": " << Quote(value) << "\n";
    }
  }

  out << "targets:\n";
  for (const auto& target : spec.targets) {
    out << Indent(1) << "- family: " << Quote(target.family) << "\n";
    out << Indent(2) << "indices: [";
    for (std::size_t index = 0; index < target.indices.size(); ++index) {
      if (index > 0) {
        out << ", ";
      }
      out << target.indices[index];
    }
    out << "]\n";
  }

  out << "dimension: " << Quote(spec.dimension) << "\n";
  out << "complex_mode: " << (spec.complex_mode ? "true" : "false") << "\n";
  if (!spec.notes.empty()) {
    out << "notes: " << Quote(spec.notes) << "\n";
  }

  return out.str();
}

}  // namespace amflow
