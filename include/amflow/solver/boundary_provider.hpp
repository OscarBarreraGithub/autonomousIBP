#pragma once

#include <string>

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

}  // namespace amflow
