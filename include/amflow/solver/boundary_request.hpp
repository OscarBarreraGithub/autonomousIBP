#pragma once

#include <vector>

#include "amflow/core/boundary_data.hpp"

namespace amflow {

struct SolveRequest;

SolveRequest AttachManualBoundaryConditions(
    const SolveRequest& request,
    const std::vector<BoundaryCondition>& explicit_boundary_conditions);

}  // namespace amflow
