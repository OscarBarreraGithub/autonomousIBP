#include "amflow/runtime/eta_mode.hpp"

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace amflow {

namespace {

bool IsBuiltinEtaModeName(const std::string& name) {
  for (const auto& candidate : BuiltinEtaModes()) {
    if (candidate == name) {
      return true;
    }
  }
  return false;
}

void ValidateUserDefinedEtaModeRegistry(
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes) {
  std::vector<std::string> seen_names;
  seen_names.reserve(user_defined_modes.size());

  for (const auto& user_defined_mode : user_defined_modes) {
    if (!user_defined_mode) {
      throw std::invalid_argument("user-defined eta mode registry contains null entry");
    }

    const std::string mode_name = user_defined_mode->Name();
    if (IsBuiltinEtaModeName(mode_name)) {
      throw std::invalid_argument("user-defined eta mode conflicts with builtin eta mode: " +
                                  mode_name);
    }

    if (std::find(seen_names.begin(), seen_names.end(), mode_name) != seen_names.end()) {
      throw std::invalid_argument("duplicate user-defined eta mode: " + mode_name);
    }
    seen_names.push_back(mode_name);
  }
}

class BuiltinEtaMode final : public EtaMode {
 public:
  explicit BuiltinEtaMode(std::string name) : name_(std::move(name)) {}

  std::string Name() const override { return name_; }

  EtaInsertionDecision Plan(const ProblemSpec& spec) const override {
    EtaInsertionDecision decision;
    decision.mode_name = name_;

    if (name_ == "All") {
      for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
        const auto& propagator = spec.family.propagators[index];
        if (propagator.kind == PropagatorKind::Auxiliary) {
          continue;
        }
        decision.selected_propagator_indices.push_back(index);
        decision.selected_propagators.push_back(propagator.expression);
      }
      if (decision.selected_propagator_indices.empty()) {
        throw std::runtime_error("eta mode All found no non-auxiliary propagators in bootstrap");
      }
      std::ostringstream explanation;
      explanation << "Bootstrap planner selected "
                  << decision.selected_propagator_indices.size()
                  << " non-auxiliary propagators for mode All";
      decision.explanation = explanation.str();
      return decision;
    }

    throw std::runtime_error("eta mode " + name_ + " is not implemented in bootstrap");
  }

 private:
  std::string name_;
};

}  // namespace

std::vector<std::string> BuiltinEtaModes() {
  return {"Prescription", "Mass", "Propagator", "Branch", "Loop", "All"};
}

std::shared_ptr<EtaMode> MakeBuiltinEtaMode(const std::string& name) {
  for (const auto& candidate : BuiltinEtaModes()) {
    if (candidate == name) {
      return std::make_shared<BuiltinEtaMode>(name);
    }
  }
  throw std::invalid_argument("unknown eta mode: " + name);
}

std::shared_ptr<EtaMode> ResolveEtaMode(
    const std::string& name,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes) {
  ValidateUserDefinedEtaModeRegistry(user_defined_modes);

  for (const auto& user_defined_mode : user_defined_modes) {
    if (user_defined_mode->Name() == name) {
      return user_defined_mode;
    }
  }

  return MakeBuiltinEtaMode(name);
}

}  // namespace amflow
