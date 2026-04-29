#include "amflow/solver/boundary_provider.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
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

class DeferredCutkoskyPhaseSpaceBoundaryProvider final : public BoundaryProvider {
 public:
  explicit DeferredCutkoskyPhaseSpaceBoundaryProvider(std::string strategy)
      : strategy_(std::move(strategy)) {}

  std::string Strategy() const override {
    return strategy_;
  }

  BoundaryCondition Provide(const DESystem&, const BoundaryRequest& request) const override {
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary values remain deferred for strategy " +
        strategy_ + " at " + DescribeBoundaryLocus(request) +
        "; automatic phase-space boundary-value generation remains unimplemented on this "
        "reviewed provider surface");
  }

 private:
  std::string strategy_;
};

class DeferredEtaInfinityBoundaryProvider final : public BoundaryProvider {
 public:
  explicit DeferredEtaInfinityBoundaryProvider(std::string strategy)
      : strategy_(std::move(strategy)) {}

  std::string Strategy() const override {
    return strategy_;
  }

  BoundaryCondition Provide(const DESystem&, const BoundaryRequest& request) const override {
    throw BoundaryUnsolvedError(
        "builtin eta->infinity boundary values remain deferred for strategy " +
        strategy_ + " at " + DescribeBoundaryLocus(request) +
        "; automatic eta->infinity boundary-value generation remains unimplemented on this "
        "reviewed provider surface");
  }

 private:
  std::string strategy_;
};

std::shared_ptr<BoundaryProvider> MakeDeferredEtaInfinityBoundaryProviderForStrategy(
    const std::string& strategy) {
  if (strategy.empty()) {
    throw std::invalid_argument(
        "deferred eta->infinity boundary provider strategy must not be empty");
  }
  return std::make_shared<DeferredEtaInfinityBoundaryProvider>(strategy);
}

std::shared_ptr<BoundaryProvider> MakeDeferredCutkoskyPhaseSpaceBoundaryProviderForStrategy(
    const std::string& strategy) {
  if (strategy.empty()) {
    throw std::invalid_argument(
        "deferred Cutkosky phase-space boundary provider strategy must not be empty");
  }
  return std::make_shared<DeferredCutkoskyPhaseSpaceBoundaryProvider>(strategy);
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

std::vector<std::shared_ptr<BoundaryProvider>>
MakeDeferredEtaInfinityBoundaryProviderRegistry() {
  return {
      MakeDeferredEtaInfinityBoundaryProviderForStrategy("builtin::eta->infinity"),
  };
}

std::vector<std::shared_ptr<BoundaryProvider>>
MakeDeferredCutkoskyPhaseSpaceBoundaryProviderRegistry() {
  return {
      MakeDeferredCutkoskyPhaseSpaceBoundaryProviderForStrategy(
          "builtin::cutkosky-phase-space"),
      MakeDeferredCutkoskyPhaseSpaceBoundaryProviderForStrategy(
          "builtin::cutkosky-phase-space::plus_i0"),
      MakeDeferredCutkoskyPhaseSpaceBoundaryProviderForStrategy(
          "builtin::cutkosky-phase-space::minus_i0"),
      MakeDeferredCutkoskyPhaseSpaceBoundaryProviderForStrategy(
          "builtin::cutkosky-phase-space::none"),
  };
}

}  // namespace amflow
