#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct AmfOptions;

struct EtaInsertionDecision {
  std::string mode_name;
  std::vector<std::size_t> selected_propagator_indices;
  std::vector<std::string> selected_propagators;
  std::string explanation;
};

class EtaMode {
 public:
  virtual ~EtaMode() = default;

  virtual std::string Name() const = 0;
  virtual EtaInsertionDecision Plan(const ProblemSpec& spec) const = 0;
};

std::vector<std::string> BuiltinEtaModes();
std::shared_ptr<EtaMode> MakeBuiltinEtaMode(const std::string& name);
std::shared_ptr<EtaMode> ResolveEtaMode(
    const std::string& name,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes);
EtaInsertionDecision PlanAmfOptionsEtaMode(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes);

}  // namespace amflow
