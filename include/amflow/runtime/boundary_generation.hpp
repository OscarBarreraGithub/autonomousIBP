#pragma once

#include <memory>
#include <string>
#include <vector>

#include "amflow/core/boundary_data.hpp"
#include "amflow/core/problem_spec.hpp"

namespace amflow {

class EndingScheme;

BoundaryRequest GenerateBuiltinEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& eta_symbol = "eta");
BoundaryRequest GeneratePlannedEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol = "eta");

}  // namespace amflow
