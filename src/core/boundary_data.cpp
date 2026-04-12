#include "amflow/core/boundary_data.hpp"

#include <map>
#include <set>
#include <sstream>

namespace amflow {

namespace {

std::string BoundaryKey(const std::string& variable, const std::string& location) {
  return variable + "\n" + location;
}

std::string DescribeBoundaryLocus(const std::string& variable, const std::string& location) {
  return variable + " @ " + location;
}

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index != 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

}  // namespace

BoundaryUnsolvedError::BoundaryUnsolvedError(const std::string& message)
    : std::runtime_error("boundary_unsolved: " + message) {}

const char* BoundaryUnsolvedError::failure_code() const noexcept {
  return "boundary_unsolved";
}

std::vector<std::string> ValidateBoundaryRequest(const DESystem& system,
                                                 const BoundaryRequest& request) {
  std::vector<std::string> messages = ValidateDESystem(system);
  std::set<std::string> declared_variables;
  for (const auto& variable : system.variables) {
    declared_variables.insert(variable.name);
  }

  if (request.variable.empty()) {
    messages.emplace_back("boundary request variable must not be empty");
  }
  if (request.location.empty()) {
    messages.emplace_back("boundary request location must not be empty");
  }
  if (request.strategy.empty()) {
    messages.emplace_back("boundary request strategy must not be empty");
  }
  if (!request.variable.empty() &&
      declared_variables.find(request.variable) == declared_variables.end()) {
    messages.emplace_back("boundary request variable is not declared in DE system: " +
                          request.variable);
  }

  return messages;
}

std::vector<std::string> ValidateBoundaryCondition(const BoundaryCondition& condition) {
  std::vector<std::string> messages;

  if (condition.variable.empty()) {
    messages.emplace_back("boundary condition variable must not be empty");
  }
  if (condition.location.empty()) {
    messages.emplace_back("boundary condition location must not be empty");
  }
  if (condition.strategy.empty()) {
    messages.emplace_back("boundary condition strategy must not be empty");
  }
  if (condition.values.empty()) {
    messages.emplace_back("boundary condition values must not be empty");
  }

  return messages;
}

void ValidateManualBoundaryAttachment(const DESystem& system,
                                      const std::vector<BoundaryRequest>& boundary_requests,
                                      const std::vector<BoundaryCondition>& explicit_conditions,
                                      const std::string& start_location) {
  if (boundary_requests.empty()) {
    if (!explicit_conditions.empty()) {
      throw BoundaryUnsolvedError("explicit boundary data provided without boundary requests");
    }
    return;
  }

  std::vector<std::string> request_messages;
  std::set<std::string> request_keys;
  bool request_covers_start_location = start_location.empty();
  for (const auto& boundary_request : boundary_requests) {
    const std::vector<std::string> messages = ValidateBoundaryRequest(system, boundary_request);
    request_messages.insert(request_messages.end(), messages.begin(), messages.end());

    const std::string key = BoundaryKey(boundary_request.variable, boundary_request.location);
    if (!request_keys.insert(key).second) {
      request_messages.emplace_back("duplicate boundary request: " +
                                    DescribeBoundaryLocus(boundary_request.variable,
                                                          boundary_request.location));
    }
    if (boundary_request.location == start_location) {
      request_covers_start_location = true;
    }
  }
  if (!request_messages.empty()) {
    throw BoundaryUnsolvedError(JoinMessages(request_messages));
  }
  if (!request_covers_start_location) {
    throw BoundaryUnsolvedError("no boundary request covers the solver start location: " +
                                start_location);
  }

  if (explicit_conditions.empty()) {
    throw BoundaryUnsolvedError("explicit boundary list must not be empty");
  }

  std::vector<std::string> condition_messages;
  std::map<std::string, const BoundaryCondition*> condition_by_key;
  bool explicit_covers_start_location = start_location.empty();
  for (const auto& condition : explicit_conditions) {
    const std::vector<std::string> messages = ValidateBoundaryCondition(condition);
    condition_messages.insert(condition_messages.end(), messages.begin(), messages.end());

    const std::string key = BoundaryKey(condition.variable, condition.location);
    if (!condition_by_key.emplace(key, &condition).second) {
      condition_messages.emplace_back("duplicate explicit boundary condition: " +
                                      DescribeBoundaryLocus(condition.variable,
                                                            condition.location));
    }
    if (!condition.values.empty() && condition.values.size() != system.masters.size()) {
      condition_messages.emplace_back("boundary value count does not match master basis for " +
                                      DescribeBoundaryLocus(condition.variable,
                                                            condition.location));
    }
    if (condition.location == start_location) {
      explicit_covers_start_location = true;
    }
  }
  if (!condition_messages.empty()) {
    throw BoundaryUnsolvedError(JoinMessages(condition_messages));
  }
  if (!explicit_covers_start_location) {
    throw BoundaryUnsolvedError("no explicit boundary data matched the solver start location: " +
                                start_location);
  }

  for (const auto& boundary_request : boundary_requests) {
    const auto condition_it =
        condition_by_key.find(BoundaryKey(boundary_request.variable, boundary_request.location));
    if (condition_it == condition_by_key.end()) {
      throw BoundaryUnsolvedError("no explicit boundary data matched request: " +
                                  DescribeBoundaryLocus(boundary_request.variable,
                                                        boundary_request.location));
    }
    if (condition_it->second->strategy != boundary_request.strategy) {
      throw BoundaryUnsolvedError("explicit boundary strategy does not match request for " +
                                  DescribeBoundaryLocus(boundary_request.variable,
                                                        boundary_request.location));
    }
  }

  for (const auto& [key, condition] : condition_by_key) {
    if (request_keys.find(key) == request_keys.end()) {
      throw BoundaryUnsolvedError("explicit boundary data did not match any boundary request: " +
                                  DescribeBoundaryLocus(condition->variable,
                                                        condition->location));
    }
  }
}

}  // namespace amflow
