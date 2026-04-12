#pragma once

#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/runtime/auxiliary_family.hpp"

namespace amflow {

struct GeneratedDerivativeTerm {
  std::string coefficient;
  TargetIntegral target;
};

struct GeneratedDerivativeRow {
  TargetIntegral source_master;
  std::vector<GeneratedDerivativeTerm> terms;
};

struct GeneratedDerivativeVariable {
  DifferentiationVariable variable;
  std::vector<GeneratedDerivativeRow> rows;
  std::vector<TargetIntegral> reduction_targets;
};

struct GeneratedDerivativeVariableReductionInput {
  GeneratedDerivativeVariable generated_variable;
  ParsedReductionResult reduction_result;
};

GeneratedDerivativeVariable GenerateEtaDerivativeVariable(
    const ParsedMasterList& master_basis,
    const AuxiliaryFamilyTransformResult& auxiliary_family);

}  // namespace amflow
