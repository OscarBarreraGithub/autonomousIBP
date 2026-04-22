#pragma once

#include <memory>
#include <string>
#include <vector>

#include "amflow/core/boundary_data.hpp"

namespace amflow {

struct SolveRequest;

class BoundaryProvider {
 public:
  virtual ~BoundaryProvider() = default;

  virtual std::string Strategy() const = 0;
  virtual BoundaryCondition Provide(const DESystem& system,
                                    const BoundaryRequest& request) const = 0;
};

SolveRequest AttachBoundaryConditionsFromProvider(const SolveRequest& request,
                                                  const BoundaryProvider& provider);
SolveRequest AttachBoundaryConditionsFromProviderRegistry(
    const SolveRequest& request,
    const std::vector<std::shared_ptr<BoundaryProvider>>& providers);

}  // namespace amflow
