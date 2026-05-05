#include "amflow/io/sample_data.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/endpoint_branch_ledger.hpp"
#include "amflow/runtime/continuation_path.hpp"
#include "amflow/runtime/endpoint_local_model.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void ExpectContains(const std::string& value,
                    const std::string& needle,
                    const std::string& message) {
  if (value.find(needle) == std::string::npos) {
    throw std::runtime_error(message + "; missing substring: " + needle + "; value: " + value);
  }
}

template <typename Callable>
void ExpectInvalidArgumentContains(Callable&& callable,
                                   const std::string& needle,
                                   const std::string& message) {
  try {
    callable();
  } catch (const std::invalid_argument& error) {
    ExpectContains(error.what(), needle, message);
    return;
  }
  throw std::runtime_error(message + "; expected std::invalid_argument");
}

void ExpectAnalysisFailure(const amflow::EtaEndpointLocalModelAnalysis& analysis,
                           const std::string& failure_code,
                           const std::string& summary_needle,
                           const std::string& message) {
  Expect(!analysis.success, message + "; analysis should fail closed");
  Expect(analysis.failure_code == failure_code,
         message + "; expected failure_code=" + failure_code + ", got " +
             analysis.failure_code);
  ExpectContains(analysis.summary, summary_needle, message);
  Expect(!analysis.model.has_value(), message + "; failed analysis should not carry a model");
}

void ExpectAnalysisFailure(const amflow::EtaEndpointBranchLedgerAnalysis& analysis,
                           const std::string& failure_code,
                           const std::string& summary_needle,
                           const std::string& message) {
  Expect(!analysis.success, message + "; analysis should fail closed");
  Expect(analysis.failure_code == failure_code,
         message + "; expected failure_code=" + failure_code + ", got " +
             analysis.failure_code);
  ExpectContains(analysis.summary, summary_needle, message);
  Expect(!analysis.ledger.has_value(), message + "; failed analysis should not carry a ledger");
}

amflow::ProblemSpec MakeComplexEndpointProblemSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.complex_mode = true;
  spec.kinematics.complex_numeric_substitutions["unused_complex_anchor"] = "1+I";
  return spec;
}

amflow::ProblemSpec MakeComplexEndpointProblemSpecWithRawPrescription(
    const amflow::FeynmanPrescription prescription) {
  amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  for (amflow::Propagator& propagator : spec.family.propagators) {
    propagator.prescription = static_cast<int>(prescription);
  }
  return spec;
}

amflow::DESystem MakeScalarEndpointDESystem(const std::string& coefficient) {
  amflow::DESystem system;
  system.masters = {
      {"srl2_endpoint", {1}, "I1"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices["eta"] = {
      {coefficient},
  };
  system.singular_points = {"eta=0"};
  return system;
}

amflow::DESystem MakeMatrixEndpointDESystem(
    const std::vector<std::vector<std::string>>& coefficient_matrix) {
  amflow::DESystem system;
  system.masters = {
      {"srl2_endpoint", {1, 0}, "I1"},
      {"srl2_endpoint", {0, 1}, "I2"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices["eta"] = coefficient_matrix;
  system.singular_points = {"eta=0"};
  return system;
}

amflow::EtaContinuationPlan MakeMarkedEtaZeroEndpointPlan(
    const amflow::DESystem& system,
    const amflow::ProblemSpec& spec,
    const amflow::EtaContourHalfPlane half_plane = amflow::EtaContourHalfPlane::Upper) {
  return amflow::PlanEtaContinuationContourWithTargetEndpointSingular(
      system,
      spec,
      "eta",
      "eta=-1",
      "eta=0",
      "eta=0",
      half_plane);
}

void UnmarkedSingularTargetEndpointStillRejectsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::DESystem system = amflow::MakeSampleDESystem();

  ExpectInvalidArgumentContains(
      [&]() {
        static_cast<void>(amflow::PlanEtaContinuationContour(
            system, spec, "eta", "eta=-1", "eta=0", amflow::EtaContourHalfPlane::Upper));
      },
      "lands on evaluated singular point eta=0",
      "unmarked singular eta endpoint should keep failing closed");
}

void MarkedSingularTargetEndpointRecordsEndpointContractTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::DESystem system = amflow::MakeSampleDESystem();

  const amflow::EtaContinuationPlan plan =
      amflow::PlanEtaContinuationContourWithTargetEndpointSingular(
          system,
          spec,
          "eta",
          "eta=-1",
          "eta=0",
          "eta=0",
          amflow::EtaContourHalfPlane::Upper);
  const amflow::EtaContinuationPlan repeat_plan =
      amflow::PlanEtaContinuationContourWithTargetEndpointSingular(
          system,
          spec,
          "eta",
          "eta=-1",
          "eta=0",
          "eta=0",
          amflow::EtaContourHalfPlane::Upper);

  Expect(plan.contour_points.size() == 2,
         "marked endpoint plan should not insert an ordinary waypoint at the target");
  Expect(plan.target_endpoint_singular.has_value(),
         "marked endpoint plan should record endpoint-singular metadata");
  Expect(plan.target_endpoint_singular->expression == "eta=0",
         "marked endpoint plan should preserve the canonical singular expression");
  Expect(plan.target_endpoint_singular->value == plan.contour_points.back(),
         "marked endpoint plan should bind endpoint-singular value to the target point");
  Expect(plan.contour_fingerprint == repeat_plan.contour_fingerprint,
         "marked endpoint plan fingerprint should be deterministic");

  const amflow::EtaContinuationPlanManifest manifest =
      amflow::MakeEtaContinuationPlanManifest(plan, "lane51-srl1-endpoint");
  const std::string yaml = amflow::SerializeEtaContinuationPlanManifestYaml(manifest);
  ExpectContains(yaml,
                 "target_endpoint_singular:",
                 "endpoint-aware manifest should persist endpoint-singular metadata");
  ExpectContains(yaml,
                 "expression: \"eta=0\"",
                 "endpoint-aware manifest should persist endpoint-singular expression");
  ExpectContains(yaml,
                 "value: \"0\"",
                 "endpoint-aware manifest should persist endpoint-singular value");
  ExpectContains(yaml,
                 "contour_fingerprint: \"" + plan.contour_fingerprint + "\"",
                 "endpoint-aware manifest should persist the endpoint-aware fingerprint");
}

void MarkedSingularTargetEndpointRejectsMismatchedMarkerTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::DESystem system = amflow::MakeSampleDESystem();

  ExpectInvalidArgumentContains(
      [&]() {
        static_cast<void>(amflow::PlanEtaContinuationContourWithTargetEndpointSingular(
            system,
            spec,
            "eta",
            "eta=-1",
            "eta=0",
            "eta=s",
            amflow::EtaContourHalfPlane::Upper));
      },
      "instead of target endpoint 0",
      "endpoint-aware planner should reject a marker that does not evaluate to the target");
}

void MarkedSingularTargetEndpointStillRejectsInteriorCrossingTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::DESystem system = amflow::MakeSampleDESystem();
  system.singular_points = {"eta=0", "eta=1"};

  ExpectInvalidArgumentContains(
      [&]() {
        static_cast<void>(amflow::FinalizeEtaContinuationContourWithTargetEndpointSingular(
            system,
            spec,
            "eta",
            std::vector<std::string>{"eta=-1", "eta=1"},
            "eta=1",
            amflow::EtaContourHalfPlane::Upper));
      },
      "crosses evaluated singular point eta=0",
      "endpoint marker should not turn interior singular crossings into accepted waypoints");
}

void EndpointLocalModelRecordsIntegerNonresonantSidecarTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);

  const amflow::EtaEndpointLocalModelAnalysis analysis =
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan, 3);
  Expect(analysis.success, "SRL-2 endpoint local model should accept reviewed scalar 1/eta");
  Expect(analysis.model.has_value(), "successful SRL-2 analysis should carry a local model");

  const amflow::EtaEndpointLocalModel& model = *analysis.model;
  Expect(model.eta_symbol == "eta", "local model should preserve eta symbol");
  Expect(model.endpoint_expression == "eta=0",
         "local model should preserve endpoint singular identity");
  Expect(model.endpoint_value.ToString() == "0", "local model should bind eta=0 endpoint");
  Expect(model.indicial_exponents.size() == 1 && model.indicial_exponents.front() == "1",
         "local model should record integer Frobenius exponent");
  Expect(model.residue_matrix.size() == 1 && model.residue_matrix.front().size() == 1 &&
             model.residue_matrix.front().front().ToString() == "1",
         "local model should record the simple-pole residue matrix");
  Expect(model.branch_sensitive,
         "SRL-2 local model should mark branch-sensitive endpoint analysis");
  Expect(!model.live_endpoint_extraction_ready,
         "SRL-2 must not claim live eta=0 endpoint extraction");

  const amflow::EtaEndpointLocalModelManifest manifest =
      amflow::MakeEtaEndpointLocalModelManifest(model, "lane56-srl2-local-model");
  const std::string yaml = amflow::SerializeEtaEndpointLocalModelManifestYaml(manifest);
  ExpectContains(yaml,
                 "manifest_kind: \"eta-endpoint-local-model\"",
                 "local-model sidecar should declare its manifest kind");
  ExpectContains(yaml,
                 "endpoint_expression: \"eta=0\"",
                 "local-model sidecar should persist endpoint identity");
  ExpectContains(yaml,
                 "local_model_kind: "
                 "\"srl2-upper-triangular-frobenius-simple-pole-integer-nonresonant\"",
                 "local-model sidecar should persist the reviewed subset name");
  ExpectContains(yaml,
                 "live_endpoint_extraction_ready: false",
                 "local-model sidecar should keep live extraction deferred");
  ExpectContains(yaml,
                 "- [\"1\"]",
                 "local-model sidecar should persist the residue matrix");
}

void EndpointLocalModelRequiresMarkedEndpointTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);
  plan.target_endpoint_singular.reset();

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan),
      "srl2_endpoint_marker_missing",
      "requires an explicit target_endpoint_singular",
      "SRL-2 local model should reject unmarked endpoint-singular contours");
}

void EndpointLocalModelRejectsDirectRealFrobeniusReuseTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan),
      "srl2_endpoint_complex_mode_required",
      "direct-real Frobenius support is not accepted",
      "SRL-2 local model should not reuse direct-real Frobenius support as complex evidence");
}

void EndpointLocalModelRejectsRegularEndpointTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  const amflow::DESystem system = MakeScalarEndpointDESystem("0");
  const amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan),
      "srl2_endpoint_regular_model_unsupported",
      "requires a singular center",
      "SRL-2 local model should reject declared endpoints that are regular in the DE matrix");
}

void EndpointLocalModelRejectsFractionalExponentTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/(2*eta)");
  const amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan),
      "srl2_endpoint_fractional_exponent_unsupported",
      "fractional Frobenius exponent",
      "SRL-2 local model should fail closed on fractional endpoint exponents");
}

void EndpointLocalModelRejectsResonantIntegerExponentsTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  const amflow::DESystem system =
      MakeMatrixEndpointDESystem({{"1/eta", "0"}, {"0", "0"}});
  const amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan),
      "srl2_endpoint_resonance_unsupported",
      "resonant Frobenius separation",
      "SRL-2 local model should fail closed on resonant integer exponent separations");
}

void EndpointLocalModelRejectsLogarithmicResonanceTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpec();
  const amflow::DESystem system =
      MakeMatrixEndpointDESystem({{"1/eta", "1"}, {"0", "0"}});
  const amflow::EtaContinuationPlan plan = MakeMarkedEtaZeroEndpointPlan(system, spec);

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan),
      "srl2_endpoint_logarithmic_model_unsupported",
      "logarithmic",
      "SRL-2 local model should fail closed on logarithmic Frobenius cases");
}

void EndpointBranchLedgerRecordsLowerPrescriptionSidecarTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::MinusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::EtaContinuationPlan plan =
      MakeMarkedEtaZeroEndpointPlan(system, spec, amflow::EtaContourHalfPlane::Lower);
  const amflow::EtaEndpointLocalModelAnalysis local_analysis =
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan, 3);
  Expect(local_analysis.success,
         "SRL-3 branch ledger test requires a successful SRL-2 local model");

  const amflow::EtaEndpointBranchLedgerAnalysis analysis =
      amflow::AnalyzeEtaEndpointBranchLedger(spec, plan, *local_analysis.model);
  Expect(analysis.success, "SRL-3 endpoint branch ledger should accept reviewed -i0 lower path");
  Expect(analysis.ledger.has_value(), "successful SRL-3 analysis should carry a branch ledger");

  const amflow::EtaEndpointBranchLedger& ledger = *analysis.ledger;
  Expect(ledger.eta_symbol == "eta", "branch ledger should preserve eta symbol");
  Expect(ledger.endpoint_expression == "eta=0",
         "branch ledger should preserve endpoint singular identity");
  Expect(ledger.endpoint_value.ToString() == "0", "branch ledger should bind eta=0 endpoint");
  Expect(ledger.half_plane == amflow::EtaContourHalfPlane::Lower,
         "branch ledger should preserve lower half-plane selection");
  Expect(ledger.prescription == amflow::FeynmanPrescription::MinusI0,
         "branch ledger should preserve resolved -i0 prescription");
  Expect(ledger.prescription_source == "family.propagators[].prescription",
         "branch ledger should record the prescription metadata source");
  Expect(ledger.approach_direction == "NegIm",
         "lower half-plane ledger should fix the negative-imaginary endpoint approach");
  Expect(ledger.log_branch_argument == "-pi",
         "lower half-plane ledger should fix the negative log branch");
  Expect(ledger.log_sheet_index == -1,
         "lower half-plane ledger should record the reviewed log sheet index");
  Expect(ledger.contour_fingerprint == plan.contour_fingerprint,
         "branch ledger should inherit contour fingerprint identity");
  Expect(ledger.local_model_kind == local_analysis.model->local_model_kind,
         "branch ledger should inherit SRL-2 local-model identity");
  Expect(ledger.extraction_order == 3,
         "branch ledger should inherit SRL-2 extraction order");
  Expect(!ledger.live_endpoint_extraction_ready,
         "SRL-3 must not claim live eta=0 endpoint extraction");
  Expect(!ledger.ledger_fingerprint.empty(),
         "branch ledger should carry a deterministic fingerprint");

  const amflow::EtaEndpointBranchLedgerManifest manifest =
      amflow::MakeEtaEndpointBranchLedgerManifest(ledger, "lane64-srl3-branch-ledger");
  const std::string yaml = amflow::SerializeEtaEndpointBranchLedgerManifestYaml(manifest);
  ExpectContains(yaml,
                 "manifest_kind: \"eta-endpoint-branch-ledger\"",
                 "branch-ledger sidecar should declare its manifest kind");
  ExpectContains(yaml,
                 "half_plane: \"lower\"",
                 "branch-ledger sidecar should persist half-plane selection");
  ExpectContains(yaml,
                 "prescription: \"-i0\"",
                 "branch-ledger sidecar should persist prescription polarity");
  ExpectContains(yaml,
                 "approach_direction: \"NegIm\"",
                 "branch-ledger sidecar should persist endpoint approach direction");
  ExpectContains(yaml,
                 "log_branch_argument: \"-pi\"",
                 "branch-ledger sidecar should persist log branch argument");
  ExpectContains(yaml,
                 "live_endpoint_extraction_ready: false",
                 "branch-ledger sidecar should keep live extraction deferred");
  ExpectContains(yaml,
                 "ledger_fingerprint: \"" + ledger.ledger_fingerprint + "\"",
                 "branch-ledger sidecar should persist the ledger fingerprint");
}

void EndpointBranchLedgerDistinguishesPlusAndMinusPrescriptionTest() {
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");

  const amflow::ProblemSpec plus_spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::EtaContinuationPlan plus_plan =
      MakeMarkedEtaZeroEndpointPlan(system, plus_spec, amflow::EtaContourHalfPlane::Upper);
  const amflow::EtaEndpointLocalModelAnalysis plus_local =
      amflow::AnalyzeEtaEndpointLocalModel(system, plus_spec, plus_plan, 3);
  Expect(plus_local.success, "plus-prescription branch ledger needs SRL-2 local model");
  const amflow::EtaEndpointBranchLedgerAnalysis plus_analysis =
      amflow::AnalyzeEtaEndpointBranchLedger(plus_spec, plus_plan, *plus_local.model);
  Expect(plus_analysis.success, "SRL-3 should accept +i0 upper-half-plane metadata");

  const amflow::ProblemSpec minus_spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::MinusI0);
  const amflow::EtaContinuationPlan minus_plan =
      MakeMarkedEtaZeroEndpointPlan(system, minus_spec, amflow::EtaContourHalfPlane::Lower);
  const amflow::EtaEndpointLocalModelAnalysis minus_local =
      amflow::AnalyzeEtaEndpointLocalModel(system, minus_spec, minus_plan, 3);
  Expect(minus_local.success, "minus-prescription branch ledger needs SRL-2 local model");
  const amflow::EtaEndpointBranchLedgerAnalysis minus_analysis =
      amflow::AnalyzeEtaEndpointBranchLedger(minus_spec, minus_plan, *minus_local.model);
  Expect(minus_analysis.success, "SRL-3 should accept -i0 lower-half-plane metadata");

  Expect(plus_analysis.ledger->approach_direction == "PosIm",
         "+i0 ledger should select positive-imaginary endpoint approach");
  Expect(plus_analysis.ledger->log_branch_argument == "+pi",
         "+i0 ledger should select positive log branch");
  Expect(plus_analysis.ledger->log_sheet_index == 0,
         "+i0 ledger should select the principal upper sheet");
  Expect(minus_analysis.ledger->approach_direction == "NegIm",
         "-i0 ledger should select negative-imaginary endpoint approach");
  Expect(minus_analysis.ledger->log_branch_argument == "-pi",
         "-i0 ledger should select negative log branch");
  Expect(minus_analysis.ledger->log_sheet_index == -1,
         "-i0 ledger should select the lower sheet");
  Expect(plus_analysis.ledger->ledger_fingerprint != minus_analysis.ledger->ledger_fingerprint,
         "opposite endpoint branch ledgers should not share a fingerprint");
}

void EndpointBranchLedgerRejectsMissingPrescriptionTest() {
  amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::None);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::EtaContinuationPlan plan =
      MakeMarkedEtaZeroEndpointPlan(system, spec, amflow::EtaContourHalfPlane::Upper);
  const amflow::EtaEndpointLocalModelAnalysis local_analysis =
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan, 3);
  Expect(local_analysis.success, "missing-prescription failure needs SRL-2 local model");

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointBranchLedger(spec, plan, *local_analysis.model),
      "srl3_endpoint_prescription_missing",
      "requires a nonzero Feynman prescription",
      "SRL-3 branch ledger should fail closed when prescription metadata is missing");
}

void EndpointBranchLedgerRejectsContradictoryPrescriptionTest() {
  amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::MinusI0);
  spec.family.propagators.front().prescription =
      static_cast<int>(amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::EtaContinuationPlan plan =
      MakeMarkedEtaZeroEndpointPlan(system, spec, amflow::EtaContourHalfPlane::Lower);
  const amflow::EtaEndpointLocalModelAnalysis local_analysis =
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan, 3);
  Expect(local_analysis.success, "contradictory-prescription failure needs SRL-2 local model");

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointBranchLedger(spec, plan, *local_analysis.model),
      "srl3_endpoint_prescription_contradiction",
      "contradictory",
      "SRL-3 branch ledger should fail closed on contradictory prescription metadata");
}

void EndpointBranchLedgerRejectsPrescriptionHalfPlaneMismatchTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::MinusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::EtaContinuationPlan plan =
      MakeMarkedEtaZeroEndpointPlan(system, spec, amflow::EtaContourHalfPlane::Upper);
  const amflow::EtaEndpointLocalModelAnalysis local_analysis =
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan, 3);
  Expect(local_analysis.success, "half-plane mismatch failure needs SRL-2 local model");

  ExpectAnalysisFailure(
      amflow::AnalyzeEtaEndpointBranchLedger(spec, plan, *local_analysis.model),
      "srl3_endpoint_prescription_half_plane_mismatch",
      "requires -i0 prescription metadata to use the lower half-plane",
      "SRL-3 branch ledger should fail closed when prescription and half-plane conflict");
}

}  // namespace

int main() {
  try {
    UnmarkedSingularTargetEndpointStillRejectsTest();
    MarkedSingularTargetEndpointRecordsEndpointContractTest();
    MarkedSingularTargetEndpointRejectsMismatchedMarkerTest();
    MarkedSingularTargetEndpointStillRejectsInteriorCrossingTest();
    EndpointLocalModelRecordsIntegerNonresonantSidecarTest();
    EndpointLocalModelRequiresMarkedEndpointTest();
    EndpointLocalModelRejectsDirectRealFrobeniusReuseTest();
    EndpointLocalModelRejectsRegularEndpointTest();
    EndpointLocalModelRejectsFractionalExponentTest();
    EndpointLocalModelRejectsResonantIntegerExponentsTest();
    EndpointLocalModelRejectsLogarithmicResonanceTest();
    EndpointBranchLedgerRecordsLowerPrescriptionSidecarTest();
    EndpointBranchLedgerDistinguishesPlusAndMinusPrescriptionTest();
    EndpointBranchLedgerRejectsMissingPrescriptionTest();
    EndpointBranchLedgerRejectsContradictoryPrescriptionTest();
    EndpointBranchLedgerRejectsPrescriptionHalfPlaneMismatchTest();
  } catch (const std::exception& error) {
    std::cerr << "singular-runtime-lane-tests failed: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
