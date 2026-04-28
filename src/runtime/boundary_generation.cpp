#include "amflow/runtime/boundary_generation.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "amflow/core/options.hpp"
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

constexpr char kBuiltinCutkoskyPhaseSpaceStrategy[] = "builtin::cutkosky-phase-space";

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

void ValidateBuiltinCutkoskyPhaseSpaceSubset(const ProblemSpec& spec) {
  bool saw_cut_propagator = false;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind == PropagatorKind::Cut) {
      saw_cut_propagator = true;
      continue;
    }
    if (propagator.kind == PropagatorKind::Standard) {
      continue;
    }
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary request generation only supports standard/cut "
        "propagators; propagator " +
        std::to_string(index) + " has kind " + ToString(propagator.kind));
  }

  if (!saw_cut_propagator) {
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary request generation requires at least one cut "
        "propagator on the current reviewed phase-space subset");
  }
}

std::string CutkoskyPhaseSpaceProviderStrategyForPrescription(
    const FeynmanPrescription prescription) {
  switch (prescription) {
    case FeynmanPrescription::PlusI0:
      return std::string(kBuiltinCutkoskyPhaseSpaceStrategy) + "::plus_i0";
    case FeynmanPrescription::MinusI0:
      return std::string(kBuiltinCutkoskyPhaseSpaceStrategy) + "::minus_i0";
    case FeynmanPrescription::None:
      return std::string(kBuiltinCutkoskyPhaseSpaceStrategy) + "::none";
  }
  return kBuiltinCutkoskyPhaseSpaceStrategy;
}

std::string ResolveRawCutkoskyPhaseSpaceStrategy(const ProblemSpec& spec) {
  std::optional<FeynmanPrescription> selected_cut_prescription;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Cut) {
      continue;
    }

    const std::optional<FeynmanPrescription> raw_prescription =
        ParseFeynmanPrescription(propagator.prescription);
    if (!raw_prescription.has_value()) {
      throw std::invalid_argument("family.propagators[" + std::to_string(index) +
                                  "].prescription must be one of -1 (-i0), 0 (none), or 1 "
                                  "(+i0)");
    }

    if (!selected_cut_prescription.has_value()) {
      selected_cut_prescription = *raw_prescription;
      continue;
    }

    if (*selected_cut_prescription != *raw_prescription) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires all cut "
          "propagators to carry the same raw provider strategy on the current reviewed "
          "raw-prescription provider-selection subset");
    }
  }

  if (!selected_cut_prescription.has_value()) {
    return kBuiltinCutkoskyPhaseSpaceStrategy;
  }

  return CutkoskyPhaseSpaceProviderStrategyForPrescription(*selected_cut_prescription);
}

std::string ResolveBuiltinCutkoskyPhaseSpaceStrategy(const ProblemSpec& spec) {
  if (spec.family.loop_prescriptions.empty()) {
    return ResolveRawCutkoskyPhaseSpaceStrategy(spec);
  }

  std::optional<FeynmanPrescription> selected_cut_prescription;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Cut) {
      continue;
    }

    const std::optional<FeynmanPrescription> derived_prescription =
        DerivePropagatorPrescriptionFromLoopPrescriptions(spec.family, propagator);
    if (!derived_prescription.has_value()) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation could not derive a "
          "loop-prescription-backed provider strategy for cut propagator " +
          std::to_string(index) + " from family.loop_prescriptions");
    }

    const std::optional<FeynmanPrescription> raw_prescription =
        ParseFeynmanPrescription(propagator.prescription);
    if (!raw_prescription.has_value()) {
      throw std::invalid_argument("family.propagators[" + std::to_string(index) +
                                  "].prescription must be one of -1 (-i0), 0 (none), or 1 "
                                  "(+i0)");
    }

    if (*derived_prescription != *raw_prescription) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires cut propagator " +
          std::to_string(index) +
          " raw prescription to match family.loop_prescriptions on the current reviewed "
          "provider-selection subset");
    }

    if (!selected_cut_prescription.has_value()) {
      selected_cut_prescription = *derived_prescription;
      continue;
    }

    if (*selected_cut_prescription != *derived_prescription) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires all cut "
          "propagators to resolve to the same loop-prescription-backed provider strategy on "
          "the current reviewed provider-selection subset");
    }
  }

  if (!selected_cut_prescription.has_value()) {
    return kBuiltinCutkoskyPhaseSpaceStrategy;
  }

  return CutkoskyPhaseSpaceProviderStrategyForPrescription(*selected_cut_prescription);
}

std::string SupportedEtaInfinityTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::eta->infinity";
}

std::string SupportedCutkoskyPhaseSpaceTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::cutkosky-phase-space";
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

void ValidatePlannedCutkoskyPhaseSpaceTerminalNodes(const ProblemSpec& spec,
                                                    const EndingDecision& decision) {
  const std::string supported_terminal_node = SupportedCutkoskyPhaseSpaceTerminalNode(spec);
  const std::size_t supported_count =
      static_cast<std::size_t>(std::count(decision.terminal_nodes.begin(),
                                          decision.terminal_nodes.end(),
                                          supported_terminal_node));
  if (supported_count > 1) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; duplicate supported terminal node");
  }

  for (const std::string& terminal_node : decision.terminal_nodes) {
    if (terminal_node != supported_terminal_node) {
      throw BoundaryUnsolvedError(
          "planned Cutkosky phase-space boundary request requires exactly one supported "
          "terminal node " +
          supported_terminal_node + "; unsupported extra terminal node " + terminal_node);
    }
  }

  if (supported_count == 0) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires exactly one supported terminal "
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

BoundaryRequest GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(const ProblemSpec& spec,
                                                                 const std::string& eta_symbol) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  if (eta_symbol.empty()) {
    throw std::invalid_argument(
        "builtin Cutkosky phase-space boundary request eta_symbol must not be empty");
  }

  ValidateBuiltinCutkoskyPhaseSpaceSubset(spec);

  BoundaryRequest request;
  request.variable = eta_symbol;
  request.location = "cutkosky-phase-space";
  request.strategy = ResolveBuiltinCutkoskyPhaseSpaceStrategy(spec);
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

BoundaryRequest GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  const EndingDecision decision =
      PlanEndingScheme(spec, ending_scheme_name, user_defined_schemes);
  ValidatePlannedCutkoskyPhaseSpaceTerminalNodes(spec, decision);
  return GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(spec, eta_symbol);
}

BoundaryRequest GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  const EndingDecision decision =
      PlanAmfOptionsEndingScheme(spec, amf_options, user_defined_schemes);
  ValidatePlannedEtaInfinityTerminalNodes(spec, decision);
  return GenerateBuiltinEtaInfinityBoundaryRequest(spec, eta_symbol);
}

BoundaryRequest GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  const EndingDecision decision =
      PlanAmfOptionsEndingScheme(spec, amf_options, user_defined_schemes);
  ValidatePlannedCutkoskyPhaseSpaceTerminalNodes(spec, decision);
  return GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(spec, eta_symbol);
}

}  // namespace amflow
