#include "amflow/io/sample_data.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/endpoint_branch_ledger.hpp"
#include "amflow/runtime/continuation_path.hpp"
#include "amflow/runtime/cutkosky_transport.hpp"
#include "amflow/runtime/endpoint_local_model.hpp"
#include "amflow/solver/series_solver.hpp"

#include <exception>
#include <fstream>
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

void ExpectNotContains(const std::string& value,
                       const std::string& needle,
                       const std::string& message) {
  if (value.find(needle) != std::string::npos) {
    throw std::runtime_error(message + "; unexpected substring: " + needle);
  }
}

std::string ReadRepoFile(const std::string& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("failed to open fixture file: " + path);
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
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

template <typename Callable>
void ExpectRuntimeErrorContains(Callable&& callable,
                                const std::string& needle,
                                const std::string& message) {
  try {
    callable();
  } catch (const std::runtime_error& error) {
    ExpectContains(error.what(), needle, message);
    return;
  }
  throw std::runtime_error(message + "; expected std::runtime_error");
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

void ExpectEndpointExtractionFailure(const amflow::SolverDiagnostics& diagnostics,
                                     const std::string& failure_code,
                                     const std::string& summary_needle,
                                     const std::string& message) {
  Expect(!diagnostics.success, message + "; extraction should fail closed");
  Expect(diagnostics.failure_code == failure_code,
         message + "; expected failure_code=" + failure_code + ", got " +
             diagnostics.failure_code);
  ExpectContains(diagnostics.summary, summary_needle, message);
  Expect(!diagnostics.full_eta_zero_contour_applied,
         message + "; failed extraction must not set the full eta=0 contour flag");
  Expect(diagnostics.target_values.empty(),
         message + "; failed extraction must not publish endpoint coefficients");
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

amflow::DESystem MakeSrl5SingularEndpointDESystem() {
  amflow::DESystem system;
  system.masters = {
      {"srl5_singular_endpoint", {1}, "I1"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices["eta"] = {
      {"1/eta"},
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

amflow::SolveRequest MakeReviewedEtaZeroEndpointSolveRequest(
    const amflow::DESystem& system,
    const amflow::EtaContinuationPlan& plan,
    const amflow::EtaEndpointLocalModel& local_model,
    const amflow::EtaEndpointBranchLedger& branch_ledger,
    const std::vector<std::string>& start_values) {
  amflow::SolveRequest request;
  request.system = system;
  request.start_location = "eta=-1";
  request.target_location = "eta=0";
  request.boundary_requests = {{"eta", request.start_location, "manual"}};
  request.boundary_conditions = {{"eta", request.start_location, start_values, "manual"}};
  request.eta_continuation_plan = plan;
  request.eta_endpoint_local_model = local_model;
  request.eta_endpoint_branch_ledger = branch_ledger;
  request.requested_digits = 50;
  return request;
}

amflow::SolveRequest MakeReviewedEtaZeroEndpointSolveRequest(
    const amflow::DESystem& system,
    const amflow::ProblemSpec& spec,
    const std::vector<std::string>& start_values,
    const int extraction_order = 3) {
  const amflow::EtaContinuationPlan plan =
      MakeMarkedEtaZeroEndpointPlan(system, spec, amflow::EtaContourHalfPlane::Upper);
  const amflow::EtaEndpointLocalModelAnalysis local_analysis =
      amflow::AnalyzeEtaEndpointLocalModel(system, spec, plan, extraction_order);
  Expect(local_analysis.success && local_analysis.model.has_value(),
         "SRL-4 test fixture requires a successful SRL-2 local model");
  const amflow::EtaEndpointBranchLedgerAnalysis ledger_analysis =
      amflow::AnalyzeEtaEndpointBranchLedger(spec, plan, *local_analysis.model);
  Expect(ledger_analysis.success && ledger_analysis.ledger.has_value(),
         "SRL-4 test fixture requires a successful SRL-3 branch ledger");
  return MakeReviewedEtaZeroEndpointSolveRequest(system,
                                                 plan,
                                                 *local_analysis.model,
                                                 *ledger_analysis.ledger,
                                                 start_values);
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

void EndpointExtractionScalarSimplePoleProducesCoefficientTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  const amflow::SolveRequest request =
      MakeReviewedEtaZeroEndpointSolveRequest(system, spec, {"7"});

  const amflow::SolverDiagnostics diagnostics =
      amflow::BootstrapSeriesSolver().Solve(request);

  Expect(diagnostics.success,
         "SRL-4 scalar endpoint extraction should execute the reviewed eta=0 path");
  Expect(diagnostics.full_eta_zero_contour_applied,
         "SRL-4 scalar endpoint extraction should set the full eta=0 contour flag");
  Expect(diagnostics.failure_code.empty(),
         "SRL-4 scalar endpoint extraction should not report a failure code");
  Expect(diagnostics.target_values.size() == 1 && diagnostics.target_values.front() == "-7",
         "SRL-4 scalar endpoint extraction should publish the Frobenius endpoint coefficient");
  Expect(!diagnostics.eta_endpoint_extraction_fingerprint.empty(),
         "SRL-4 scalar endpoint extraction should carry an extraction fingerprint");
  Expect(diagnostics.eta_endpoint_contour_fingerprint ==
             request.eta_continuation_plan->contour_fingerprint,
         "SRL-4 scalar endpoint extraction should report the consumed contour fingerprint");
  Expect(diagnostics.eta_endpoint_branch_ledger_fingerprint ==
             request.eta_endpoint_branch_ledger->ledger_fingerprint,
         "SRL-4 scalar endpoint extraction should report the consumed branch-ledger fingerprint");
  ExpectContains(diagnostics.summary,
                 "full_eta_zero_contour_applied=true",
                 "SRL-4 scalar endpoint extraction should publish an explicit runtime audit");
}

void EndpointExtractionTwoMasterDiagonalPreservesMasterOrderTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system =
      MakeMatrixEndpointDESystem({{"1/eta", "0"}, {"0", "1/eta"}});
  const amflow::SolveRequest request =
      MakeReviewedEtaZeroEndpointSolveRequest(system, spec, {"3", "-4"});

  const amflow::SolverDiagnostics diagnostics =
      amflow::BootstrapSeriesSolver().Solve(request);

  Expect(diagnostics.success,
         "SRL-4 two-master endpoint extraction should execute the reviewed eta=0 path");
  Expect(diagnostics.full_eta_zero_contour_applied,
         "SRL-4 two-master endpoint extraction should set the full eta=0 contour flag");
  Expect(diagnostics.target_values.size() == 2 &&
             diagnostics.target_values[0] == "-3" && diagnostics.target_values[1] == "4",
         "SRL-4 two-master endpoint extraction should preserve master order");
}

void EndpointExtractionRejectsLocalModelResidueMismatchTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  amflow::SolveRequest request =
      MakeReviewedEtaZeroEndpointSolveRequest(system, spec, {"7"});
  request.eta_endpoint_local_model->residue_matrix.front().front() = {"2", "1"};

  const amflow::SolverDiagnostics diagnostics =
      amflow::BootstrapSeriesSolver().Solve(request);

  ExpectEndpointExtractionFailure(
      diagnostics,
      "srl4_endpoint_local_model_mismatch",
      "residue matrix",
      "SRL-4 endpoint extraction should consume and verify the SRL-2 local model");
}

void EndpointExtractionRejectsBranchLedgerFingerprintMismatchTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  amflow::SolveRequest request =
      MakeReviewedEtaZeroEndpointSolveRequest(system, spec, {"7"});
  request.eta_endpoint_branch_ledger->ledger_fingerprint = "stale-srl3-ledger";

  const amflow::SolverDiagnostics diagnostics =
      amflow::BootstrapSeriesSolver().Solve(request);

  ExpectEndpointExtractionFailure(
      diagnostics,
      "srl4_endpoint_branch_ledger_fingerprint_mismatch",
      "branch-ledger fingerprint",
      "SRL-4 endpoint extraction should reject stale SRL-3 branch ledgers");
}

void EndpointExtractionRejectsStaleContourFingerprintTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system = MakeScalarEndpointDESystem("1/eta");
  amflow::SolveRequest request =
      MakeReviewedEtaZeroEndpointSolveRequest(system, spec, {"7"});
  request.eta_continuation_plan->contour_fingerprint = "stale-srl1-contour";

  const amflow::SolverDiagnostics diagnostics =
      amflow::BootstrapSeriesSolver().Solve(request);

  ExpectEndpointExtractionFailure(
      diagnostics,
      "srl4_endpoint_contour_fingerprint_mismatch",
      "contour fingerprint",
      "SRL-4 endpoint extraction should reject stale SRL-1 endpoint contours");
}

void Srl5CaseStudyEvidenceMatchesLiveEndpointExtractionTest() {
  const amflow::ProblemSpec spec = MakeComplexEndpointProblemSpecWithRawPrescription(
      amflow::FeynmanPrescription::PlusI0);
  const amflow::DESystem system = MakeSrl5SingularEndpointDESystem();
  const amflow::SolveRequest request =
      MakeReviewedEtaZeroEndpointSolveRequest(system, spec, {"7"});

  const amflow::SolverDiagnostics diagnostics =
      amflow::BootstrapSeriesSolver().Solve(request);

  Expect(diagnostics.success,
         "SRL-5 evidence fixture should be reproducible by the live SRL-4 endpoint path");
  Expect(diagnostics.full_eta_zero_contour_applied,
         "SRL-5 evidence fixture must exercise full eta=0 endpoint extraction");
  Expect(diagnostics.target_values.size() == 1 && diagnostics.target_values.front() == "-7",
         "SRL-5 evidence fixture should reproduce the retained endpoint coefficient");

  const std::string evidence = ReadRepoFile(
      "tools/reference-harness/specs/case-studies/"
      "one-singular-endpoint-case.numeric-evidence.json");
  const std::string cpp_result = ReadRepoFile(
      "tools/reference-harness/specs/case-studies/"
      "one-singular-endpoint-case.srl5.digits80.cpp-result.json");
  const std::string comparison = ReadRepoFile(
      "tools/reference-harness/specs/case-studies/"
      "one-singular-endpoint-case.srl5.digits80.compare.json");
  const std::string golden = ReadRepoFile(
      "tools/reference-harness/specs/case-studies/"
      "one-singular-endpoint-case.srl5-golden.txt");

  ExpectContains(evidence,
                 "\"case_study_id\": \"one-singular-endpoint-case\"",
                 "SRL-5 sidecar should target the singular case-study family");
  ExpectContains(evidence,
                 "\"evidence_kind\": "
                 "\"srl4-live-endpoint-extraction-vs-accepted-exact-golden\"",
                 "SRL-5 sidecar should name the live SRL-4 runtime evidence kind");
  ExpectContains(evidence,
                 "\"minimum_observed_correct_digits\": 999",
                 "SRL-5 sidecar should preserve the exact comparison digit floor");
  ExpectContains(evidence,
                 "\"full_eta_zero_contour_applied\": true",
                 "SRL-5 sidecar should preserve the endpoint extraction audit flag");
  ExpectContains(evidence,
                 "\"endpoint_coefficients\": [\n      \"-7\"",
                 "SRL-5 sidecar should publish the live endpoint coefficient");
  ExpectNotContains(evidence,
                    "b62p",
                    "SRL-5 sidecar should not keep the retired singular runtime blocker");

  ExpectContains(cpp_result,
                 "\"full_eta_zero_contour_applied\": true",
                 "SRL-5 C++ result should set the full eta=0 contour flag");
  ExpectContains(cpp_result,
                 "\"runtime_application\": \"full-eta-zero-contour-endpoint-extraction\"",
                 "SRL-5 C++ result should identify the endpoint runtime");
  ExpectContains(cpp_result,
                 "\"integral\": \"srl5_singular_endpoint[1]\"",
                 "SRL-5 C++ result should expose the reviewed fixture integral");
  ExpectContains(cpp_result,
                 "\"exact_real\": \"-7\"",
                 "SRL-5 C++ result should retain the live endpoint coefficient exactly");

  ExpectContains(comparison,
                 "\"passed\": true",
                 "SRL-5 comparison summary should pass");
  ExpectContains(comparison,
                 "\"compared_coefficient_count\": 1",
                 "SRL-5 comparison summary should be coefficient-bearing");
  ExpectContains(comparison,
                 "\"minimum_digit_agreement\": 999",
                 "SRL-5 comparison summary should record exact agreement");
  ExpectContains(golden,
                 "j[srl5_singular_endpoint,1] -> -7",
                 "SRL-5 accepted golden should match the reviewed endpoint coefficient");
}

amflow::ProblemSpec MakeB63nAutomaticPhaseSpaceSpec() {
  amflow::ProblemSpec spec;
  spec.family.name = "phase";
  spec.family.loop_momenta = {"l1", "l2"};
  spec.family.loop_prescriptions = {amflow::FeynmanPrescription::None,
                                    amflow::FeynmanPrescription::None};
  spec.family.propagators = {
      amflow::Propagator("l1^2-msq"),
      amflow::Propagator("(l1+p1)^2"),
      amflow::Propagator("l2^2"),
      amflow::Propagator("(l1+l2+p1)^2"),
      amflow::Propagator("(l1+l2+p1+p2)^2"),
      amflow::Propagator("(l1+l2+p2)^2"),
      amflow::Propagator("(l1+p2)^2"),
  };
  for (std::size_t index : {std::size_t{0}, std::size_t{2}, std::size_t{4}}) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Cut;
    spec.family.propagators[index].prescription =
        static_cast<int>(amflow::FeynmanPrescription::None);
  }
  spec.kinematics.invariants = {"s", "msq"};
  spec.kinematics.numeric_substitutions = {{"s", "100"}, {"msq", "1"}};
  spec.targets = {amflow::TargetIntegral{"phase", {1, 2, 1, 1, 1, 1, 1}}};
  return spec;
}

amflow::ProblemSpec MakeB63nFeynmanPrescriptionSpec(
    const amflow::FeynmanPrescription l1_prescription,
    const amflow::FeynmanPrescription l2_prescription) {
  amflow::ProblemSpec spec;
  spec.family.name = "loopxloop";
  spec.family.loop_momenta = {"l1", "l2", "q"};
  spec.family.loop_prescriptions = {l1_prescription,
                                    l2_prescription,
                                    amflow::FeynmanPrescription::None};
  spec.family.propagators = {
      amflow::Propagator("l1^2"),
      amflow::Propagator("(l1+p1)^2"),
      amflow::Propagator("(l1+p1+p2)^2"),
      amflow::Propagator("(l1+q)^2"),
      amflow::Propagator("l2^2"),
      amflow::Propagator("(l2-p2)^2"),
      amflow::Propagator("(l2-p2-p1)^2"),
      amflow::Propagator("(l2-q)^2"),
      amflow::Propagator("q^2-msq"),
      amflow::Propagator("(p1+p2-q)^2-m2sq"),
      amflow::Propagator("(l1-l2)^2"),
      amflow::Propagator("(p1-q)^2"),
  };
  for (std::size_t index : {std::size_t{8}, std::size_t{9}}) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Cut;
    spec.family.propagators[index].prescription =
        static_cast<int>(amflow::FeynmanPrescription::None);
  }
  spec.kinematics.invariants = {"s", "msq", "m2sq"};
  spec.kinematics.numeric_substitutions = {
      {"s", "10"},
      {"msq", "1"},
      {"m2sq", "2/5"},
  };
  spec.targets = {
      amflow::TargetIntegral{"loopxloop", {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0}}};
  return spec;
}

void B63nAutomaticPhaseSpaceCutkoskyTransportScaffoldAuditsEndpointContractTest() {
  const amflow::CutkoskyEtaZeroTransportAudit audit =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(MakeB63nAutomaticPhaseSpaceSpec());

  Expect(audit.reviewed_surface,
         "b63n scaffold should recognize the exact automatic_phasespace topology");
  Expect(!audit.live_coefficients_available,
         "b63n scaffold must not claim live residue coefficients");
  Expect(!audit.retained_solution_samples_used,
         "b63n scaffold must not consume retained AMFlow solution samples");
  Expect(audit.cut_propagator_indices ==
             std::vector<std::size_t>({0, 2, 4}),
         "b63n automatic_phasespace scaffold should preserve the D1,D3,D5 cut support");
  Expect(audit.phase_volume_loop_count == 2,
         "b63n automatic_phasespace scaffold should identify the two-loop phase volume");
  Expect(audit.provider_strategy == "builtin::cutkosky-phase-space::none",
         "b63n automatic_phasespace scaffold should keep the no-prescription cut provider");
  Expect(audit.eta_contour_direction == "NegIm",
         "b63n automatic_phasespace scaffold should map no prescription to NegIm");
  ExpectContains(audit.cutkosky_prefactor,
                 "K_2(eps)",
                 "b63n automatic_phasespace scaffold should audit the reviewed K_2 prefactor");
  ExpectContains(audit.coefficient_gap,
                 "Live Cutkosky residue coefficients are not yet implemented",
                 "b63n scaffold should explicitly report the remaining coefficient gap");
}

void B63nFeynmanPrescriptionCutkoskyTransportScaffoldRecordsConjugateLedgersTest() {
  const amflow::CutkoskyEtaZeroTransportAudit plus_minus =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(
          MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::PlusI0,
                                          amflow::FeynmanPrescription::MinusI0));
  const amflow::CutkoskyEtaZeroTransportAudit minus_plus =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(
          MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::MinusI0,
                                          amflow::FeynmanPrescription::PlusI0));

  Expect(plus_minus.reviewed_surface && minus_plus.reviewed_surface,
         "b63n scaffold should recognize both feynman_prescription conjugate inputs");
  Expect(plus_minus.cut_propagator_indices ==
             std::vector<std::size_t>({8, 9}),
         "b63n feynman_prescription scaffold should preserve the D9,D10 cut support");
  Expect(plus_minus.phase_volume_loop_count == 1,
         "b63n feynman_prescription scaffold should identify the one-loop phase volume");
  Expect(plus_minus.provider_strategy == "builtin::cutkosky-phase-space::none",
         "b63n feynman_prescription q-cut should resolve to the no-prescription provider");
  ExpectContains(plus_minus.cutkosky_prefactor,
                 "K_1(eps)",
                 "b63n feynman_prescription scaffold should audit the reviewed K_1 prefactor");
  ExpectContains(plus_minus.branch_ledger_entries.back(),
                 "T_l1=plus_i0, T_l2=minus_i0",
                 "b63n feynman_prescription scaffold should record the first uncut ledger");
  ExpectContains(minus_plus.branch_ledger_entries.back(),
                 "T_l1=minus_i0, T_l2=plus_i0",
                 "b63n feynman_prescription scaffold should record the conjugate uncut ledger");
  Expect(!plus_minus.live_coefficients_available && !minus_plus.live_coefficients_available,
         "b63n feynman_prescription scaffold must not claim coefficient parity");
}

void B63nCutkoskyTransportScaffoldRejectsEtaOnCutDenominatorTest() {
  amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  spec.family.propagators[0].expression = "l1^2-msq+eta";

  ExpectRuntimeErrorContains(
      [&spec]() {
        static_cast<void>(amflow::BuildCutkoskyEtaZeroTransportScaffold(spec));
      },
      "rejects eta insertion on cut denominators",
      "b63n scaffold should fail closed before moving eta through a cut residue");
}

void B63nCutkoskyTransportScaffoldRejectsNonUnitCutPowersTest() {
  amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  spec.targets.front().indices[2] = 2;

  ExpectRuntimeErrorContains(
      [&spec]() {
        static_cast<void>(amflow::BuildCutkoskyEtaZeroTransportScaffold(spec));
      },
      "requires unit powers on reviewed cut propagators",
      "b63n scaffold should fail closed on non-unit cut powers in the reviewed subset");
}

void B63nCutkoskyTransportScaffoldRejectsMutatedReviewedDenominatorTest() {
  amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  spec.family.propagators[1].expression = "(l1+p2)^2";

  ExpectRuntimeErrorContains(
      [&spec]() {
        static_cast<void>(amflow::BuildCutkoskyEtaZeroTransportScaffold(spec));
      },
      "intentionally limited to the exact automatic_phasespace and feynman_prescription "
      "topologies",
      "b63n scaffold should not recognize a reviewed surface after a denominator mutation");
}

void B63nCutkoskyTransportScaffoldRejectsMutatedReviewedMassTest() {
  amflow::ProblemSpec spec = MakeB63nFeynmanPrescriptionSpec(
      amflow::FeynmanPrescription::PlusI0,
      amflow::FeynmanPrescription::MinusI0);
  spec.family.propagators[8].mass = "msq";

  ExpectRuntimeErrorContains(
      [&spec]() {
        static_cast<void>(amflow::BuildCutkoskyEtaZeroTransportScaffold(spec));
      },
      "intentionally limited to the exact automatic_phasespace and feynman_prescription "
      "topologies",
      "b63n scaffold should not recognize a reviewed surface after a mass-field mutation");
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
    EndpointExtractionScalarSimplePoleProducesCoefficientTest();
    EndpointExtractionTwoMasterDiagonalPreservesMasterOrderTest();
    EndpointExtractionRejectsLocalModelResidueMismatchTest();
    EndpointExtractionRejectsBranchLedgerFingerprintMismatchTest();
    EndpointExtractionRejectsStaleContourFingerprintTest();
    Srl5CaseStudyEvidenceMatchesLiveEndpointExtractionTest();
    B63nAutomaticPhaseSpaceCutkoskyTransportScaffoldAuditsEndpointContractTest();
    B63nFeynmanPrescriptionCutkoskyTransportScaffoldRecordsConjugateLedgersTest();
    B63nCutkoskyTransportScaffoldRejectsEtaOnCutDenominatorTest();
    B63nCutkoskyTransportScaffoldRejectsNonUnitCutPowersTest();
    B63nCutkoskyTransportScaffoldRejectsMutatedReviewedDenominatorTest();
    B63nCutkoskyTransportScaffoldRejectsMutatedReviewedMassTest();
  } catch (const std::exception& error) {
    std::cerr << "singular-runtime-lane-tests failed: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
