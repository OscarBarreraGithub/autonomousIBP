#include "amflow/runtime/ending_scheme.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "amflow/core/options.hpp"
#include "amflow/runtime/boundary_generation.hpp"

namespace amflow {

namespace {

bool IsBuiltinEndingSchemeName(const std::string& name) {
  for (const auto& candidate : BuiltinEndingSchemes()) {
    if (candidate == name) {
      return true;
    }
  }
  return false;
}

std::string JoinIndices(const std::vector<std::size_t>& indices) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << indices[index];
  }
  stream << "]";
  return stream.str();
}

std::string DescribeCutComponents(
    const std::vector<CutkoskyPhaseSpaceCutComponent>& components) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < components.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << JoinIndices(components[index].cut_propagator_indices);
  }
  stream << "]";
  return stream.str();
}

std::vector<std::size_t> CollectCutPropagatorIndices(const ProblemSpec& spec) {
  std::vector<std::size_t> cut_indices;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    if (spec.family.propagators[index].kind == PropagatorKind::Cut) {
      cut_indices.push_back(index);
    }
  }
  return cut_indices;
}

std::string EtaInfinityTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::eta->infinity";
}

std::string CutkoskyPhaseSpaceTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::cutkosky-phase-space";
}

void ValidateTraditionEndingSurface(const ProblemSpec& spec) {
  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    return;
  }

  throw std::runtime_error("ending scheme Tradition only supports the current loop-only subset; "
                           "cut propagators are present at indices " +
                           JoinIndices(cut_indices));
}

void ValidateCutkoskyEndingSurface(const ProblemSpec& spec) {
  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw std::runtime_error("ending scheme Cutkosky requires at least one cut propagator on "
                             "the current reviewed phase-space subset");
  }

  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const PropagatorKind kind = spec.family.propagators[index].kind;
    if (kind == PropagatorKind::Standard || kind == PropagatorKind::Cut) {
      continue;
    }
    throw std::runtime_error("ending scheme Cutkosky only supports standard/cut propagators on "
                             "the current reviewed phase-space subset; propagator " +
                             std::to_string(index) + " has kind " + ToString(kind));
  }

  const CutkoskyPhaseSpaceTopology topology =
      AnalyzeCutkoskyPhaseSpaceCutTopology(spec.family);
  for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
    if (!support.loop_momenta.empty()) {
      continue;
    }
    throw std::runtime_error(
        "ending scheme Cutkosky requires cut propagator " +
        std::to_string(support.propagator_index) +
        " to carry declared loop-momentum support before emitting the reviewed phase-space "
        "terminal node; no declared loop momentum support found in expression \"" +
        spec.family.propagators[support.propagator_index].expression + "\"");
  }

  if (topology.cut_components.size() > 1) {
    throw std::runtime_error(
        "ending scheme Cutkosky requires a connected cut surface before emitting the reviewed "
        "phase-space terminal node; disconnected cut components: " +
        DescribeCutComponents(topology.cut_components));
  }
}

void ValidateUserDefinedEndingSchemeRegistry(
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  std::vector<std::string> seen_names;
  seen_names.reserve(user_defined_schemes.size());

  for (const auto& user_defined_scheme : user_defined_schemes) {
    if (!user_defined_scheme) {
      throw std::invalid_argument("user-defined ending scheme registry contains null entry");
    }

    const std::string scheme_name = user_defined_scheme->Name();
    if (IsBuiltinEndingSchemeName(scheme_name)) {
      throw std::invalid_argument("user-defined ending scheme conflicts with builtin ending "
                                  "scheme: " +
                                  scheme_name);
    }

    if (std::find(seen_names.begin(), seen_names.end(), scheme_name) != seen_names.end()) {
      throw std::invalid_argument("duplicate user-defined ending scheme: " + scheme_name);
    }
    seen_names.push_back(scheme_name);
  }
}

EndingDecision SelectEndingSchemeDecision(
    const ProblemSpec& spec,
    const std::vector<std::string>& ending_scheme_names,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  if (ending_scheme_names.empty()) {
    throw std::invalid_argument("ending-scheme list must not be empty");
  }

  for (std::size_t index = 0; index < ending_scheme_names.size(); ++index) {
    const std::shared_ptr<EndingScheme> ending_scheme =
        ResolveEndingScheme(ending_scheme_names[index], user_defined_schemes);
    try {
      return ending_scheme->Plan(spec);
    } catch (const std::exception&) {
      if (index + 1 == ending_scheme_names.size()) {
        throw;
      }
    }
  }

  throw std::runtime_error("failed to select an ending scheme");
}

class BuiltinEndingScheme final : public EndingScheme {
 public:
  explicit BuiltinEndingScheme(std::string name) : name_(std::move(name)) {}

  std::string Name() const override { return name_; }

  EndingDecision Plan(const ProblemSpec& spec) const override {
    EndingDecision decision;
    decision.terminal_strategy = name_;
    if (name_ == "Tradition") {
      ValidateTraditionEndingSurface(spec);
      decision.terminal_nodes.push_back(EtaInfinityTerminalNode(spec));
      return decision;
    }
    if (name_ == "Cutkosky") {
      ValidateCutkoskyEndingSurface(spec);
      decision.terminal_nodes.push_back(CutkoskyPhaseSpaceTerminalNode(spec));
      return decision;
    }

    decision.terminal_nodes.push_back(EtaInfinityTerminalNode(spec));
    if (name_ == "Trivial") {
      decision.terminal_nodes.push_back(spec.family.name + "::trivial-region");
    }
    return decision;
  }

 private:
  std::string name_;
};

}  // namespace

std::vector<std::string> BuiltinEndingSchemes() {
  return {"Tradition", "Cutkosky", "SingleMass", "Trivial"};
}

std::shared_ptr<EndingScheme> MakeBuiltinEndingScheme(const std::string& name) {
  for (const auto& candidate : BuiltinEndingSchemes()) {
    if (candidate == name) {
      return std::make_shared<BuiltinEndingScheme>(name);
    }
  }
  throw std::invalid_argument("unknown ending scheme: " + name);
}

std::shared_ptr<EndingScheme> ResolveEndingScheme(
    const std::string& name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  ValidateUserDefinedEndingSchemeRegistry(user_defined_schemes);

  for (const auto& user_defined_scheme : user_defined_schemes) {
    if (user_defined_scheme->Name() == name) {
      return user_defined_scheme;
    }
  }

  return MakeBuiltinEndingScheme(name);
}

EndingDecision PlanEndingScheme(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  const std::shared_ptr<EndingScheme> ending_scheme =
      ResolveEndingScheme(ending_scheme_name, user_defined_schemes);
  return ending_scheme->Plan(spec);
}

EndingDecision PlanEndingSchemeList(
    const ProblemSpec& spec,
    const std::vector<std::string>& ending_scheme_names,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  return SelectEndingSchemeDecision(spec, ending_scheme_names, user_defined_schemes);
}

EndingDecision PlanAmfOptionsEndingScheme(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  return PlanEndingSchemeList(spec,
                              amf_options.ending_schemes,
                              user_defined_schemes);
}

}  // namespace amflow
