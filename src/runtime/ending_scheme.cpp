#include "amflow/runtime/ending_scheme.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>

#include "amflow/core/options.hpp"

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
    decision.terminal_nodes.push_back(spec.family.name + "::eta->infinity");
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
