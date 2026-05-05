#include "amflow/runtime/endpoint_branch_ledger.hpp"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "amflow/runtime/artifact_store.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

namespace {

ExactRational ZeroRational() {
  return {"0", "1"};
}

ExactRational ExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, NumericEvaluationPoint{});
}

ExactRational SubtractRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic("(" + lhs.ToString() + ")-(" + rhs.ToString() + ")");
}

int CompareRational(const ExactRational& lhs, const ExactRational& rhs) {
  const ExactRational difference = SubtractRational(lhs, rhs);
  if (difference.IsZero()) {
    return 0;
  }
  return !difference.numerator.empty() && difference.numerator.front() == '-' ? -1 : 1;
}

bool SameCanonicalRational(const ExactRational& lhs, const ExactRational& rhs) {
  try {
    return ExactArithmetic(lhs.ToString()) == ExactArithmetic(rhs.ToString());
  } catch (const std::exception&) {
    return false;
  }
}

bool IsZeroComplexEndpoint(const ExactComplexRational& value) {
  return SameCanonicalRational(value.real, ZeroRational()) &&
         SameCanonicalRational(value.imaginary, ZeroRational());
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

EtaEndpointBranchLedgerAnalysis MakeFailure(std::string failure_code,
                                            std::string summary) {
  EtaEndpointBranchLedgerAnalysis analysis;
  analysis.success = false;
  analysis.failure_code = std::move(failure_code);
  analysis.summary = std::move(summary);
  return analysis;
}

bool SameEndpoint(const EtaContourSingularPoint& endpoint_singular,
                  const EtaEndpointLocalModel& local_model) {
  return endpoint_singular.expression == local_model.endpoint_expression &&
         endpoint_singular.value == local_model.endpoint_value;
}

void AddResolvedPrescription(std::vector<FeynmanPrescription>& prescriptions,
                             const FeynmanPrescription prescription) {
  if (prescription == FeynmanPrescription::None) {
    return;
  }
  prescriptions.push_back(prescription);
}

std::optional<FeynmanPrescription> UniquePrescription(
    const std::vector<FeynmanPrescription>& prescriptions) {
  if (prescriptions.empty()) {
    return std::nullopt;
  }
  const FeynmanPrescription first = prescriptions.front();
  for (const FeynmanPrescription prescription : prescriptions) {
    if (prescription != first) {
      return std::nullopt;
    }
  }
  return first;
}

struct ResolvedPrescription {
  FeynmanPrescription prescription = FeynmanPrescription::None;
  std::string source;
};

EtaEndpointBranchLedgerAnalysis ResolveEndpointPrescription(
    const ProblemSpec& spec,
    ResolvedPrescription& resolved) {
  std::vector<FeynmanPrescription> loop_prescriptions;
  loop_prescriptions.reserve(spec.family.loop_prescriptions.size());
  for (const FeynmanPrescription prescription : spec.family.loop_prescriptions) {
    AddResolvedPrescription(loop_prescriptions, prescription);
  }

  std::vector<FeynmanPrescription> raw_propagator_prescriptions;
  raw_propagator_prescriptions.reserve(spec.family.propagators.size());
  for (const Propagator& propagator : spec.family.propagators) {
    const std::optional<FeynmanPrescription> prescription =
        ParseFeynmanPrescription(propagator.prescription);
    if (!prescription.has_value()) {
      return MakeFailure(
          "srl3_endpoint_prescription_invalid",
          "SRL-3 endpoint branch ledger requires raw propagator prescriptions to use the "
          "reviewed -1/0/1 Feynman prescription vocabulary");
    }
    AddResolvedPrescription(raw_propagator_prescriptions, *prescription);
  }

  const std::optional<FeynmanPrescription> unique_loop =
      UniquePrescription(loop_prescriptions);
  if (!loop_prescriptions.empty() && !unique_loop.has_value()) {
    return MakeFailure(
        "srl3_endpoint_prescription_contradiction",
        "SRL-3 endpoint branch ledger found contradictory nonzero "
        "family.loop_prescriptions metadata");
  }

  const std::optional<FeynmanPrescription> unique_raw =
      UniquePrescription(raw_propagator_prescriptions);
  if (!raw_propagator_prescriptions.empty() && !unique_raw.has_value()) {
    return MakeFailure(
        "srl3_endpoint_prescription_contradiction",
        "SRL-3 endpoint branch ledger found contradictory nonzero raw propagator "
        "prescriptions");
  }

  if (unique_loop.has_value() && unique_raw.has_value() && *unique_loop != *unique_raw) {
    return MakeFailure(
        "srl3_endpoint_prescription_contradiction",
        "SRL-3 endpoint branch ledger found contradictory loop-prescription and raw "
        "propagator prescription metadata");
  }

  if (unique_loop.has_value()) {
    resolved.prescription = *unique_loop;
    resolved.source = "family.loop_prescriptions";
    return {};
  }
  if (unique_raw.has_value()) {
    resolved.prescription = *unique_raw;
    resolved.source = "family.propagators[].prescription";
    return {};
  }

  return MakeFailure(
      "srl3_endpoint_prescription_missing",
      "SRL-3 endpoint branch ledger requires a nonzero Feynman prescription to select "
      "the eta=0 logarithm and power branch");
}

EtaContourHalfPlane RequiredHalfPlaneForPrescription(
    const FeynmanPrescription prescription) {
  return prescription == FeynmanPrescription::PlusI0 ? EtaContourHalfPlane::Upper
                                                     : EtaContourHalfPlane::Lower;
}

std::string ApproachDirectionForHalfPlane(const EtaContourHalfPlane half_plane) {
  return half_plane == EtaContourHalfPlane::Upper ? "PosIm" : "NegIm";
}

std::string LogBranchArgumentForHalfPlane(const EtaContourHalfPlane half_plane) {
  return half_plane == EtaContourHalfPlane::Upper ? "+pi" : "-pi";
}

int LogSheetIndexForHalfPlane(const EtaContourHalfPlane half_plane) {
  return half_plane == EtaContourHalfPlane::Upper ? 0 : -1;
}

std::string SerializeEndpointBranchLedgerForFingerprint(
    const EtaEndpointBranchLedger& ledger) {
  std::ostringstream out;
  out << "eta_symbol=" << ledger.eta_symbol << "\n";
  out << "endpoint_expression=" << ledger.endpoint_expression << "\n";
  out << "endpoint_value=" << ledger.endpoint_value.ToString() << "\n";
  out << "half_plane=" << ToString(ledger.half_plane) << "\n";
  out << "prescription=" << ToString(ledger.prescription) << "\n";
  out << "prescription_source=" << ledger.prescription_source << "\n";
  out << "approach_direction=" << ledger.approach_direction << "\n";
  out << "log_branch_argument=" << ledger.log_branch_argument << "\n";
  out << "log_sheet_index=" << ledger.log_sheet_index << "\n";
  out << "endpoint_branch_winding=" << ledger.endpoint_branch_winding << "\n";
  out << "contour_fingerprint=" << ledger.contour_fingerprint << "\n";
  out << "local_model_kind=" << ledger.local_model_kind << "\n";
  out << "extraction_order=" << ledger.extraction_order << "\n";
  out << "live_endpoint_extraction_ready="
      << (ledger.live_endpoint_extraction_ready ? "true" : "false") << "\n";
  return out.str();
}

}  // namespace

std::string ToString(const FeynmanPrescription prescription) {
  switch (prescription) {
    case FeynmanPrescription::MinusI0:
      return "-i0";
    case FeynmanPrescription::None:
      return "none";
    case FeynmanPrescription::PlusI0:
      return "+i0";
  }
  return "none";
}

EtaEndpointBranchLedgerAnalysis AnalyzeEtaEndpointBranchLedger(
    const ProblemSpec& spec,
    const EtaContinuationPlan& plan,
    const EtaEndpointLocalModel& local_model) {
  const std::vector<std::string> spec_messages = ValidateProblemSpec(spec);
  if (!spec_messages.empty()) {
    return MakeFailure("srl3_endpoint_problem_spec_invalid", JoinMessages(spec_messages));
  }
  if (!spec.complex_mode) {
    return MakeFailure(
        "srl3_endpoint_complex_mode_required",
        "SRL-3 endpoint branch ledger is branch-sensitive complex eta=0 evidence");
  }
  if (!plan.target_endpoint_singular.has_value()) {
    return MakeFailure(
        "srl3_endpoint_marker_missing",
        "SRL-3 endpoint branch ledger requires an explicit target_endpoint_singular marker");
  }
  if (plan.contour_points.size() < 2) {
    return MakeFailure(
        "srl3_endpoint_contour_missing",
        "SRL-3 endpoint branch ledger requires a contour with a start and endpoint");
  }

  const EtaContourSingularPoint& endpoint_singular = *plan.target_endpoint_singular;
  if (endpoint_singular.value != plan.contour_points.back()) {
    return MakeFailure(
        "srl3_endpoint_marker_mismatch",
        "SRL-3 endpoint branch ledger marker value must match the final contour point");
  }
  if (!IsZeroComplexEndpoint(endpoint_singular.value)) {
    return MakeFailure(
        "srl3_endpoint_not_eta_zero",
        "SRL-3 endpoint branch ledger is limited to the reviewed eta=0 singular endpoint");
  }
  if (!SameEndpoint(endpoint_singular, local_model) ||
      local_model.eta_symbol != plan.eta_symbol ||
      local_model.contour_fingerprint != plan.contour_fingerprint) {
    return MakeFailure(
        "srl3_endpoint_local_model_mismatch",
        "SRL-3 endpoint branch ledger requires the SRL-2 local model to match the marked "
        "endpoint contour identity and fingerprint");
  }
  if (!local_model.branch_sensitive) {
    return MakeFailure(
        "srl3_endpoint_local_model_not_branch_sensitive",
        "SRL-3 endpoint branch ledger requires a branch-sensitive SRL-2 local model");
  }

  const ExactComplexRational& approach_point =
      plan.contour_points[plan.contour_points.size() - 2];
  if (!SameCanonicalRational(approach_point.imaginary, ZeroRational()) ||
      CompareRational(approach_point.real, endpoint_singular.value.real) >= 0) {
    return MakeFailure(
        "srl3_endpoint_approach_unsupported",
        "SRL-3 endpoint branch ledger currently supports the reviewed eta=0 approach from "
        "the negative real side, with the selected half-plane supplying the infinitesimal "
        "imaginary prescription");
  }

  ResolvedPrescription resolved;
  const EtaEndpointBranchLedgerAnalysis prescription_failure =
      ResolveEndpointPrescription(spec, resolved);
  if (!prescription_failure.failure_code.empty()) {
    return prescription_failure;
  }

  const EtaContourHalfPlane required_half_plane =
      RequiredHalfPlaneForPrescription(resolved.prescription);
  if (plan.half_plane != required_half_plane) {
    return MakeFailure(
        "srl3_endpoint_prescription_half_plane_mismatch",
        "SRL-3 endpoint branch ledger requires " + ToString(resolved.prescription) +
            " prescription metadata to use the " + ToString(required_half_plane) +
            " half-plane, but the contour plan uses the " + ToString(plan.half_plane) +
            " half-plane");
  }

  EtaEndpointBranchLedger ledger;
  ledger.eta_symbol = plan.eta_symbol;
  ledger.endpoint_expression = endpoint_singular.expression;
  ledger.endpoint_value = endpoint_singular.value;
  ledger.half_plane = plan.half_plane;
  ledger.prescription = resolved.prescription;
  ledger.prescription_source = resolved.source;
  ledger.approach_direction = ApproachDirectionForHalfPlane(plan.half_plane);
  ledger.log_branch_argument = LogBranchArgumentForHalfPlane(plan.half_plane);
  ledger.log_sheet_index = LogSheetIndexForHalfPlane(plan.half_plane);
  ledger.endpoint_branch_winding = endpoint_singular.branch_winding;
  ledger.contour_fingerprint = plan.contour_fingerprint;
  ledger.local_model_kind = local_model.local_model_kind;
  ledger.extraction_order = local_model.extraction_order;
  ledger.live_endpoint_extraction_ready = false;
  ledger.ledger_fingerprint =
      ComputeArtifactFingerprint(SerializeEndpointBranchLedgerForFingerprint(ledger));

  EtaEndpointBranchLedgerAnalysis analysis;
  analysis.success = true;
  analysis.summary =
      "SRL-3 endpoint branch ledger constructed; live eta=0 endpoint extraction remains "
      "deferred";
  analysis.ledger = std::move(ledger);
  return analysis;
}

}  // namespace amflow
