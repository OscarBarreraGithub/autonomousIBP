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

SolveRequest AttachBoundaryConditionsFromProviderRegistry(
    const SolveRequest& request,
    const std::vector<std::shared_ptr<BoundaryProvider>>& providers) {
  ValidateBoundaryRequestsForProvider(request);

  if (!request.boundary_conditions.empty()) {
    throw BoundaryUnsolvedError(
        "manual boundary conditions are already attached to this solve request");
  }

  if (request.boundary_requests.empty()) {
    return AttachManualBoundaryConditions(request, {});
  }

  std::vector<std::pair<std::string, const BoundaryProvider*>> registered_providers;
  registered_providers.reserve(providers.size());
  for (std::size_t index = 0; index < providers.size(); ++index) {
    if (!providers[index]) {
      throw std::invalid_argument("boundary provider registry entry " +
                                  std::to_string(index + 1) + " is null");
    }

    const std::string strategy = providers[index]->Strategy();
    for (const auto& entry : registered_providers) {
      if (entry.first == strategy) {
        throw std::invalid_argument("boundary provider registry contains duplicate strategy: " +
                                    strategy);
      }
    }
    registered_providers.push_back({strategy, providers[index].get()});
  }

  std::vector<BoundaryCondition> explicit_conditions;
  explicit_conditions.reserve(request.boundary_requests.size());
  for (const auto& boundary_request : request.boundary_requests) {
    const BoundaryProvider* matched_provider = nullptr;
    for (const auto& entry : registered_providers) {
      if (entry.first == boundary_request.strategy) {
        matched_provider = entry.second;
        break;
      }
    }

    if (!matched_provider) {
      throw BoundaryUnsolvedError("boundary provider registry does not contain strategy " +
                                  boundary_request.strategy + " for " +
                                  DescribeBoundaryLocus(boundary_request));
    }

    explicit_conditions.push_back(matched_provider->Provide(request.system, boundary_request));
  }

  return AttachManualBoundaryConditions(request, explicit_conditions);
}

}  // namespace amflow
