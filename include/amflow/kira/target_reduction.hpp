#pragma once

#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/solver/series_solver.hpp"

namespace amflow {

std::vector<std::vector<SolverDiagnostics::EpsilonCoefficient>>
ApplyParsedTargetReductionToEpsilonCoefficients(
    const ParsedReductionResult& reduction_result,
    const std::vector<TargetIntegral>& requested_targets,
    const std::vector<MasterIntegral>& available_masters,
    const std::vector<std::vector<SolverDiagnostics::EpsilonCoefficient>>&
        available_master_coefficients,
    const std::string& dimension_expression,
    int max_eps_order);

}  // namespace amflow
