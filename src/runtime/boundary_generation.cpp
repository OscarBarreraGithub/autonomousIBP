#include "amflow/runtime/boundary_generation.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "amflow/runtime/ending_scheme.hpp"

namespace amflow {

namespace {

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

void ValidateBuiltinEtaInfinitySubset(const ProblemSpec& spec) {
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Standard) {
      throw BoundaryUnsolvedError(
          "builtin eta->infinity boundary request generation only supports standard "
          "propagators; propagator " +
          std::to_string(index) + " has kind " + ToString(propagator.kind));
    }
    if (propagator.mass != "0") {
      throw BoundaryUnsolvedError(
          "builtin eta->infinity boundary request generation only supports propagators with "
          "mass exactly \"0\"; propagator " +
          std::to_string(index) + " has mass \"" + propagator.mass + "\"");
    }
  }
}

std::string SupportedEtaInfinityTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::eta->infinity";
}

void ValidatePlannedEtaInfinityTerminalNodes(const ProblemSpec& spec,
                                             const EndingDecision& decision) {
  const std::string supported_terminal_node = SupportedEtaInfinityTerminalNode(spec);
  const std::size_t supported_count =
      static_cast<std::size_t>(std::count(decision.terminal_nodes.begin(),
                                          decision.terminal_nodes.end(),
                                          supported_terminal_node));
  if (supported_count > 1) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; duplicate supported terminal node");
  }

  for (const std::string& terminal_node : decision.terminal_nodes) {
    if (terminal_node != supported_terminal_node) {
      throw BoundaryUnsolvedError(
          "planned eta->infinity boundary request requires exactly one supported terminal "
          "node " +
          supported_terminal_node + "; unsupported extra terminal node " + terminal_node);
    }
  }

  if (supported_count == 0) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; missing supported terminal node");
  }
}

}  // namespace

BoundaryRequest GenerateBuiltinEtaInfinityBoundaryRequest(const ProblemSpec& spec,
                                                         const std::string& eta_symbol) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  if (eta_symbol.empty()) {
    throw std::invalid_argument(
        "builtin eta->infinity boundary request eta_symbol must not be empty");
  }

  ValidateBuiltinEtaInfinitySubset(spec);

  BoundaryRequest request;
  request.variable = eta_symbol;
  request.location = "infinity";
  request.strategy = "builtin::eta->infinity";
  return request;
}

BoundaryRequest GeneratePlannedEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  const EndingDecision decision =
      PlanEndingScheme(spec, ending_scheme_name, user_defined_schemes);
  ValidatePlannedEtaInfinityTerminalNodes(spec, decision);
  return GenerateBuiltinEtaInfinityBoundaryRequest(spec, eta_symbol);
}

}  // namespace amflow
