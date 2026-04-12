#pragma once

#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/de/eta_derivative_generation.hpp"
#include "amflow/kira/kira_backend.hpp"

namespace amflow {

struct ReducedDerivativeRowBinding {
  TargetIntegral source_master;
  TargetIntegral reduced_target;
};

struct ReducedDerivativeVariableInput {
  DifferentiationVariable variable;
  std::vector<ReducedDerivativeRowBinding> row_bindings;
  ParsedReductionResult reduction_result;
};

DESystem AssembleReducedDESystem(
    const ParsedMasterList& master_basis,
    const std::vector<ReducedDerivativeVariableInput>& variable_inputs);

DESystem AssembleGeneratedDerivativeDESystem(
    const ParsedMasterList& master_basis,
    const std::vector<GeneratedDerivativeVariableReductionInput>& variable_inputs);

}  // namespace amflow
