#pragma once

#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"
#include "amflow/de/eta_derivative_generation.hpp"

namespace amflow {

struct InvariantPropagatorDerivativeTerm {
  std::string coefficient = "1";
  std::vector<int> factor_indices;
};

struct InvariantPropagatorDerivative {
  std::vector<InvariantPropagatorDerivativeTerm> terms;
};

struct InvariantDerivativeSeed {
  std::string family;
  DifferentiationVariable variable;
  std::vector<InvariantPropagatorDerivative> propagator_derivatives;
};

InvariantDerivativeSeed BuildInvariantDerivativeSeed(const ProblemSpec& spec,
                                                     const std::string& invariant_name);

GeneratedDerivativeVariable GenerateInvariantDerivativeVariable(
    const ParsedMasterList& master_basis,
    const InvariantDerivativeSeed& seed);

}  // namespace amflow
