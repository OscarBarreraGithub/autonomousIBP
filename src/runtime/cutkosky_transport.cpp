#include "amflow/runtime/cutkosky_transport.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/math/constants/constants.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include "amflow/core/boundary_data.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/boundary_generation.hpp"

namespace amflow {

namespace {

constexpr char kCutkoskyStrategy[] = "builtin::cutkosky-phase-space";
using CutkoskyPrefactorFloat = boost::multiprecision::cpp_dec_float_100;

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

CutkoskyPrefactorFloat PositiveIntegerPower(CutkoskyPrefactorFloat base,
                                            const std::size_t exponent) {
  CutkoskyPrefactorFloat result = 1;
  for (std::size_t index = 0; index < exponent; ++index) {
    result *= base;
  }
  return result;
}

std::string FormatCutkoskyPrefactorFloat(const CutkoskyPrefactorFloat& value,
                                         const int precision_digits) {
  std::ostringstream out;
  out << std::scientific << std::setprecision(precision_digits - 1) << value;
  return out.str();
}

CutkoskyPrefactorFloat ParseCutkoskyPrefactorFloat(const std::string& value,
                                                   const std::string& label) {
  try {
    return CutkoskyPrefactorFloat(value);
  } catch (const std::exception& error) {
    throw std::invalid_argument("b63n Cutkosky prefactor multiplication requires "
                                "decimal numeric " +
                                label + "; got `" + value + "` (" + error.what() + ")");
  }
}

struct CutkoskyComplexCoefficient {
  CutkoskyPrefactorFloat real = 0;
  CutkoskyPrefactorFloat imaginary = 0;
};

CutkoskyComplexCoefficient ParseCutkoskySeriesTerm(
    const CutkoskyPrefactorSeriesTerm& term,
    const std::string& label) {
  return {ParseCutkoskyPrefactorFloat(term.real, label + ".real"),
          ParseCutkoskyPrefactorFloat(term.imaginary.empty() ? "0" : term.imaginary,
                                      label + ".imaginary")};
}

CutkoskyComplexCoefficient ParseCutkoskyResidueSeriesTermCoefficient(
    const CutkoskyResidueSeriesTerm& term,
    const std::string& label) {
  return {
      ParseCutkoskyPrefactorFloat(term.coefficient.real, label + ".coefficient.real"),
      ParseCutkoskyPrefactorFloat(
          term.coefficient.imaginary.empty() ? "0" : term.coefficient.imaginary,
          label + ".coefficient.imaginary")};
}

bool IsZeroCutkoskyCoefficient(const CutkoskyComplexCoefficient& coefficient) {
  return coefficient.real == 0 && coefficient.imaginary == 0;
}

struct CutkoskyResidueSeriesTermKey {
  int eps_order = 0;
  int eta_power = 0;
  int log_power = 0;
  std::string region_key;

  bool operator<(const CutkoskyResidueSeriesTermKey& other) const {
    if (eps_order != other.eps_order) {
      return eps_order < other.eps_order;
    }
    if (eta_power != other.eta_power) {
      return eta_power < other.eta_power;
    }
    if (log_power != other.log_power) {
      return log_power < other.log_power;
    }
    return region_key < other.region_key;
  }
};

std::string MergeCutkoskyResidueCoefficientLabels(const std::string& lhs,
                                                  const std::string& rhs) {
  if (rhs.empty() || lhs == rhs) {
    return lhs;
  }
  if (lhs.empty()) {
    return rhs;
  }
  return lhs + "+" + rhs;
}

void ValidateCutkoskyPrefactorForMultiplication(
    const CutkoskyPrefactorSeries& prefactor) {
  if (prefactor.loop_count == 0 || prefactor.terms.empty()) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires a non-empty prefactor series");
  }
  if (prefactor.requested_precision_digits < 16 ||
      prefactor.requested_precision_digits > 90) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires a prefactor precision in "
        "[16, 90] decimal digits");
  }
  if (prefactor.min_eps_order != 0) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires prefactor terms from eps^0");
  }
}

std::string ContourHalfPlaneForDirection(const std::string& direction) {
  return direction == "Im" ? "upper" : "lower";
}

std::string PropagatorDisplayLabel(const std::size_t index) {
  return "D" + std::to_string(index + 1);
}

CutkoskyBranchLedgerPrescription MakeBranchLedgerPrescription(
    const std::string& target,
    const FeynmanPrescription prescription,
    const std::string& source) {
  CutkoskyBranchLedgerPrescription entry;
  entry.target = target;
  entry.prescription = prescription;
  entry.source = source;
  return entry;
}

std::vector<CutkoskyBranchLedgerPrescription> BuildLoopPrescriptionLedger(
    const FamilyDefinition& family) {
  std::vector<CutkoskyBranchLedgerPrescription> prescriptions;
  prescriptions.reserve(family.loop_prescriptions.size());
  for (std::size_t index = 0; index < family.loop_prescriptions.size(); ++index) {
    const std::string target = index < family.loop_momenta.size()
                                   ? "loop:" + family.loop_momenta[index]
                                   : "loop[" + std::to_string(index) + "]";
    prescriptions.push_back(MakeBranchLedgerPrescription(
        target, family.loop_prescriptions[index], "family.loop_prescriptions"));
  }
  return prescriptions;
}

std::vector<CutkoskyBranchLedgerPrescription> BuildCutProviderPrescriptionLedger(
    const std::vector<std::size_t>& cut_indices,
    const FeynmanPrescription cut_prescription) {
  std::vector<CutkoskyBranchLedgerPrescription> prescriptions;
  prescriptions.reserve(cut_indices.size());
  for (const std::size_t cut_index : cut_indices) {
    prescriptions.push_back(MakeBranchLedgerPrescription(
        PropagatorDisplayLabel(cut_index),
        cut_prescription,
        "family.propagators[].prescription"));
  }
  return prescriptions;
}

CutkoskyBranchLedgerEntry MakeBranchLedgerEntry(
    const std::string& summary,
    const std::vector<CutkoskyBranchLedgerPrescription>& prescriptions,
    const std::vector<std::size_t>& cut_support,
    const std::string& eta_half_plane,
    const std::string& branch_provenance) {
  CutkoskyBranchLedgerEntry entry;
  entry.summary = summary;
  entry.prescriptions = prescriptions;
  entry.cut_support = cut_support;
  entry.eta_half_plane = eta_half_plane;
  entry.branch_provenance = branch_provenance;
  return entry;
}

void RecordBranchLedgerEntry(CutkoskyEtaZeroTransportAudit& audit,
                             const CutkoskyBranchLedgerEntry& entry) {
  audit.branch_ledger.push_back(entry);
  audit.branch_ledger_entries.push_back(
      SerializeCutkoskyBranchLedgerEntrySummary(entry));
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

bool MatchesAutomaticPhaseSpaceFirstCoefficientSurface(
    const ProblemSpec& spec,
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
         VectorEquals(spec.targets.front().indices, {1, 0, 1, 0, 1, 0, 0}) &&
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

CutkoskyResidueEndpointModel BuildAutomaticPhaseSpaceFirstCoefficientEndpointModel(
    const FeynmanPrescription cut_prescription) {
  CutkoskyResidueEndpointModel model;
  model.parsed = true;
  model.model_kind = "automatic_phasespace::pure-cut-three-body-volume";
  model.phase_space_parameterization =
      "dPhi_3(P;m,0,0)=dq2/(2*pi)*dPhi_2(P;m,sqrt(q2))*dPhi_2(q;0,0)";
  model.physical_integration_domain =
      "q2 in [0,81] with no uncut denominator weights for the selected master";
  model.kallen_discriminant = "lambda(100,1,q2)=(q2-81)*(q2-121)";
  model.contour_half_plane =
      ContourHalfPlaneForDirection(EtaDirectionForPrescription(cut_prescription));
  model.endpoint_local_model_kind = "cutkosky-pure-phase-volume-r0";
  model.ir_pole_classification =
      "pure three-body phase-volume residue is regular in the selected eta row; "
      "non-selected soft/collinear weighted residues remain deferred";
  model.eta_zero_selection_rule =
      "selected pure-cut master has a unique integer eta^0 residue coefficient";
  model.residue_variables = {"q2", "cos_theta_a", "cos_theta_b"};
  model.residue_factors = {
      "Cut(D1=l1^2-msq)",
      "Cut(D3=l2^2)",
      "Cut(D5=(l1+l2+p1+p2)^2)",
      "K_2(eps)",
      "unit uncut-denominator moment",
  };
  model.endpoint_poles = AutomaticPhaseSpaceEndpointPoles();
  model.eta_contour_waypoints = PlanCutkoskyEtaZeroContour(model);
  model.contour_fingerprint =
      ComputeArtifactFingerprint(SerializeCutkoskyResidueContourPlanForFingerprint(model));
  return model;
}

std::string SerializeCutkoskyResidueCoefficientAuditForFingerprint(
    const CutkoskyResidueCoefficientAudit& audit) {
  std::ostringstream out;
  out << "kind=b63n-cutkosky-selected-residue-coefficient\n";
  out << "master=" << audit.master_label << "\n";
  out << "runtime_application=" << audit.runtime_application << "\n";
  out << "transport_scope=" << audit.transport_scope << "\n";
  out << "residue_model_kind=" << audit.residue_model_kind << "\n";
  out << "endpoint_local_model_kind=" << audit.endpoint_local_model_kind << "\n";
  out << "contour_fingerprint=" << audit.contour_fingerprint << "\n";
  out << "final_solution_samples_used_as_input=false\n";
  return out.str();
}

std::string AutomaticPhaseSpaceStructuralFormForDenominator(
    const std::string& denominator_id) {
  if (denominator_id == "D2") {
    return "inverse_denominator_weight[D2(q2,cos_theta_a)]";
  }
  if (denominator_id == "D4") {
    return "inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)]";
  }
  if (denominator_id == "D6") {
    return "inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)]";
  }
  if (denominator_id == "D7") {
    return "inverse_denominator_weight[D7(q2,cos_theta_a)]";
  }
  throw BoundaryUnsolvedError(
      "b63n automatic phase-space symbolic integrand received unsupported uncut " +
      denominator_id);
}

void ValidateAutomaticPhaseSpaceSymbolicFactorRole(
    const CutkoskyResidueEndpointModel& model,
    const std::size_t role_index,
    const std::string& denominator_id) {
  if (role_index >= model.uncut_denominator_roles.size()) {
    throw BoundaryUnsolvedError(
        "b63n automatic phase-space symbolic integrand is missing an endpoint-model "
        "uncut denominator role for " +
        denominator_id);
  }
  const std::string& role = model.uncut_denominator_roles[role_index];
  if (role.find(denominator_id + "=") != 0) {
    throw BoundaryUnsolvedError(
        "b63n automatic phase-space symbolic integrand role/order mismatch for " +
        denominator_id);
  }
}

std::string JoinPropagatorDisplayLabels(const std::vector<std::size_t>& indices) {
  std::ostringstream out;
  bool first = true;
  for (const std::size_t index : indices) {
    if (!first) {
      out << ",";
    }
    first = false;
    out << PropagatorDisplayLabel(index);
  }
  return out.str();
}

const CutkoskyBranchLedgerEntry& RequireFeynmanSubintegralLedger(
    const CutkoskyEtaZeroTransportAudit& audit) {
  for (const CutkoskyBranchLedgerEntry& entry : audit.branch_ledger) {
    bool has_t_l1 = false;
    bool has_t_l2 = false;
    for (const CutkoskyBranchLedgerPrescription& prescription : entry.prescriptions) {
      has_t_l1 = has_t_l1 || prescription.target == "T_l1";
      has_t_l2 = has_t_l2 || prescription.target == "T_l2";
    }
    if (has_t_l1 && has_t_l2) {
      return entry;
    }
  }
  throw BoundaryUnsolvedError(
      "b63n feynman_prescription symbolic subintegral assembly requires the recorded "
      "T_l1/T_l2 branch ledger");
}

const CutkoskyBranchLedgerPrescription& RequireLedgerPrescription(
    const CutkoskyBranchLedgerEntry& ledger,
    const std::string& ledger_handle) {
  for (const CutkoskyBranchLedgerPrescription& prescription : ledger.prescriptions) {
    if (prescription.target == ledger_handle) {
      return prescription;
    }
  }
  throw BoundaryUnsolvedError(
      "b63n feynman_prescription symbolic subintegral assembly is missing branch-ledger "
      "prescription for " +
      ledger_handle);
}

std::string LoopMomentumForFeynmanSubintegral(const std::string& ledger_handle) {
  if (ledger_handle == "T_l1") {
    return "l1";
  }
  if (ledger_handle == "T_l2") {
    return "l2";
  }
  throw BoundaryUnsolvedError(
      "b63n feynman_prescription symbolic subintegral assembly received unsupported "
      "ledger handle " +
      ledger_handle);
}

std::string FeynmanSubintegralHandleForDenominator(
    const std::string& denominator_id) {
  if (denominator_id == "D2" || denominator_id == "D3" ||
      denominator_id == "D4") {
    return "T_l1";
  }
  if (denominator_id == "D5" || denominator_id == "D7" ||
      denominator_id == "D8") {
    return "T_l2";
  }
  throw BoundaryUnsolvedError(
      "b63n feynman_prescription symbolic subintegral assembly received unsupported "
      "uncut denominator " +
      denominator_id);
}

std::string FeynmanSubintegralStructuralFormForDenominator(
    const std::string& denominator_id,
    const std::string& ledger_handle,
    const std::string& ledger_sign) {
  return "inverse_subintegral_denominator[" + ledger_handle + "." + ledger_sign +
         ":" + denominator_id + "(cos_theta)]";
}

void ValidateFeynmanSubintegralFactorRole(
    const CutkoskyResidueEndpointModel& model,
    const std::size_t role_index,
    const std::string& denominator_id,
    const std::string& ledger_handle,
    const std::string& ledger_sign) {
  if (role_index >= model.uncut_denominator_roles.size()) {
    throw BoundaryUnsolvedError(
        "b63n feynman_prescription symbolic subintegral assembly is missing an "
        "endpoint-model uncut denominator role for " +
        denominator_id);
  }
  const std::string& role = model.uncut_denominator_roles[role_index];
  const std::string expected_prefix = denominator_id + "=";
  if (role.substr(std::string::size_type{}, expected_prefix.size()) !=
      expected_prefix) {
    throw BoundaryUnsolvedError(
        "b63n feynman_prescription symbolic subintegral assembly role/order mismatch "
        "for " +
        denominator_id);
  }
  if (role.find(" in " + ledger_handle + " ") == std::string::npos ||
      role.find(" with " + ledger_sign) == std::string::npos) {
    throw BoundaryUnsolvedError(
        "b63n feynman_prescription symbolic subintegral assembly role does not match "
        "the recorded " +
        ledger_handle + " branch ledger");
  }
}

CutkoskySymbolicSubintegral MakeFeynmanSubintegralCarrier(
    const CutkoskyBranchLedgerEntry& ledger,
    const std::string& ledger_handle) {
  const CutkoskyBranchLedgerPrescription& prescription =
      RequireLedgerPrescription(ledger, ledger_handle);
  CutkoskySymbolicSubintegral subintegral;
  subintegral.ledger_handle = ledger_handle;
  subintegral.loop_momentum = LoopMomentumForFeynmanSubintegral(ledger_handle);
  subintegral.prescription = prescription.prescription;
  subintegral.ledger_sign = PrescriptionLabel(prescription.prescription);
  subintegral.prescription_source = prescription.source;
  return subintegral;
}

CutkoskySymbolicSubintegral& RequireMutableSubintegralByHandle(
    std::vector<CutkoskySymbolicSubintegral>& subintegrals,
    const std::string& ledger_handle) {
  for (CutkoskySymbolicSubintegral& subintegral : subintegrals) {
    if (subintegral.ledger_handle == ledger_handle) {
      return subintegral;
    }
  }
  throw BoundaryUnsolvedError(
      "b63n feynman_prescription symbolic subintegral assembly cannot find carrier "
      "for ledger handle " +
      ledger_handle);
}

void ClassifyReviewedSurface(const ProblemSpec& spec,
                             const std::vector<std::size_t>& cut_indices,
                             const FeynmanPrescription cut_prescription,
                             CutkoskyEtaZeroTransportAudit& audit) {
  const std::string eta_half_plane =
      ContourHalfPlaneForDirection(EtaDirectionForPrescription(cut_prescription));
  if (MatchesAutomaticPhaseSpaceSurface(spec, cut_indices)) {
    audit.reviewed_surface = true;
    audit.reviewed_endpoint_model =
        "automatic_phasespace one-mass three-body Cutkosky residue scaffold";
    RecordBranchLedgerEntry(
        audit,
        MakeBranchLedgerEntry(
            "loop_prescriptions=[0, 0]",
            BuildLoopPrescriptionLedger(spec.family),
            cut_indices,
            eta_half_plane,
            "reviewed automatic_phasespace loop-prescription metadata"));
    RecordBranchLedgerEntry(
        audit,
        MakeBranchLedgerEntry(
            "cut propagators D1,D3,D5 resolved as prescription-insensitive real "
            "phase-space cuts",
            BuildCutProviderPrescriptionLedger(cut_indices, cut_prescription),
            cut_indices,
            eta_half_plane,
            "reviewed automatic_phasespace real phase-space cut support"));
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
    RecordBranchLedgerEntry(
        audit,
        MakeBranchLedgerEntry(
            "loop_prescriptions=" + JoinPrescriptionVector(spec.family.loop_prescriptions),
            BuildLoopPrescriptionLedger(spec.family),
            cut_indices,
            eta_half_plane,
            "reviewed feynman_prescription loop-prescription metadata"));
    RecordBranchLedgerEntry(
        audit,
        MakeBranchLedgerEntry(
            plus_minus ? "uncut ledger: T_l1=plus_i0, T_l2=minus_i0"
                       : "uncut ledger: T_l1=minus_i0, T_l2=plus_i0",
            {MakeBranchLedgerPrescription(
                 "T_l1",
                 plus_minus ? FeynmanPrescription::PlusI0
                            : FeynmanPrescription::MinusI0,
                 "family.loop_prescriptions"),
             MakeBranchLedgerPrescription(
                 "T_l2",
                 plus_minus ? FeynmanPrescription::MinusI0
                            : FeynmanPrescription::PlusI0,
                 "family.loop_prescriptions")},
            cut_indices,
            eta_half_plane,
            "reviewed feynman_prescription conjugate loop-subintegral branch ledger"));
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

CutkoskyPrefactorSeries BuildCutkoskyPrefactorEpsilonSeries(
    const std::size_t loop_count,
    const int min_eps_order,
    const int max_eps_order,
    const int requested_precision_digits) {
  if (loop_count == 0) {
    throw std::invalid_argument(
        "b63n Cutkosky K_r(eps) prefactor requires positive phase-volume loop count");
  }
  if (min_eps_order < 0) {
    throw std::invalid_argument(
        "b63n Cutkosky K_r(eps) prefactor is analytic at eps=0; negative Laurent "
        "orders belong to the residue series before prefactor multiplication");
  }
  if (max_eps_order < min_eps_order) {
    throw std::invalid_argument(
        "b63n Cutkosky K_r(eps) prefactor requires max_eps_order >= min_eps_order");
  }
  if (requested_precision_digits < 16 || requested_precision_digits > 90) {
    throw std::invalid_argument(
        "insufficient_precision: b63n Cutkosky K_r(eps) primitive uses cpp_dec_float_100 "
        "and accepts requested precision in [16, 90] decimal digits");
  }

  const CutkoskyPrefactorFloat pi =
      boost::math::constants::pi<CutkoskyPrefactorFloat>();
  const CutkoskyPrefactorFloat base = CutkoskyPrefactorFloat(1) / (16 * pi * pi);
  const CutkoskyPrefactorFloat sign = (loop_count % 2 == 1) ? 1 : -1;
  const CutkoskyPrefactorFloat normalization =
      2 * sign * PositiveIntegerPower(base, loop_count);
  const CutkoskyPrefactorFloat exponential_scale =
      CutkoskyPrefactorFloat(loop_count) * log(4 * pi);

  CutkoskyPrefactorSeries series;
  series.loop_count = loop_count;
  series.min_eps_order = min_eps_order;
  series.max_eps_order = max_eps_order;
  series.requested_precision_digits = requested_precision_digits;
  series.working_precision_digits =
      std::numeric_limits<CutkoskyPrefactorFloat>::digits10;
  series.formula = CutkoskyPrefactorForLoopCount(loop_count);
  series.normalization_factor =
      FormatCutkoskyPrefactorFloat(normalization, requested_precision_digits);
  series.exponential_scale =
      FormatCutkoskyPrefactorFloat(exponential_scale, requested_precision_digits);
  series.source_reference =
      "AMFlow.m:941-950 and docs/theory/b63n-runtime-lane.md:149-167; lane149 "
      "requires numerical/series K_2(eps) before multiplication into live residues";

  std::ostringstream diagnostics;
  diagnostics << "b63n Cutkosky K_r(eps) prefactor expanded from reviewed closed form "
              << "K_r=2*(-1)^(r+1)/(16*pi^2)^r*exp(r*eps*log(4*pi)); "
              << "requested_digits=" << requested_precision_digits
              << ", working_digits=" << series.working_precision_digits
              << ", guard_digits="
              << (series.working_precision_digits - requested_precision_digits)
              << ", emitted_orders=[" << min_eps_order << "," << max_eps_order
              << "], coefficients are real for real eps and positive loop count; "
              << "weighted residue integration and coefficient publication remain "
              << "deferred.";
  series.precision_diagnostics = diagnostics.str();

  CutkoskyPrefactorFloat coefficient = normalization;
  for (int order = 0; order <= max_eps_order; ++order) {
    if (order > 0) {
      coefficient *= exponential_scale;
      coefficient /= order;
    }
    if (order < min_eps_order) {
      continue;
    }
    series.terms.push_back(
        {order, FormatCutkoskyPrefactorFloat(coefficient, requested_precision_digits),
         "0"});
  }
  return series;
}

std::vector<CutkoskyPrefactorSeriesTerm>
MultiplyCutkoskyPrefactorIntoLaurentSeries(
    const CutkoskyPrefactorSeries& prefactor,
    const std::vector<CutkoskyPrefactorSeriesTerm>& residue_series,
    const int min_eps_order,
    const int max_eps_order) {
  if (prefactor.loop_count == 0 || prefactor.terms.empty()) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires a non-empty prefactor series");
  }
  if (prefactor.requested_precision_digits < 16 ||
      prefactor.requested_precision_digits > 90) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires a prefactor precision in "
        "[16, 90] decimal digits");
  }
  if (prefactor.min_eps_order != 0) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires prefactor terms from eps^0");
  }
  if (residue_series.empty()) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires explicit residue terms");
  }
  if (max_eps_order < min_eps_order) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication requires max_eps_order >= min_eps_order");
  }

  int residue_min_order = residue_series.front().eps_order;
  for (const CutkoskyPrefactorSeriesTerm& term : residue_series) {
    residue_min_order = std::min(residue_min_order, term.eps_order);
  }
  const int needed_prefactor_order = max_eps_order - residue_min_order;
  if (prefactor.max_eps_order < needed_prefactor_order) {
    throw std::invalid_argument(
        "b63n Cutkosky prefactor multiplication needs prefactor terms through eps^" +
        std::to_string(needed_prefactor_order) + " for the requested output range");
  }

  std::map<int, CutkoskyComplexCoefficient> output_terms;
  for (const CutkoskyPrefactorSeriesTerm& prefactor_term : prefactor.terms) {
    const CutkoskyComplexCoefficient k =
        ParseCutkoskySeriesTerm(prefactor_term, "prefactor");
    for (const CutkoskyPrefactorSeriesTerm& residue_term : residue_series) {
      const int order = prefactor_term.eps_order + residue_term.eps_order;
      if (order < min_eps_order || order > max_eps_order) {
        continue;
      }
      const CutkoskyComplexCoefficient residue =
          ParseCutkoskySeriesTerm(residue_term, "residue");
      CutkoskyComplexCoefficient& target = output_terms[order];
      target.real += k.real * residue.real - k.imaginary * residue.imaginary;
      target.imaginary += k.real * residue.imaginary + k.imaginary * residue.real;
    }
  }

  std::vector<CutkoskyPrefactorSeriesTerm> multiplied;
  multiplied.reserve(output_terms.size());
  for (const auto& [order, coefficient] : output_terms) {
    if (coefficient.real == 0 && coefficient.imaginary == 0) {
      continue;
    }
    multiplied.push_back(
        {order,
         FormatCutkoskyPrefactorFloat(coefficient.real,
                                      prefactor.requested_precision_digits),
         coefficient.imaginary == 0
             ? "0"
             : FormatCutkoskyPrefactorFloat(coefficient.imaginary,
                                            prefactor.requested_precision_digits)});
  }
  return multiplied;
}

CutkoskyResidueSeries MultiplyCutkoskyPrefactorIntoResidueSeries(
    const CutkoskyPrefactorSeries& prefactor,
    const CutkoskyResidueSeries& residue_series,
    const int min_eps_order,
    const int max_eps_order) {
  ValidateCutkoskyPrefactorForMultiplication(prefactor);
  if (residue_series.terms.empty()) {
    throw std::invalid_argument(
        "b63n Cutkosky residue-series prefactor multiplication requires explicit "
        "residue terms");
  }
  if (max_eps_order < min_eps_order) {
    throw std::invalid_argument(
        "b63n Cutkosky residue-series prefactor multiplication requires "
        "max_eps_order >= min_eps_order");
  }

  int residue_min_order = residue_series.terms.front().eps_order;
  for (const CutkoskyResidueSeriesTerm& term : residue_series.terms) {
    residue_min_order = std::min(residue_min_order, term.eps_order);
  }
  const int needed_prefactor_order = max_eps_order - residue_min_order;
  if (prefactor.max_eps_order < needed_prefactor_order) {
    throw std::invalid_argument(
        "b63n Cutkosky residue-series prefactor multiplication needs prefactor terms "
        "through eps^" +
        std::to_string(needed_prefactor_order) + " for the requested output range");
  }

  struct AccumulatedResidueTerm {
    CutkoskyResidueSeriesTerm term;
    CutkoskyComplexCoefficient coefficient;
  };
  std::map<CutkoskyResidueSeriesTermKey, AccumulatedResidueTerm> output_terms;
  for (const CutkoskyPrefactorSeriesTerm& prefactor_term : prefactor.terms) {
    const CutkoskyComplexCoefficient k =
        ParseCutkoskySeriesTerm(prefactor_term, "prefactor");
    for (const CutkoskyResidueSeriesTerm& residue_term : residue_series.terms) {
      const int order = prefactor_term.eps_order + residue_term.eps_order;
      if (order < min_eps_order || order > max_eps_order) {
        continue;
      }
      const CutkoskyComplexCoefficient residue =
          ParseCutkoskyResidueSeriesTermCoefficient(residue_term, "residue");
      const CutkoskyResidueSeriesTermKey key{
          order,
          residue_term.eta_power,
          residue_term.log_power,
          residue_term.region_key.empty() ? "integer" : residue_term.region_key};
      AccumulatedResidueTerm& target = output_terms[key];
      if (target.term.coefficient_label.empty() && target.term.provenance.source.empty()) {
        target.term = residue_term;
        target.term.eps_order = order;
        target.term.region_key = key.region_key;
        target.term.precision.requested_precision_digits =
            prefactor.requested_precision_digits;
        target.term.precision.working_precision_digits =
            prefactor.working_precision_digits;
        target.term.precision.arithmetic_backend = "cpp_dec_float_100";
        target.term.precision.summary =
            "b63n explicit residue-series term multiplied by reviewed K_r(eps) "
            "prefactor; coefficient publication remains deferred";
        target.term.provenance.coefficient_published = false;
      } else {
        target.term.coefficient_label = MergeCutkoskyResidueCoefficientLabels(
            target.term.coefficient_label, residue_term.coefficient_label);
        target.term.provenance.fixture_id = MergeCutkoskyResidueCoefficientLabels(
            target.term.provenance.fixture_id, residue_term.provenance.fixture_id);
        target.term.provenance.synthetic_fixture =
            target.term.provenance.synthetic_fixture ||
            residue_term.provenance.synthetic_fixture;
        target.term.provenance.retained_solution_samples_used =
            target.term.provenance.retained_solution_samples_used ||
            residue_term.provenance.retained_solution_samples_used;
        target.term.provenance.coefficient_published = false;
      }
      target.coefficient.real += k.real * residue.real - k.imaginary * residue.imaginary;
      target.coefficient.imaginary +=
          k.real * residue.imaginary + k.imaginary * residue.real;
    }
  }

  CutkoskyResidueSeries multiplied;
  multiplied.series_label = residue_series.series_label.empty()
                                ? "prefactor-multiplied-residue-series"
                                : residue_series.series_label + "::prefactor";
  multiplied.expansion_variable = residue_series.expansion_variable;
  multiplied.eta_variable = residue_series.eta_variable;
  multiplied.min_eps_order = min_eps_order;
  multiplied.max_eps_order = max_eps_order;
  multiplied.requested_precision_digits = prefactor.requested_precision_digits;
  multiplied.working_precision_digits = prefactor.working_precision_digits;
  multiplied.precision_diagnostics =
      "b63n residue-series carrier multiplied by reviewed K_r(eps) prefactor; "
      "requested_digits=" +
      std::to_string(prefactor.requested_precision_digits) +
      ", working_digits=" + std::to_string(prefactor.working_precision_digits) +
      ", retained_solution_samples_used=false for synthetic fixtures, coefficient "
      "publication remains deferred";

  for (auto& entry : output_terms) {
    AccumulatedResidueTerm& accumulated = entry.second;
    if (IsZeroCutkoskyCoefficient(accumulated.coefficient)) {
      continue;
    }
    accumulated.term.coefficient.real = FormatCutkoskyPrefactorFloat(
        accumulated.coefficient.real, prefactor.requested_precision_digits);
    accumulated.term.coefficient.imaginary =
        accumulated.coefficient.imaginary == 0
            ? "0"
            : FormatCutkoskyPrefactorFloat(accumulated.coefficient.imaginary,
                                           prefactor.requested_precision_digits);
    multiplied.terms.push_back(accumulated.term);
  }
  return multiplied;
}

std::vector<CutkoskyEtaZeroTerm> ProjectCutkoskyResidueSeriesToEtaZeroTerms(
    const CutkoskyResidueSeries& series,
    const int eps_order) {
  std::vector<CutkoskyEtaZeroTerm> projected;
  for (const CutkoskyResidueSeriesTerm& residue_term : series.terms) {
    if (residue_term.eps_order != eps_order) {
      continue;
    }
    const CutkoskyComplexCoefficient coefficient =
        ParseCutkoskyResidueSeriesTermCoefficient(residue_term, "residue");
    if (IsZeroCutkoskyCoefficient(coefficient)) {
      continue;
    }
    projected.push_back({residue_term.region_key.empty() ? "integer"
                                                         : residue_term.region_key,
                         residue_term.eta_power,
                         residue_term.log_power,
                         residue_term.coefficient_label});
  }
  return projected;
}

std::string SerializeCutkoskyBranchLedgerEntrySummary(
    const CutkoskyBranchLedgerEntry& entry) {
  return entry.summary;
}

std::vector<std::string> SerializeCutkoskyBranchLedgerSummaries(
    const std::vector<CutkoskyBranchLedgerEntry>& ledger) {
  std::vector<std::string> summaries;
  summaries.reserve(ledger.size());
  for (const CutkoskyBranchLedgerEntry& entry : ledger) {
    summaries.push_back(SerializeCutkoskyBranchLedgerEntrySummary(entry));
  }
  return summaries;
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
  ClassifyReviewedSurface(spec, cut_indices, cut_prescription, audit);
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

  ClassifyReviewedSurface(spec, cut_indices, cut_prescription, audit);
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

CutkoskyResidueCoefficientAudit BuildAutomaticPhaseSpaceFirstCutkoskyCoefficientAudit(
    const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw BoundaryUnsolvedError(
        "b63n first Cutkosky coefficient evaluator requires cut propagators");
  }
  RejectEtaOnCutPropagators(spec, cut_indices);
  static_cast<void>(CollectCutPowers(spec, cut_indices));

  const CutkoskyPhaseSpaceTopology topology =
      AnalyzeCutkoskyPhaseSpaceCutTopology(spec);
  if (topology.cut_components.size() != 1) {
    throw BoundaryUnsolvedError(
        "b63n first Cutkosky coefficient evaluator requires a single phase-volume "
        "cut component");
  }
  const CutkoskyPhaseSpaceCutComponent& component = topology.cut_components.front();
  if (component.cut_propagator_indices.size() != component.loop_momenta.size() + 1) {
    throw BoundaryUnsolvedError(
        "b63n first Cutkosky coefficient evaluator requires phase-volume cut count "
        "to equal loop count plus one");
  }

  const FeynmanPrescription cut_prescription = ResolveCutPrescription(spec, cut_indices);
  if (!MatchesAutomaticPhaseSpaceFirstCoefficientSurface(spec, cut_indices)) {
    throw BoundaryUnsolvedError(
        "b63n first Cutkosky coefficient evaluator is limited to "
        "automatic_phasespace phase[1,0,1,0,1,0,0]");
  }

  const CutkoskyResidueEndpointModel endpoint_model =
      BuildAutomaticPhaseSpaceFirstCoefficientEndpointModel(cut_prescription);
  const CutkoskyEtaZeroSelectionResult selection =
      PickCutkoskyEtaZeroTerm({{"integer", 0, 0, spec.targets.front().Label()}});
  if (!selection.success) {
    throw BoundaryUnsolvedError(selection.summary);
  }

  CutkoskyResidueCoefficientAudit audit;
  audit.live_coefficients_available = true;
  audit.retained_solution_samples_used = false;
  audit.full_eta_zero_contour_applied = false;
  audit.master_label = spec.targets.front().Label();
  audit.runtime_application =
      "b63n-cutkosky-pure-phase-volume-selected-residue";
  audit.transport_scope = "eta-zero-selected-endpoint-coefficients";
  audit.residue_model_kind = endpoint_model.model_kind;
  audit.endpoint_local_model_kind = endpoint_model.endpoint_local_model_kind;
  audit.contour_fingerprint = endpoint_model.contour_fingerprint;
  audit.eta_zero_selection_audit =
      selection.summary + "; selected_coefficient_label=" +
      selection.selected_coefficient_label;
  audit.extraction_fingerprint =
      ComputeArtifactFingerprint(
          SerializeCutkoskyResidueCoefficientAuditForFingerprint(audit));
  audit.summary =
      "Applied b63n automatic_phasespace pure Cutkosky residue endpoint transport for " +
      audit.master_label +
      " from retained eta-infinity Cutkosky boundary samples without reading final "
      "solution samples; selected the unique eta^0 pure phase-volume residue, "
      "residue_model_kind=" + audit.residue_model_kind +
      ", endpoint_local_model_kind=" + audit.endpoint_local_model_kind +
      ", contour_fingerprint=" + audit.contour_fingerprint +
      ", extraction_fingerprint=" + audit.extraction_fingerprint +
      ", final_solution_samples_used_as_input=false. Full weighted "
      "automatic_phasespace residues and feynman_prescription Cutkosky residues remain "
      "deferred; full_eta_zero_contour_applied stays false.";
  return audit;
}

CutkoskySymbolicIntegrand BuildAutomaticPhaseSpaceSymbolicIntegrand(
    const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw BoundaryUnsolvedError(
        "b63n automatic phase-space symbolic integrand requires cut propagators");
  }
  RejectEtaOnCutPropagators(spec, cut_indices);
  static_cast<void>(CollectCutPowers(spec, cut_indices));
  if (!MatchesAutomaticPhaseSpaceSurface(spec, cut_indices)) {
    throw BoundaryUnsolvedError(
        "b63n automatic phase-space symbolic integrand is limited to "
        "phase[1,2,1,1,1,1,1]");
  }

  const FeynmanPrescription cut_prescription = ResolveCutPrescription(spec, cut_indices);
  const CutkoskyResidueEndpointModel endpoint_model =
      BuildCutkoskyResidueEndpointModelInternal(spec, cut_indices, cut_prescription);
  if (endpoint_model.uncut_denominator_indices.size() !=
      endpoint_model.uncut_denominator_roles.size()) {
    throw BoundaryUnsolvedError(
        "b63n automatic phase-space symbolic integrand requires one recorded role per "
        "uncut denominator");
  }

  CutkoskySymbolicIntegrand integrand;
  integrand.surface_label = spec.targets.front().Label();
  integrand.model_kind = endpoint_model.model_kind;
  integrand.phase_space_parameterization =
      endpoint_model.phase_space_parameterization;
  integrand.physical_integration_domain =
      endpoint_model.physical_integration_domain;
  integrand.residue_variables = endpoint_model.residue_variables;
  integrand.coefficient_policy =
      "coefficient-free symbolic assembly; no endpoint Laurent coefficients evaluated "
      "or published";
  integrand.factors.reserve(endpoint_model.uncut_denominator_indices.size());
  for (std::size_t role_index = 0;
       role_index < endpoint_model.uncut_denominator_indices.size();
       ++role_index) {
    const std::size_t denominator_index =
        endpoint_model.uncut_denominator_indices[role_index];
    if (denominator_index >= spec.family.propagators.size() ||
        denominator_index >= spec.targets.front().indices.size()) {
      throw BoundaryUnsolvedError(
          "b63n automatic phase-space symbolic integrand found an out-of-range "
          "uncut denominator index");
    }
    const std::string denominator_id =
        "D" + std::to_string(denominator_index + 1);
    ValidateAutomaticPhaseSpaceSymbolicFactorRole(
        endpoint_model, role_index, denominator_id);
    const int propagator_power = spec.targets.front().indices[denominator_index];
    if (propagator_power <= 0) {
      throw BoundaryUnsolvedError(
          "b63n automatic phase-space symbolic integrand requires positive uncut "
          "denominator powers on the reviewed surface");
    }
    integrand.factors.push_back(
        {denominator_id,
         denominator_index,
         propagator_power,
         spec.family.propagators[denominator_index].expression,
         endpoint_model.uncut_denominator_roles[role_index],
         AutomaticPhaseSpaceStructuralFormForDenominator(denominator_id)});
  }
  return integrand;
}

std::string SerializeCutkoskySymbolicIntegrandAudit(
    const CutkoskySymbolicIntegrand& integrand) {
  std::ostringstream out;
  out << "kind=b63n-automatic-phasespace-symbolic-integrand\n";
  out << "surface=" << integrand.surface_label << "\n";
  out << "model=" << integrand.model_kind << "\n";
  out << "parameterization=" << integrand.phase_space_parameterization << "\n";
  out << "domain=" << integrand.physical_integration_domain << "\n";
  out << "variables=";
  for (std::size_t index = 0; index < integrand.residue_variables.size();
       ++index) {
    if (index != 0) {
      out << ",";
    }
    out << integrand.residue_variables[index];
  }
  out << "\n";
  out << "coefficient_policy=" << integrand.coefficient_policy << "\n";
  out << "factor_count=" << integrand.factors.size() << "\n";
  for (std::size_t index = 0; index < integrand.factors.size(); ++index) {
    const CutkoskySymbolicIntegrandFactor& factor = integrand.factors[index];
    out << "factor[" << index << "]="
        << "denominator=" << factor.denominator_id
        << ";denominator_index=" << factor.denominator_index
        << ";power=" << factor.propagator_power
        << ";form=" << factor.structural_form
        << ";role=" << factor.role
        << ";propagator=" << factor.propagator_expression << "\n";
  }
  return out.str();
}

CutkoskySymbolicSubintegralAssembly
BuildFeynmanPrescriptionSymbolicSubintegralAssembly(const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw BoundaryUnsolvedError(
        "b63n feynman_prescription symbolic subintegral assembly requires cut "
        "propagators");
  }
  RejectEtaOnCutPropagators(spec, cut_indices);
  static_cast<void>(CollectCutPowers(spec, cut_indices));
  if (!MatchesFeynmanPrescriptionSurface(spec, cut_indices)) {
    throw BoundaryUnsolvedError(
        "b63n feynman_prescription symbolic subintegral assembly is limited to the "
        "exact feynman_prescription reviewed surface "
        "loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]");
  }

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
        "b63n feynman_prescription symbolic subintegral assembly recognizes only the "
        "reviewed plus-minus and minus-plus loop-prescription ledgers");
  }

  const FeynmanPrescription cut_prescription = ResolveCutPrescription(spec, cut_indices);
  const CutkoskyEtaZeroTransportAudit transport_audit =
      BuildCutkoskyEtaZeroTransportScaffold(spec);
  const CutkoskyBranchLedgerEntry& subintegral_ledger =
      RequireFeynmanSubintegralLedger(transport_audit);
  const CutkoskyResidueEndpointModel endpoint_model =
      BuildCutkoskyResidueEndpointModelInternal(spec, cut_indices, cut_prescription);
  if (endpoint_model.uncut_denominator_indices.size() !=
      endpoint_model.uncut_denominator_roles.size()) {
    throw BoundaryUnsolvedError(
        "b63n feynman_prescription symbolic subintegral assembly requires one recorded "
        "role per uncut subintegral denominator");
  }

  CutkoskySymbolicSubintegralAssembly assembly;
  assembly.live_coefficients_available = false;
  assembly.retained_solution_samples_used = false;
  assembly.surface_label = spec.targets.front().Label();
  assembly.model_kind = endpoint_model.model_kind;
  assembly.phase_space_parameterization =
      endpoint_model.phase_space_parameterization;
  assembly.physical_integration_domain = endpoint_model.physical_integration_domain;
  assembly.cut_denominator_indices = cut_indices;
  assembly.residue_variables = endpoint_model.residue_variables;
  assembly.branch_ledger_summary =
      SerializeCutkoskyBranchLedgerEntrySummary(subintegral_ledger);
  assembly.coefficient_policy =
      "coefficient-free symbolic assembly; no endpoint Laurent coefficients evaluated "
      "or published";
  assembly.subintegrals.push_back(
      MakeFeynmanSubintegralCarrier(subintegral_ledger, "T_l1"));
  assembly.subintegrals.push_back(
      MakeFeynmanSubintegralCarrier(subintegral_ledger, "T_l2"));

  for (std::size_t role_index = 0;
       role_index < endpoint_model.uncut_denominator_indices.size();
       ++role_index) {
    const std::size_t denominator_index =
        endpoint_model.uncut_denominator_indices[role_index];
    if (denominator_index >= spec.family.propagators.size() ||
        denominator_index >= spec.targets.front().indices.size()) {
      throw BoundaryUnsolvedError(
          "b63n feynman_prescription symbolic subintegral assembly found an "
          "out-of-range uncut denominator index");
    }
    const std::string denominator_id =
        PropagatorDisplayLabel(denominator_index);
    const std::string ledger_handle =
        FeynmanSubintegralHandleForDenominator(denominator_id);
    CutkoskySymbolicSubintegral& subintegral =
        RequireMutableSubintegralByHandle(assembly.subintegrals, ledger_handle);
    ValidateFeynmanSubintegralFactorRole(endpoint_model,
                                         role_index,
                                         denominator_id,
                                         ledger_handle,
                                         subintegral.ledger_sign);
    const int propagator_power = spec.targets.front().indices[denominator_index];
    if (propagator_power <= 0) {
      throw BoundaryUnsolvedError(
          "b63n feynman_prescription symbolic subintegral assembly requires positive "
          "powers for reviewed T_l1/T_l2 denominators");
    }
    subintegral.factors.push_back(
        {subintegral.ledger_handle,
         subintegral.ledger_sign,
         denominator_id,
         denominator_index,
         propagator_power,
         spec.family.propagators[denominator_index].expression,
         endpoint_model.uncut_denominator_roles[role_index],
         FeynmanSubintegralStructuralFormForDenominator(
             denominator_id, ledger_handle, subintegral.ledger_sign)});
  }

  return assembly;
}

std::string SerializeCutkoskySymbolicSubintegralAssemblyAudit(
    const CutkoskySymbolicSubintegralAssembly& assembly) {
  std::ostringstream out;
  out << "kind=b63n-feynman-prescription-symbolic-subintegral-assembly\n";
  out << "live_coefficients_available="
      << (assembly.live_coefficients_available ? "true" : "false") << "\n";
  out << "retained_solution_samples_used="
      << (assembly.retained_solution_samples_used ? "true" : "false") << "\n";
  out << "surface=" << assembly.surface_label << "\n";
  out << "model=" << assembly.model_kind << "\n";
  out << "parameterization=" << assembly.phase_space_parameterization << "\n";
  out << "domain=" << assembly.physical_integration_domain << "\n";
  out << "cut_denominators="
      << JoinPropagatorDisplayLabels(assembly.cut_denominator_indices) << "\n";
  out << "variables=";
  for (std::size_t index = 0; index < assembly.residue_variables.size();
       ++index) {
    if (index != 0) {
      out << ",";
    }
    out << assembly.residue_variables[index];
  }
  out << "\n";
  out << "branch_ledger=" << assembly.branch_ledger_summary << "\n";
  out << "coefficient_policy=" << assembly.coefficient_policy << "\n";
  out << "subintegral_count=" << assembly.subintegrals.size() << "\n";
  for (std::size_t subintegral_index = 0;
       subintegral_index < assembly.subintegrals.size();
       ++subintegral_index) {
    const CutkoskySymbolicSubintegral& subintegral =
        assembly.subintegrals[subintegral_index];
    out << "subintegral[" << subintegral_index << "]="
        << "handle=" << subintegral.ledger_handle
        << ";loop=" << subintegral.loop_momentum
        << ";prescription=" << subintegral.ledger_sign
        << ";prescription_source=" << subintegral.prescription_source
        << ";factor_count=" << subintegral.factors.size() << "\n";
    for (std::size_t factor_index = 0;
         factor_index < subintegral.factors.size();
         ++factor_index) {
      const CutkoskySymbolicSubintegralFactor& factor =
          subintegral.factors[factor_index];
      out << "subintegral[" << subintegral_index << "].factor["
          << factor_index << "]="
          << "ledger_handle=" << factor.ledger_handle
          << ";ledger_sign=" << factor.ledger_sign
          << ";denominator=" << factor.denominator_id
          << ";denominator_index=" << factor.denominator_index
          << ";power=" << factor.propagator_power
          << ";form=" << factor.structural_form
          << ";role=" << factor.role
          << ";propagator=" << factor.propagator_expression << "\n";
    }
  }
  return out.str();
}

CutkoskyWeightedResidueEvaluationPlan
BuildCutkoskyWeightedResidueEvaluationPlan(const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }

  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky weighted residue evaluation plan requires exact "
        "automatic_phasespace and feynman_prescription weighted residue surfaces");
  }
  RejectEtaOnCutPropagators(spec, cut_indices);
  static_cast<void>(CollectCutPowers(spec, cut_indices));

  if (MatchesAutomaticPhaseSpaceFirstCoefficientSurface(spec, cut_indices)) {
    throw BoundaryUnsolvedError(
        "b63n Cutkosky weighted residue evaluation plan rejects selected pure-cut "
        "coefficient surfaces; use the selected pure-cut coefficient evaluator instead");
  }

  CutkoskyWeightedResidueEvaluationPlan plan;
  plan.reviewed_surface = true;
  plan.coefficient_free = true;
  plan.live_coefficients_available = false;
  plan.retained_solution_samples_used = false;
  plan.full_eta_zero_contour_applied = false;
  plan.requires_moment_reduction = true;
  plan.requires_branch_ledger_validation = true;
  plan.requires_endpoint_laurent_series = true;
  plan.requires_external_cas_validation = true;
  plan.coefficient_policy = "coefficient-free";

  if (MatchesAutomaticPhaseSpaceSurface(spec, cut_indices)) {
    const CutkoskySymbolicIntegrand integrand =
        BuildAutomaticPhaseSpaceSymbolicIntegrand(spec);
    const CutkoskyEtaZeroTransportAudit transport_audit =
        BuildCutkoskyEtaZeroTransportScaffold(spec);
    plan.surface_label = integrand.surface_label;
    plan.residue_model_kind = integrand.model_kind;
    plan.phase_space_parameterization = integrand.phase_space_parameterization;
    plan.physical_integration_domain = integrand.physical_integration_domain;
    plan.cut_denominator_indices = cut_indices;
    plan.residue_variables = integrand.residue_variables;
    if (!transport_audit.branch_ledger_entries.empty()) {
      plan.branch_ledger_summary = transport_audit.branch_ledger_entries.back();
    }
    plan.uncut_denominator_indices.reserve(integrand.factors.size());
    plan.uncut_denominator_roles.reserve(integrand.factors.size());
    for (const CutkoskySymbolicIntegrandFactor& factor : integrand.factors) {
      plan.uncut_denominator_indices.push_back(factor.denominator_index);
      plan.uncut_denominator_roles.push_back(factor.role);
    }
    return plan;
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
          "b63n Cutkosky weighted residue evaluation plan requires exact "
          "automatic_phasespace and feynman_prescription weighted residue surfaces, "
          "with feynman_prescription restricted to reviewed plus/minus or minus/plus "
          "loop-prescription assignments");
    }

    const CutkoskySymbolicSubintegralAssembly assembly =
        BuildFeynmanPrescriptionSymbolicSubintegralAssembly(spec);
    plan.requires_feynman_conjugate_validation = true;
    plan.surface_label = assembly.surface_label;
    plan.residue_model_kind = assembly.model_kind;
    plan.conjugate_residue_model_kind =
        plus_minus ? "feynman_prescription::two-body-residue::minus-plus"
                   : "feynman_prescription::two-body-residue::plus-minus";
    plan.phase_space_parameterization = assembly.phase_space_parameterization;
    plan.physical_integration_domain = assembly.physical_integration_domain;
    plan.cut_denominator_indices = assembly.cut_denominator_indices;
    plan.residue_variables = assembly.residue_variables;
    plan.branch_ledger_summary = assembly.branch_ledger_summary;
    for (const CutkoskySymbolicSubintegral& subintegral : assembly.subintegrals) {
      for (const CutkoskySymbolicSubintegralFactor& factor : subintegral.factors) {
        plan.uncut_denominator_indices.push_back(factor.denominator_index);
        plan.uncut_denominator_roles.push_back(factor.role);
      }
    }
    return plan;
  }

  throw BoundaryUnsolvedError(
      "b63n Cutkosky weighted residue evaluation plan requires exact "
      "automatic_phasespace and feynman_prescription weighted residue surfaces");
}

std::string SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
    const CutkoskyWeightedResidueEvaluationPlan& plan) {
  std::ostringstream out;
  out << "kind=b63n-cutkosky-weighted-residue-evaluation-plan\n";
  out << "reviewed_surface=" << (plan.reviewed_surface ? "true" : "false") << "\n";
  out << "surface=" << plan.surface_label << "\n";
  out << "residue_model_kind=" << plan.residue_model_kind << "\n";
  out << "coefficient_policy="
      << (plan.coefficient_policy.empty() ? "coefficient-free"
                                          : plan.coefficient_policy)
      << "\n";
  out << "live_coefficients_available="
      << (plan.live_coefficients_available ? "true" : "false") << "\n";
  out << "retained_solution_samples_used="
      << (plan.retained_solution_samples_used ? "true" : "false") << "\n";
  out << "full_eta_zero_contour_applied="
      << (plan.full_eta_zero_contour_applied ? "true" : "false") << "\n";
  out << "parameterization=" << plan.phase_space_parameterization << "\n";
  out << "domain=" << plan.physical_integration_domain << "\n";
  out << "cut_denominators="
      << JoinPropagatorDisplayLabels(plan.cut_denominator_indices) << "\n";
  out << "moment_weights="
      << JoinPropagatorDisplayLabels(plan.uncut_denominator_indices) << "\n";
  out << "variables=";
  for (std::size_t index = 0; index < plan.residue_variables.size(); ++index) {
    if (index != 0) {
      out << ",";
    }
    out << plan.residue_variables[index];
  }
  out << "\n";
  for (std::size_t index = 0; index < plan.uncut_denominator_roles.size(); ++index) {
    out << "moment_weight[" << index << "]=" << plan.uncut_denominator_roles[index]
        << "\n";
  }
  if (!plan.branch_ledger_summary.empty()) {
    out << "branch_ledger=" << plan.branch_ledger_summary << "\n";
  }
  if (!plan.conjugate_residue_model_kind.empty()) {
    out << "conjugate_partner=" << plan.conjugate_residue_model_kind << "\n";
  }
  if (plan.requires_moment_reduction) {
    out << "required_validation=moment-reduction\n";
  }
  if (plan.requires_branch_ledger_validation) {
    out << "required_validation=branch-ledger\n";
  }
  if (plan.requires_endpoint_laurent_series) {
    out << "required_validation=endpoint-Laurent\n";
  }
  if (plan.requires_external_cas_validation) {
    out << "required_validation=external-CAS\n";
  }
  if (plan.requires_feynman_conjugate_validation) {
    out << "required_validation=feynman-conjugate\n";
  }
  return out.str();
}

}  // namespace amflow
