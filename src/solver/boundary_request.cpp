#include "amflow/solver/boundary_request.hpp"

#include "amflow/solver/series_solver.hpp"

namespace amflow {

SolveRequest AttachManualBoundaryConditions(
    const SolveRequest& request,
    const std::vector<BoundaryCondition>& explicit_boundary_conditions) {
  if (!request.boundary_conditions.empty()) {
    throw BoundaryUnsolvedError(
        "manual boundary conditions are already attached to this solve request");
  }

  ValidateManualBoundaryAttachment(request.system,
                                   request.boundary_requests,
                                   explicit_boundary_conditions,
                                   request.start_location);

  SolveRequest attached_request = request;
  attached_request.boundary_conditions = explicit_boundary_conditions;
  return attached_request;
}

}  // namespace amflow
