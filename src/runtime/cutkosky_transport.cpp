#include "amflow/runtime/cutkosky_transport.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "amflow/core/boundary_data.hpp"
#include "amflow/runtime/boundary_generation.hpp"

namespace amflow {

namespace {

constexpr char kCutkoskyStrategy[] = "builtin::cutkosky-phase-space";

std::string RemoveAsciiSpaces(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](const unsigned char current) {
                               return std::isspace(current) != 0;
                             }),
              value.end());
  return value;
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

std::string JoinSizeVector(const std::vector<std::size_t>& values) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << values[index];
  }
  out << "]";
  return out.str();
}

std::string JoinPrescriptionVector(
    const std::vector<FeynmanPrescription>& prescriptions) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < prescriptions.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << static_cast<int>(prescriptions[index]);
  }
  out << "]";
  return out.str();
}

std::set<std::string> ExtractIdentifiers(const std::string& expression) {
  std::set<std::string> identifiers;
  std::size_t index = 0;
  while (index < expression.size()) {
    const unsigned char current = static_cast<unsigned char>(expression[index]);
    if (std::isalpha(current) == 0 && current != static_cast<unsigned char>('_')) {
      ++index;
      continue;
    }
    const std::size_t begin = index;
    ++index;
    while (index < expression.size()) {
      const unsigned char next = static_cast<unsigned char>(expression[index]);
      if (std::isalnum(next) == 0 && next != static_cast<unsigned char>('_')) {
        break;
      }
      ++index;
    }
    identifiers.insert(expression.substr(begin, index - begin));
  }
  return identifiers;
}

std::string PrescriptionLabel(const FeynmanPrescription prescription) {
  switch (prescription) {
    case FeynmanPrescription::PlusI0:
      return "plus_i0";
    case FeynmanPrescription::MinusI0:
      return "minus_i0";
    case FeynmanPrescription::None:
      return "none";
  }
  return "none";
}

std::string ProviderStrategyForPrescription(const FeynmanPrescription prescription) {
  switch (prescription) {
    case FeynmanPrescription::PlusI0:
      return std::string(kCutkoskyStrategy) + "::plus_i0";
    case FeynmanPrescription::MinusI0:
      return std::string(kCutkoskyStrategy) + "::minus_i0";
    case FeynmanPrescription::None:
      return std::string(kCutkoskyStrategy) + "::none";
  }
  return kCutkoskyStrategy;
}

std::string EtaDirectionForPrescription(const FeynmanPrescription prescription) {
  return prescription == FeynmanPrescription::MinusI0 ? "Im" : "NegIm";
}

std::vector<std::size_t> CollectCutPropagatorIndices(const ProblemSpec& spec) {
  std::vector<std::size_t> cut_indices;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    if (spec.family.propagators[index].kind == PropagatorKind::Cut) {
      cut_indices.push_back(index);
    }
  }
  return cut_indices;
}

std::vector<int> CollectCutPowers(const ProblemSpec& spec,
                                  const std::vector<std::size_t>& cut_indices) {
  if (spec.targets.size() != 1) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold is limited to one reviewed target "
        "integral; live multi-target residue transport remains deferred");
  }
  const TargetIntegral& target = spec.targets.front();
  if (target.indices.size() != spec.family.propagators.size()) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold requires target index width to match "
        "the propagator count");
  }

  std::vector<int> cut_powers;
  cut_powers.reserve(cut_indices.size());
  for (const std::size_t cut_index : cut_indices) {
    const int power = target.indices[cut_index];
    if (power != 1) {
      throw BoundaryUnsolvedError(
          "b63n Cutkosky eta=0 transport scaffold requires unit powers on reviewed cut "
          "propagators; cut propagator " +
          std::to_string(cut_index) + " has target power " + std::to_string(power));
    }
    cut_powers.push_back(power);
  }
  return cut_powers;
}

void RejectEtaOnCutPropagators(const ProblemSpec& spec,
                               const std::vector<std::size_t>& cut_indices) {
  for (const std::size_t cut_index : cut_indices) {
    const std::set<std::string> identifiers =
        ExtractIdentifiers(spec.family.propagators[cut_index].expression);
    if (identifiers.find("eta") == identifiers.end()) {
      continue;
    }
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold rejects eta insertion on cut "
        "denominators before residue construction; cut propagator " +
        std::to_string(cut_index) + " contains eta");
  }
}

FeynmanPrescription ResolveCutPrescription(const ProblemSpec& spec,
                                           const std::vector<std::size_t>& cut_indices) {
  std::optional<FeynmanPrescription> selected;
  std::size_t selected_index = 0;
  for (const std::size_t cut_index : cut_indices) {
    const Propagator& propagator = spec.family.propagators[cut_index];
    std::optional<FeynmanPrescription> resolved;
    if (!spec.family.loop_prescriptions.empty()) {
      resolved = DerivePropagatorPrescriptionFromLoopPrescriptions(spec.family, propagator);
      if (!resolved.has_value()) {
        throw BoundaryUnsolvedError(
            "b63n Cutkosky eta=0 transport scaffold found a prescription conflict while "
            "deriving the cut provider for propagator " +
            std::to_string(cut_index) + " from family.loop_prescriptions");
      }
      const std::optional<FeynmanPrescription> raw =
          ParseFeynmanPrescription(propagator.prescription);
      if (!raw.has_value()) {
        throw std::invalid_argument("family.propagators[" + std::to_string(cut_index) +
                                    "].prescription must be one of -1 (-i0), 0 (none), or "
                                    "1 (+i0)");
      }
      if (*raw != *resolved) {
        throw BoundaryUnsolvedError(
            "b63n Cutkosky eta=0 transport scaffold requires raw cut prescriptions to "
            "match loop-prescription-derived cut prescriptions; cut propagator " +
            std::to_string(cut_index) + " raw=" + PrescriptionLabel(*raw) +
            " derived=" + PrescriptionLabel(*resolved));
      }
    } else {
      resolved = ParseFeynmanPrescription(propagator.prescription);
      if (!resolved.has_value()) {
        throw std::invalid_argument("family.propagators[" + std::to_string(cut_index) +
                                    "].prescription must be one of -1 (-i0), 0 (none), or "
                                    "1 (+i0)");
      }
    }

    if (!selected.has_value()) {
      selected = *resolved;
      selected_index = cut_index;
      continue;
    }
    if (*selected != *resolved) {
      throw BoundaryUnsolvedError(
          "b63n Cutkosky eta=0 transport scaffold requires all cut propagators to "
          "resolve to one provider strategy; cut propagator " +
          std::to_string(cut_index) + " disagrees with cut propagator " +
          std::to_string(selected_index));
    }
  }

  if (!selected.has_value()) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold requires at least one cut propagator");
  }
  return *selected;
}

bool NumericSubstitutionEquals(const ProblemSpec& spec,
                               const std::string& key,
                               const std::string& expected) {
  const auto it = spec.kinematics.numeric_substitutions.find(key);
  return it != spec.kinematics.numeric_substitutions.end() &&
         RemoveAsciiSpaces(it->second) == RemoveAsciiSpaces(expected);
}

bool IsReviewedZeroMassLiteral(const std::string& value) {
  const std::string normalized = RemoveAsciiSpaces(value);
  return normalized == "0" || normalized == "+0" || normalized == "-0";
}

bool PropagatorsMatchExactSurface(const ProblemSpec& spec,
                                  const std::vector<std::string>& expected_expressions) {
  if (spec.family.propagators.size() != expected_expressions.size()) {
    return false;
  }
  for (std::size_t index = 0; index < expected_expressions.size(); ++index) {
    if (RemoveAsciiSpaces(spec.family.propagators[index].expression) !=
        RemoveAsciiSpaces(expected_expressions[index])) {
      return false;
    }
    if (!IsReviewedZeroMassLiteral(spec.family.propagators[index].mass)) {
      return false;
    }
  }
  return true;
}

bool VectorEquals(const std::vector<std::size_t>& lhs,
                  const std::vector<std::size_t>& rhs) {
  return lhs == rhs;
}

bool VectorEquals(const std::vector<std::string>& lhs,
                  const std::vector<std::string>& rhs) {
  return lhs == rhs;
}

bool VectorEquals(const std::vector<int>& lhs, const std::vector<int>& rhs) {
  return lhs == rhs;
}

bool VectorEquals(const std::vector<FeynmanPrescription>& lhs,
                  const std::vector<FeynmanPrescription>& rhs) {
  return lhs == rhs;
}

std::string CutkoskyPrefactorForLoopCount(const std::size_t loop_count) {
  if (loop_count == 1) {
    return "K_1(eps) = 2*Pi^(2-eps)*(2*Pi)^(2*eps-4)";
  }
  if (loop_count == 2) {
    return "K_2(eps) = -2*(Pi^(2-eps)*(2*Pi)^(2*eps-4))^2";
  }
  return "K_r(eps) = 2*(Pi^(2-eps)*(2*Pi)^(2*eps-4))^r*(-1)^(r+1)";
}

void ClassifyReviewedSurface(const ProblemSpec& spec,
                             const std::vector<std::size_t>& cut_indices,
                             CutkoskyEtaZeroTransportAudit& audit) {
  const std::vector<int>& target_indices = spec.targets.front().indices;
  const std::vector<std::string> automatic_phasespace_propagators = {
      "l1^2-msq",
      "(l1+p1)^2",
      "l2^2",
      "(l1+l2+p1)^2",
      "(l1+l2+p1+p2)^2",
      "(l1+l2+p2)^2",
      "(l1+p2)^2",
  };
  if (spec.family.name == "phase" &&
      VectorEquals(spec.family.loop_momenta, {"l1", "l2"}) &&
      PropagatorsMatchExactSurface(spec, automatic_phasespace_propagators) &&
      VectorEquals(cut_indices, {0, 2, 4}) &&
      VectorEquals(target_indices, {1, 2, 1, 1, 1, 1, 1}) &&
      NumericSubstitutionEquals(spec, "s", "100") &&
      NumericSubstitutionEquals(spec, "msq", "1")) {
    audit.reviewed_surface = true;
    audit.reviewed_endpoint_model =
        "automatic_phasespace one-mass three-body Cutkosky residue scaffold";
    audit.branch_ledger_entries.push_back("loop_prescriptions=[0, 0]");
    audit.branch_ledger_entries.push_back(
        "cut propagators D1,D3,D5 resolved as prescription-insensitive real "
        "phase-space cuts");
    return;
  }

  const std::vector<std::string> feynman_prescription_propagators = {
      "l1^2",
      "(l1+p1)^2",
      "(l1+p1+p2)^2",
      "(l1+q)^2",
      "l2^2",
      "(l2-p2)^2",
      "(l2-p2-p1)^2",
      "(l2-q)^2",
      "q^2-msq",
      "(p1+p2-q)^2-m2sq",
      "(l1-l2)^2",
      "(p1-q)^2",
  };
  if (spec.family.name == "loopxloop" &&
      VectorEquals(spec.family.loop_momenta, {"l1", "l2", "q"}) &&
      PropagatorsMatchExactSurface(spec, feynman_prescription_propagators) &&
      VectorEquals(cut_indices, {8, 9}) &&
      VectorEquals(target_indices, {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0}) &&
      NumericSubstitutionEquals(spec, "s", "10") &&
      NumericSubstitutionEquals(spec, "msq", "1") &&
      NumericSubstitutionEquals(spec, "m2sq", "2/5")) {
    const bool plus_minus =
        VectorEquals(spec.family.loop_prescriptions,
                     {FeynmanPrescription::PlusI0,
                      FeynmanPrescription::MinusI0,
                      FeynmanPrescription::None});
    const bool minus_plus =
        VectorEquals(spec.family.loop_prescriptions,
                     {FeynmanPrescription::MinusI0,
                      FeynmanPrescription::PlusI0,
                      FeynmanPrescription::None});
    if (!plus_minus && !minus_plus) {
      throw BoundaryUnsolvedError(
          "b63n Cutkosky eta=0 transport scaffold recognizes feynman_prescription only "
          "for the two reviewed conjugate loop-prescription assignments");
    }
    audit.reviewed_surface = true;
    audit.reviewed_endpoint_model =
        "feynman_prescription two-body cut with prescription-aware loop subintegral "
        "scaffold";
    audit.branch_ledger_entries.push_back(
        "loop_prescriptions=" + JoinPrescriptionVector(spec.family.loop_prescriptions));
    audit.branch_ledger_entries.push_back(
        plus_minus ? "uncut ledger: T_l1=plus_i0, T_l2=minus_i0"
                   : "uncut ledger: T_l1=minus_i0, T_l2=plus_i0");
    return;
  }

  throw BoundaryUnsolvedError(
      "b63n Cutkosky eta=0 transport scaffold is intentionally limited to the exact "
      "automatic_phasespace and feynman_prescription topologies; got family " +
      spec.family.name + " with cut propagators " + JoinSizeVector(cut_indices));
}

}  // namespace

CutkoskyEtaZeroTransportAudit BuildCutkoskyEtaZeroTransportScaffold(
    const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold requires cut propagators");
  }
  RejectEtaOnCutPropagators(spec, cut_indices);

  const CutkoskyPhaseSpaceTopology topology =
      AnalyzeCutkoskyPhaseSpaceCutTopology(spec);
  if (topology.cut_components.size() != 1) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold requires a single phase-volume cut "
        "component");
  }
  const CutkoskyPhaseSpaceCutComponent& component = topology.cut_components.front();
  if (component.cut_propagator_indices.size() != component.loop_momenta.size() + 1) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 transport scaffold requires phase-volume cut count to equal "
        "loop count plus one; cuts=" +
        std::to_string(component.cut_propagator_indices.size()) + " loops=" +
        std::to_string(component.loop_momenta.size()));
  }

  CutkoskyEtaZeroTransportAudit audit;
  audit.family = spec.family.name;
  audit.cut_propagator_indices = cut_indices;
  audit.cut_powers = CollectCutPowers(spec, cut_indices);
  audit.phase_volume_loop_momenta = component.loop_momenta;
  audit.phase_volume_loop_count = component.loop_momenta.size();
  const FeynmanPrescription cut_prescription = ResolveCutPrescription(spec, cut_indices);
  audit.provider_strategy = ProviderStrategyForPrescription(cut_prescription);
  audit.eta_contour_direction = EtaDirectionForPrescription(cut_prescription);
  audit.cutkosky_prefactor = CutkoskyPrefactorForLoopCount(audit.phase_volume_loop_count);
  audit.endpoint_selection_rule =
      "PickCutkoskyEtaZeroTerm scaffold: integer eta^0 coefficient selection required "
      "before publication";
  audit.coefficient_gap =
      "Live Cutkosky residue coefficients are not yet implemented in this scaffold; no "
      "AMFlow solution samples are consumed and full_eta_zero_contour_applied must remain "
      "false until endpoint coefficients are produced by the reviewed eta=0 transport.";
  audit.live_coefficients_available = false;
  audit.retained_solution_samples_used = false;

  ClassifyReviewedSurface(spec, cut_indices, audit);
  return audit;
}

}  // namespace amflow
