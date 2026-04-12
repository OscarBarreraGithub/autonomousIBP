#include "amflow/solver/boundary_provider.hpp"

#include <vector>

#include "amflow/solver/boundary_request.hpp"
#include "amflow/solver/series_solver.hpp"

namespace amflow {

namespace {

std::string DescribeBoundaryLocus(const BoundaryRequest& request) {
  return request.variable + " @ " + request.location;
}

std::vector<BoundaryCondition> MakePlaceholderBoundaryConditions(
    const SolveRequest& request) {
  std::vector<BoundaryCondition> explicit_conditions;
  explicit_conditions.reserve(request.boundary_requests.size());

  const std::vector<std::string> placeholder_values(request.system.masters.size(), "0");
  for (const auto& boundary_request : request.boundary_requests) {
    BoundaryCondition condition;
    condition.variable = boundary_request.variable;
    condition.location = boundary_request.location;
    condition.values = placeholder_values;
    condition.strategy = boundary_request.strategy;
    explicit_conditions.push_back(condition);
  }

  return explicit_conditions;
}

void ValidateBoundaryRequestsForProvider(const SolveRequest& request) {
  if (request.boundary_requests.empty()) {
    return;
  }

  ValidateManualBoundaryAttachment(request.system,
                                   request.boundary_requests,
                                   MakePlaceholderBoundaryConditions(request),
                                   request.start_location);
}

}  // namespace

SolveRequest AttachBoundaryConditionsFromProvider(const SolveRequest& request,
                                                  const BoundaryProvider& provider) {
  ValidateBoundaryRequestsForProvider(request);

  if (!request.boundary_conditions.empty()) {
    throw BoundaryUnsolvedError(
        "manual boundary conditions are already attached to this solve request");
  }

  if (request.boundary_requests.empty()) {
    return AttachManualBoundaryConditions(request, {});
  }

  const std::string provider_strategy = provider.Strategy();
  for (const auto& boundary_request : request.boundary_requests) {
    if (boundary_request.strategy != provider_strategy) {
      throw BoundaryUnsolvedError("boundary provider strategy does not match request for " +
                                  DescribeBoundaryLocus(boundary_request));
    }
  }

  std::vector<BoundaryCondition> explicit_conditions;
  explicit_conditions.reserve(request.boundary_requests.size());
  for (const auto& boundary_request : request.boundary_requests) {
    explicit_conditions.push_back(provider.Provide(request.system, boundary_request));
  }

  return AttachManualBoundaryConditions(request, explicit_conditions);
}

}  // namespace amflow
