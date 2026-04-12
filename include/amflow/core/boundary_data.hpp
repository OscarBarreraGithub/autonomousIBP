#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"

namespace amflow {

struct BoundaryRequest {
  std::string variable;
  std::string location;
  std::string strategy;
};

struct BoundaryCondition {
  std::string variable;
  std::string location;
  std::vector<std::string> values;
  std::string strategy;
};

class BoundaryUnsolvedError : public std::runtime_error {
 public:
  explicit BoundaryUnsolvedError(const std::string& message);

  const char* failure_code() const noexcept;
};

std::vector<std::string> ValidateBoundaryRequest(const DESystem& system,
                                                 const BoundaryRequest& request);
std::vector<std::string> ValidateBoundaryCondition(const BoundaryCondition& condition);
void ValidateManualBoundaryAttachment(const DESystem& system,
                                      const std::vector<BoundaryRequest>& boundary_requests,
                                      const std::vector<BoundaryCondition>& explicit_conditions,
                                      const std::string& start_location);

}  // namespace amflow
