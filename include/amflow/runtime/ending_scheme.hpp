#pragma once

#include <memory>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct AmfOptions;

struct EndingDecision {
  std::string terminal_strategy;
  std::vector<std::string> terminal_nodes;
};

class EndingScheme {
 public:
  virtual ~EndingScheme() = default;

  virtual std::string Name() const = 0;
  virtual EndingDecision Plan(const ProblemSpec& spec) const = 0;
};

std::vector<std::string> BuiltinEndingSchemes();
std::shared_ptr<EndingScheme> MakeBuiltinEndingScheme(const std::string& name);
std::shared_ptr<EndingScheme> ResolveEndingScheme(
    const std::string& name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes);
EndingDecision PlanEndingScheme(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes);
EndingDecision PlanEndingSchemeList(
    const ProblemSpec& spec,
    const std::vector<std::string>& ending_scheme_names,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes);
EndingDecision PlanAmfOptionsEndingScheme(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes);

}  // namespace amflow
