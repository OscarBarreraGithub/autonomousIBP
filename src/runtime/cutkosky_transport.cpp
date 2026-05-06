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
#include "amflow/runtime/artifact_store.hpp"
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

std::string ContourHalfPlaneForDirection(const std::string& direction) {
  return direction == "Im" ? "upper" : "lower";
}

std::string ImaginaryEtaWaypoint(const std::string& radius,
                                 const std::string& half_plane) {
  return half_plane == "upper" ? "eta=" + radius + "*I"
                               : "eta=-" + radius + "*I";
}

std::string SerializeCutkoskyResidueContourPlanForFingerprint(
    const CutkoskyResidueEndpointModel& model) {
  std::ostringstream out;
  out << "kind=b63n-cutkosky-residue-contour-plan\n";
  out << "model_kind=" << model.model_kind << "\n";
  out << "half_plane=" << model.contour_half_plane << "\n";
  out << "domain=" << model.physical_integration_domain << "\n";
  out << "poles=" << model.endpoint_poles.size() << "\n";
  for (std::size_t index = 0; index < model.endpoint_poles.size(); ++index) {
    const CutkoskyResidueEndpointPole& pole = model.endpoint_poles[index];
    out << "pole[" << index << "]=" << pole.variable << "@"
        << pole.location << "|" << pole.classification << "|"
        << (pole.on_physical_contour ? "physical" : "off-contour") << "\n";
  }
  out << "waypoints=" << model.eta_contour_waypoints.size() << "\n";
  for (std::size_t index = 0; index < model.eta_contour_waypoints.size(); ++index) {
    out << "waypoint[" << index << "]="
        << model.eta_contour_waypoints[index].eta << "|"
        << model.eta_contour_waypoints[index].purpose << "\n";
  }
  return out.str();
}

bool MatchesAutomaticPhaseSpaceSurface(const ProblemSpec& spec,
                                       const std::vector<std::size_t>& cut_indices) {
  if (spec.targets.empty()) {
    return false;
  }
  const std::vector<std::string> automatic_phasespace_propagators = {
      "l1^2-msq",
      "(l1+p1)^2",
      "l2^2",
      "(l1+l2+p1)^2",
      "(l1+l2+p1+p2)^2",
      "(l1+l2+p2)^2",
      "(l1+p2)^2",
  };
  return spec.family.name == "phase" &&
         VectorEquals(spec.family.loop_momenta, {"l1", "l2"}) &&
         PropagatorsMatchExactSurface(spec, automatic_phasespace_propagators) &&
         VectorEquals(cut_indices, {0, 2, 4}) &&
         VectorEquals(spec.targets.front().indices, {1, 2, 1, 1, 1, 1, 1}) &&
         NumericSubstitutionEquals(spec, "s", "100") &&
         NumericSubstitutionEquals(spec, "msq", "1");
}

bool MatchesFeynmanPrescriptionSurface(const ProblemSpec& spec,
                                       const std::vector<std::size_t>& cut_indices) {
  if (spec.targets.empty()) {
    return false;
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
  return spec.family.name == "loopxloop" &&
         VectorEquals(spec.family.loop_momenta, {"l1", "l2", "q"}) &&
         PropagatorsMatchExactSurface(spec, feynman_prescription_propagators) &&
         VectorEquals(cut_indices, {8, 9}) &&
         VectorEquals(spec.targets.front().indices,
                      {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0}) &&
         NumericSubstitutionEquals(spec, "s", "10") &&
         NumericSubstitutionEquals(spec, "msq", "1") &&
         NumericSubstitutionEquals(spec, "m2sq", "2/5");
}

std::vector<CutkoskyResidueEndpointPole> AutomaticPhaseSpaceEndpointPoles() {
  return {
      {"q2",
       "0",
       "massless-pair soft endpoint",
       "dPhi_2(q;0,0) and angular denominator moments",
       true},
      {"q2",
       "81",
       "physical one-mass threshold endpoint",
       "lambda(100,1,q2) physical root",
       true},
      {"q2",
       "121",
       "crossed Kallen branch point outside the physical interval",
       "lambda(100,1,q2) second root",
       false},
      {"cos_theta_a",
       "-1",
       "angular collinear endpoint",
       "uncut denominator moment boundary",
       true},
      {"cos_theta_a",
       "1",
       "angular collinear endpoint",
       "uncut denominator moment boundary",
       true},
      {"cos_theta_b",
       "-1..1",
       "secondary angular endpoint pair",
       "D4,D6,D7 angular moment boundaries",
       true},
  };
}

std::vector<CutkoskyResidueEndpointPole> FeynmanPrescriptionEndpointPoles() {
  return {
      {"cos_theta",
       "-1",
       "two-body angular endpoint",
       "phase-space angular moment boundary",
       true},
      {"cos_theta",
       "1",
       "two-body angular endpoint",
       "phase-space angular moment boundary",
       true},
      {"s",
       "(sqrt(msq)-sqrt(m2sq))^2",
       "Kallen lower branch point outside the pinned s-contour",
       "lambda(s,msq,m2sq)",
       false},
      {"s",
       "(sqrt(msq)+sqrt(m2sq))^2",
       "Kallen production threshold below pinned s=10",
       "lambda(s,msq,m2sq)",
       false},
  };
}

CutkoskyResidueEndpointModel BuildCutkoskyResidueEndpointModelInternal(
    const ProblemSpec& spec,
    const std::vector<std::size_t>& cut_indices,
    const FeynmanPrescription cut_prescription) {
  CutkoskyResidueEndpointModel model;
  model.parsed = true;
  model.contour_half_plane =
      ContourHalfPlaneForDirection(EtaDirectionForPrescription(cut_prescription));
  model.eta_zero_selection_rule =
      "PickCutkoskyEtaZeroTerm requires one live propagated eta^0 residue term before "
      "coefficient publication";
  model.coefficient_gap =
      "Live Cutkosky residue endpoint coefficients are still deferred; this model is an "
      "audited parse/contour plan, not AMFlow parity evidence.";

  if (MatchesAutomaticPhaseSpaceSurface(spec, cut_indices)) {
    model.model_kind = "automatic_phasespace::one-mass-three-body-residue";
    model.phase_space_parameterization =
        "dPhi_3(P;m,0,0)=dq2/(2*pi)*dPhi_2(P;m,sqrt(q2))*dPhi_2(q;0,0)";
    model.physical_integration_domain =
        "q2 in [0,81] with angular moments for D2,D4,D6,D7";
    model.kallen_discriminant = "lambda(100,1,q2)=(q2-81)*(q2-121)";
    model.residue_variables = {"q2", "cos_theta_a", "cos_theta_b"};
    model.uncut_denominator_indices = {1, 3, 5, 6};
    model.uncut_denominator_roles = {
        "D2=(l1+p1)^2 angular weight with power 2",
        "D4=(l1+l2+p1)^2 angular weight",
        "D6=(l1+l2+p2)^2 angular weight",
        "D7=(l1+p2)^2 angular weight",
    };
    model.residue_factors = {
        "Cut(D1=l1^2-msq)",
        "Cut(D3=l2^2)",
        "Cut(D5=(l1+l2+p1+p2)^2)",
        "K_2(eps)",
    };
    model.endpoint_local_model_kind =
        "cutkosky-soft-collinear-laurent-scaffold";
    model.ir_pole_classification =
        "massless q2=0 and angular collinear endpoint poles remain classified but "
        "unintegrated";
  } else if (MatchesFeynmanPrescriptionSurface(spec, cut_indices)) {
    const bool plus_minus =
        VectorEquals(spec.family.loop_prescriptions,
                     {FeynmanPrescription::PlusI0,
                      FeynmanPrescription::MinusI0,
                      FeynmanPrescription::None});
    const std::string l1_label = plus_minus ? "plus_i0" : "minus_i0";
    const std::string l2_label = plus_minus ? "minus_i0" : "plus_i0";
    model.model_kind =
        plus_minus ? "feynman_prescription::two-body-residue::plus-minus"
                   : "feynman_prescription::two-body-residue::minus-plus";
    model.phase_space_parameterization =
        "dPhi_2(P;sqrt(msq),sqrt(m2sq)) times prescription-aware T_l1*T_l2";
    model.physical_integration_domain =
        "cos_theta in [-1,1] at lambda(10,1,2/5)=1809/25";
    model.kallen_discriminant = "lambda(10,1,2/5)=1809/25";
    model.residue_variables = {"cos_theta"};
    model.uncut_denominator_indices = {1, 2, 3, 4, 6, 7};
    model.uncut_denominator_roles = {
        "D2=(l1+p1)^2 in T_l1 with " + l1_label,
        "D3=(l1+p1+p2)^2 in T_l1 with " + l1_label,
        "D4=(l1+q)^2 in T_l1 with " + l1_label,
        "D5=l2^2 in T_l2 with " + l2_label,
        "D7=(l2-p2-p1)^2 in T_l2 with " + l2_label,
        "D8=(l2-q)^2 in T_l2 with " + l2_label,
    };
    model.residue_factors = {
        "Cut(D9=q^2-msq)",
        "Cut(D10=(p1+p2-q)^2-m2sq)",
        "K_1(eps)",
        "T_l1=" + l1_label,
        "T_l2=" + l2_label,
    };
    model.endpoint_local_model_kind =
        "cutkosky-two-body-hypergeometric-moment-scaffold";
    model.ir_pole_classification =
        "two-body angular endpoints and loop-subintegral threshold letters remain "
        "classified but unintegrated";
  } else {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky residue endpoint model is intentionally limited to the exact "
        "automatic_phasespace and feynman_prescription reviewed surfaces");
  }

  model.endpoint_poles = ExtractCutkoskyResidueEndpointPoles(model);
  model.eta_contour_waypoints = PlanCutkoskyEtaZeroContour(model);
  model.contour_fingerprint =
      ComputeArtifactFingerprint(SerializeCutkoskyResidueContourPlanForFingerprint(model));
  return model;
}

void ClassifyReviewedSurface(const ProblemSpec& spec,
                             const std::vector<std::size_t>& cut_indices,
                             CutkoskyEtaZeroTransportAudit& audit) {
  if (MatchesAutomaticPhaseSpaceSurface(spec, cut_indices)) {
    audit.reviewed_surface = true;
    audit.reviewed_endpoint_model =
        "automatic_phasespace one-mass three-body Cutkosky residue scaffold";
    audit.branch_ledger_entries.push_back("loop_prescriptions=[0, 0]");
    audit.branch_ledger_entries.push_back(
        "cut propagators D1,D3,D5 resolved as prescription-insensitive real "
        "phase-space cuts");
    return;
  }

  if (MatchesFeynmanPrescriptionSurface(spec, cut_indices)) {
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

std::vector<CutkoskyResidueEndpointPole> ExtractCutkoskyResidueEndpointPoles(
    const CutkoskyResidueEndpointModel& model) {
  if (!model.parsed) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky residue endpoint pole extraction requires a parsed endpoint model");
  }
  if (model.model_kind == "automatic_phasespace::one-mass-three-body-residue") {
    return AutomaticPhaseSpaceEndpointPoles();
  }
  if (model.model_kind.find("feynman_prescription::two-body-residue::") == 0) {
    return FeynmanPrescriptionEndpointPoles();
  }
  throw BoundaryUnsolvedError(
      "b63n Cutkosky residue endpoint pole extraction received an unsupported model kind " +
      model.model_kind);
}

std::vector<CutkoskyEtaContourWaypoint> PlanCutkoskyEtaZeroContour(
    const CutkoskyResidueEndpointModel& model) {
  if (!model.parsed) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 contour planning requires a parsed endpoint model");
  }
  if (model.contour_half_plane != "lower" && model.contour_half_plane != "upper") {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky eta=0 contour planning requires a reviewed half-plane");
  }
  return {
      {ImaginaryEtaWaypoint("64", model.contour_half_plane),
       "asymptotic entry waypoint"},
      {ImaginaryEtaWaypoint("16", model.contour_half_plane),
       "large-radius pole-clearance waypoint"},
      {ImaginaryEtaWaypoint("1", model.contour_half_plane),
       "unit-radius endpoint approach waypoint"},
      {ImaginaryEtaWaypoint("1/16", model.contour_half_plane),
       "near-endpoint branch-ledger waypoint"},
      {"eta=0", "Cutkosky residue endpoint"},
  };
}

CutkoskyEtaZeroSelectionResult PickCutkoskyEtaZeroTerm(
    const std::vector<CutkoskyEtaZeroTerm>& terms) {
  CutkoskyEtaZeroSelectionResult result;
  if (terms.empty()) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b63n PickCutkoskyEtaZeroTerm received no propagated endpoint terms; the "
        "partial scaffold does not publish an implicit coefficient";
    return result;
  }

  std::set<std::string> region_keys;
  std::vector<const CutkoskyEtaZeroTerm*> zero_terms;
  for (const CutkoskyEtaZeroTerm& term : terms) {
    region_keys.insert(term.region_key.empty() ? "integer" : term.region_key);
    if (term.log_power != 0) {
      result.failure_code = "continuation_budget_exhausted";
      result.summary =
          "b63n PickCutkoskyEtaZeroTerm rejects unresolved logarithmic eta=0 terms";
      return result;
    }
    if (term.eta_power < 0 && !term.coefficient_label.empty()) {
      result.dropped_singular_terms.push_back(term.coefficient_label);
    }
    if (term.eta_power == 0) {
      zero_terms.push_back(&term);
    }
  }
  if (region_keys.size() > 1) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b63n PickCutkoskyEtaZeroTerm rejects multiple endpoint regions before "
        "coefficient publication";
    return result;
  }
  if (zero_terms.empty()) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b63n PickCutkoskyEtaZeroTerm found no eta^0 coefficient; the partial scaffold "
        "does not publish an implicit coefficient";
    return result;
  }
  if (zero_terms.size() != 1) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b63n PickCutkoskyEtaZeroTerm found an ambiguous eta^0 coefficient selection";
    return result;
  }
  if (zero_terms.front()->coefficient_label.empty()) {
    result.failure_code = "boundary_unsolved";
    result.summary =
        "b63n PickCutkoskyEtaZeroTerm requires a symbolic live coefficient label";
    return result;
  }

  result.success = true;
  result.selected_coefficient_label = zero_terms.front()->coefficient_label;
  result.summary =
      result.dropped_singular_terms.empty()
          ? "b63n PickCutkoskyEtaZeroTerm selected the unique eta^0 residue term"
          : "b63n PickCutkoskyEtaZeroTerm dropped singular endpoint powers and selected "
            "the unique eta^0 residue term";
  return result;
}

CutkoskyResidueEndpointModel BuildCutkoskyResidueEndpointModel(
    const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky residue endpoint model requires cut propagators");
  }
  RejectEtaOnCutPropagators(spec, cut_indices);
  static_cast<void>(CollectCutPowers(spec, cut_indices));

  const CutkoskyPhaseSpaceTopology topology =
      AnalyzeCutkoskyPhaseSpaceCutTopology(spec);
  if (topology.cut_components.size() != 1) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky residue endpoint model requires a single phase-volume cut "
        "component");
  }
  const CutkoskyPhaseSpaceCutComponent& component = topology.cut_components.front();
  if (component.cut_propagator_indices.size() != component.loop_momenta.size() + 1) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky residue endpoint model requires phase-volume cut count to equal "
        "loop count plus one");
  }

  const FeynmanPrescription cut_prescription = ResolveCutPrescription(spec, cut_indices);
  CutkoskyEtaZeroTransportAudit audit;
  ClassifyReviewedSurface(spec, cut_indices, audit);
  return BuildCutkoskyResidueEndpointModelInternal(spec, cut_indices, cut_prescription);
}

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
  audit.full_eta_zero_contour_applied = false;

  ClassifyReviewedSurface(spec, cut_indices, audit);
  const CutkoskyResidueEndpointModel endpoint_model =
      BuildCutkoskyResidueEndpointModelInternal(spec, cut_indices, cut_prescription);
  audit.runtime_application =
      "partial-cutkosky-eta-zero-residue-contour-audit-no-coefficients";
  audit.transport_scope =
      "cutkosky-residue-endpoint-model-and-contour-plan-no-coefficients";
  audit.residue_model_kind = endpoint_model.model_kind;
  audit.phase_space_parameterization = endpoint_model.phase_space_parameterization;
  audit.physical_integration_domain = endpoint_model.physical_integration_domain;
  audit.contour_fingerprint = endpoint_model.contour_fingerprint;
  audit.endpoint_local_model_kind = endpoint_model.endpoint_local_model_kind;
  audit.ir_pole_classification = endpoint_model.ir_pole_classification;
  audit.uncut_denominator_indices = endpoint_model.uncut_denominator_indices;
  audit.uncut_denominator_roles = endpoint_model.uncut_denominator_roles;
  audit.residue_variables = endpoint_model.residue_variables;
  audit.endpoint_poles = endpoint_model.endpoint_poles;
  audit.eta_contour_waypoints = endpoint_model.eta_contour_waypoints;
  audit.endpoint_selection_rule = endpoint_model.eta_zero_selection_rule;
  audit.eta_zero_selection_audit =
      "PickCutkoskyEtaZeroTerm is wired for live propagated terms, but no endpoint "
      "coefficient terms are produced by this partial lane.";
  audit.failure_code_contract =
      "boundary_unsolved, master_set_instability, continuation_budget_exhausted, "
      "insufficient_precision";
  audit.summary =
      "b63n Cutkosky eta=0 residue scaffold parsed " + endpoint_model.model_kind +
      ", extracted " + std::to_string(endpoint_model.endpoint_poles.size()) +
      " endpoint pole candidate(s), planned a " + endpoint_model.contour_half_plane +
      "-half-plane eta contour with " +
      std::to_string(endpoint_model.eta_contour_waypoints.size()) +
      " waypoint(s), and fingerprinted the contour plan as " +
      endpoint_model.contour_fingerprint +
      ". Live Cutkosky residue integration, eta=0 coefficient construction, and "
      "AMFlow-matching Laurent coefficients remain deferred; retained solution samples "
      "are not consumed and full_eta_zero_contour_applied stays false.";
  return audit;
}

}  // namespace amflow
