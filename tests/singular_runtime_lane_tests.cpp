#include "amflow/io/sample_data.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/endpoint_branch_ledger.hpp"
#include "amflow/runtime/continuation_path.hpp"
#include "amflow/runtime/cutkosky_transport.hpp"
#include "amflow/runtime/endpoint_local_model.hpp"
#include "amflow/runtime/lightlike_propagator.hpp"
#include "amflow/solver/series_solver.hpp"

#include <algorithm>
#include <complex>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/multiprecision/cpp_dec_float.hpp>

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

const amflow::CutkoskyPrefactorSeriesTerm& CutkoskyPrefactorTermAt(
    const amflow::CutkoskyPrefactorSeries& series,
    const int eps_order) {
  for (const amflow::CutkoskyPrefactorSeriesTerm& term : series.terms) {
    if (term.eps_order == eps_order) {
      return term;
    }
  }
  throw std::runtime_error("missing Cutkosky prefactor eps order " +
                           std::to_string(eps_order));
}

const amflow::CutkoskyPrefactorSeriesTerm& CutkoskyLaurentTermAt(
    const std::vector<amflow::CutkoskyPrefactorSeriesTerm>& series,
    const int eps_order) {
  for (const amflow::CutkoskyPrefactorSeriesTerm& term : series) {
    if (term.eps_order == eps_order) {
      return term;
    }
  }
  throw std::runtime_error("missing Cutkosky Laurent eps order " +
                           std::to_string(eps_order));
}

const amflow::CutkoskyResidueSeriesTerm& CutkoskyResidueTermAt(
    const amflow::CutkoskyResidueSeries& series,
    const int eps_order,
    const int eta_power,
    const int log_power,
    const std::string& region_key) {
  for (const amflow::CutkoskyResidueSeriesTerm& term : series.terms) {
    if (term.eps_order == eps_order && term.eta_power == eta_power &&
        term.log_power == log_power && term.region_key == region_key) {
      return term;
    }
  }
  throw std::runtime_error("missing synthetic Cutkosky residue term");
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

amflow::CutkoskyResidueSeriesTerm MakeSyntheticB63nResidueTerm(
    const int eps_order,
    const int eta_power,
    const int log_power,
    const std::string& region_key,
    const std::string& real,
    const std::string& imaginary,
    const std::string& coefficient_label,
    const std::string& fixture_id) {
  amflow::CutkoskyResidueSeriesTerm term;
  term.eps_order = eps_order;
  term.eta_power = eta_power;
  term.log_power = log_power;
  term.region_key = region_key;
  term.coefficient_label = coefficient_label;
  // Synthetic-only fixture coefficient; this is not AMFlow parity data.
  term.coefficient = {real, imaginary};
  term.precision.requested_precision_digits = 70;
  term.precision.working_precision_digits = 100;
  term.precision.arithmetic_backend = "cpp_dec_float_100";
  term.precision.summary =
      "lane163 synthetic residue fixture precision metadata; no coefficient "
      "publication";
  term.provenance.source = "lane163 synthetic fixture; not AMFlow final samples";
  term.provenance.derivation = "synthetic hand-authored transport fixture";
  term.provenance.fixture_id = fixture_id;
  term.provenance.synthetic_fixture = true;
  term.provenance.retained_solution_samples_used = false;
  term.provenance.coefficient_published = false;
  return term;
}

amflow::CutkoskyResidueSeries MakeSyntheticB63nResidueSeries(
    std::vector<amflow::CutkoskyResidueSeriesTerm> terms) {
  amflow::CutkoskyResidueSeries series;
  series.series_label = "lane163-synthetic-b63n-residue-series";
  series.requested_precision_digits = 70;
  series.working_precision_digits = 100;
  series.precision_diagnostics =
      "lane163 synthetic residue fixture series; no real residue coefficients";
  if (!terms.empty()) {
    series.min_eps_order = terms.front().eps_order;
    series.max_eps_order = terms.front().eps_order;
    for (const amflow::CutkoskyResidueSeriesTerm& term : terms) {
      series.min_eps_order = std::min(series.min_eps_order, term.eps_order);
      series.max_eps_order = std::max(series.max_eps_order, term.eps_order);
    }
  }
  series.terms = std::move(terms);
  return series;
}

void B63nCutkoskyPrefactorSeriesExpandsReviewedKFactorsTest() {
  const amflow::CutkoskyPrefactorSeries k1 =
      amflow::BuildCutkoskyPrefactorEpsilonSeries(1, 0, 3, 70);
  Expect(k1.loop_count == 1 && k1.terms.size() == 4,
         "b63n K_1 prefactor should emit the requested eps^0..eps^3 terms");
  ExpectContains(k1.formula,
                 "K_1(eps)",
                 "b63n K_1 prefactor should retain the reviewed formula string");
  ExpectContains(k1.source_reference,
                 "AMFlow.m:941-950",
                 "b63n prefactor should cite the reviewed AMFlow source lines");
  ExpectContains(k1.precision_diagnostics,
                 "requested_digits=70",
                 "b63n prefactor should report requested precision");
  ExpectContains(CutkoskyPrefactorTermAt(k1, 0).real,
                 "1.266514795529222143048493290121595486",
                 "b63n K_1 eps^0 coefficient should match the closed form");
  ExpectContains(CutkoskyPrefactorTermAt(k1, 1).real,
                 "3.205579656629814776052568833811351026",
                 "b63n K_1 eps^1 coefficient should match the closed form");
  ExpectContains(CutkoskyPrefactorTermAt(k1, 3).real,
                 "3.422535285265455831395416287729280494",
                 "b63n K_1 eps^3 coefficient should match the closed form");

  const amflow::CutkoskyPrefactorSeries k2 =
      amflow::BuildCutkoskyPrefactorEpsilonSeries(2, 0, 3, 70);
  Expect(k2.loop_count == 2 && k2.terms.size() == 4,
         "b63n K_2 prefactor should emit the requested eps^0..eps^3 terms");
  ExpectContains(k2.formula,
                 "K_2(eps)",
                 "b63n K_2 prefactor should retain the reviewed formula string");
  ExpectContains(CutkoskyPrefactorTermAt(k2, 0).real,
                 "-8.020298636472136866525611927436479798",
                 "b63n K_2 eps^0 coefficient should include the reviewed sign");
  ExpectContains(CutkoskyPrefactorTermAt(k2, 1).real,
                 "-4.059914063369143987574473963550411920",
                 "b63n K_2 eps^1 coefficient should match the closed form");
  ExpectContains(CutkoskyPrefactorTermAt(k2, 3).real,
                 "-1.733876630803810708483436431638325963",
                 "b63n K_2 eps^3 coefficient should match the closed form");

  const amflow::CutkoskyPrefactorSeries kr =
      amflow::BuildCutkoskyPrefactorEpsilonSeries(3, 0, 4, 70);
  Expect(kr.loop_count == 3 && kr.terms.size() == 5,
         "b63n generic K_r prefactor should emit the requested eps^0..eps^4 terms");
  ExpectContains(kr.formula,
                 "K_r(eps)",
                 "b63n generic prefactor should retain the reviewed K_r formula string");
  ExpectContains(CutkoskyPrefactorTermAt(kr, 0).real,
                 "5.078913443827403789501160326729518709",
                 "b63n K_r eps^0 coefficient should match the r=3 closed form");
  ExpectContains(CutkoskyPrefactorTermAt(kr, 2).real,
                 "1.464117517035159373273158303298927815",
                 "b63n K_r eps^2 coefficient should match the r=3 closed form");
  ExpectContains(CutkoskyPrefactorTermAt(kr, 4).real,
                 "7.034444563119589252532449050178716610",
                 "b63n K_r eps^4 coefficient should match the r=3 closed form");

  const std::vector<amflow::CutkoskyPrefactorSeriesTerm> multiplied =
      amflow::MultiplyCutkoskyPrefactorIntoLaurentSeries(
          k1,
          {{-1, "3", "0"}, {0, "5", "0"}},
          -1,
          1);
  Expect(multiplied.size() == 3,
         "b63n prefactor multiplication should preserve the explicit Laurent range");
  ExpectContains(CutkoskyLaurentTermAt(multiplied, -1).real,
                 "3.799544386587666429145479870364786458",
                 "b63n prefactor multiplication should handle residue pole terms");
  ExpectContains(CutkoskyLaurentTermAt(multiplied, 0).real,
                 "1.594931294753555504340017295204203050",
                 "b63n prefactor multiplication should convolve eps^0 terms");
  ExpectContains(CutkoskyLaurentTermAt(multiplied, 1).real,
                 "2.819799803793140591827277792660639686",
                 "b63n prefactor multiplication should convolve eps^1 terms");

  ExpectInvalidArgumentContains(
      []() {
        static_cast<void>(amflow::BuildCutkoskyPrefactorEpsilonSeries(0, 0, 2, 70));
      },
      "positive phase-volume loop count",
      "b63n prefactor should reject r=0");
  ExpectInvalidArgumentContains(
      []() {
        static_cast<void>(amflow::BuildCutkoskyPrefactorEpsilonSeries(1, -1, 2, 70));
      },
      "analytic at eps=0",
      "b63n prefactor should not emit implicit negative-power zeros");
  ExpectInvalidArgumentContains(
      []() {
        static_cast<void>(amflow::BuildCutkoskyPrefactorEpsilonSeries(1, 0, 2, 96));
      },
      "insufficient_precision",
      "b63n prefactor should reject requests beyond its working precision");

  const amflow::CutkoskyEtaZeroTransportAudit audit =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(MakeB63nAutomaticPhaseSpaceSpec());
  Expect(!audit.full_eta_zero_contour_applied,
         "b63n prefactor primitive must not promote the full eta=0 contour flag");
  Expect(!audit.retained_solution_samples_used,
         "b63n prefactor primitive must not introduce final-solution sample input");
}

void B63nSyntheticResidueSeriesPrefactorFeedsEtaZeroSelectorTest() {
  const amflow::CutkoskyPrefactorSeries k1 =
      amflow::BuildCutkoskyPrefactorEpsilonSeries(1, 0, 2, 70);

  const amflow::CutkoskyResidueSeries selected_fixture =
      amflow::MultiplyCutkoskyPrefactorIntoResidueSeries(
          k1,
          MakeSyntheticB63nResidueSeries({
              MakeSyntheticB63nResidueTerm(
                  -1,
                  0,
                  0,
                  "integer",
                  "1.5",
                  "0",
                  "synthetic_test_prefactor_eta0",
                  "lane163-synthetic-prefactor-interop"),
          }),
          -1,
          0);
  const amflow::CutkoskyResidueSeriesTerm& selected_term =
      CutkoskyResidueTermAt(selected_fixture, 0, 0, 0, "integer");
  Expect(selected_term.precision.requested_precision_digits == 70,
         "b63n synthetic residue carrier should retain prefactor precision metadata");
  Expect(selected_term.provenance.synthetic_fixture,
         "b63n synthetic residue carrier should retain synthetic provenance");
  Expect(!selected_term.provenance.retained_solution_samples_used,
         "b63n synthetic residue carrier must not read retained final solution samples");
  Expect(!selected_term.provenance.coefficient_published,
         "b63n synthetic residue carrier must remain non-publishing");
  const amflow::CutkoskyEtaZeroSelectionResult selected =
      amflow::PickCutkoskyEtaZeroTerm(
          amflow::ProjectCutkoskyResidueSeriesToEtaZeroTerms(selected_fixture, 0));
  Expect(selected.success,
         "b63n synthetic residue carrier should interoperate with PickCutkoskyEtaZeroTerm");
  Expect(selected.selected_coefficient_label == "synthetic_test_prefactor_eta0",
         "b63n synthetic residue carrier should preserve selector labels");

  const amflow::CutkoskyResidueSeries missing_order_fixture =
      amflow::MultiplyCutkoskyPrefactorIntoResidueSeries(
          k1,
          MakeSyntheticB63nResidueSeries({
              MakeSyntheticB63nResidueTerm(2,
                                           0,
                                           0,
                                           "integer",
                                           "1.5",
                                           "0",
                                           "synthetic_test_missing_order",
                                           "lane163-synthetic-missing-order"),
          }),
          0,
          0);
  Expect(missing_order_fixture.terms.empty(),
         "b63n synthetic prefactor multiplication must not invent missing eps orders");
  const amflow::CutkoskyEtaZeroSelectionResult missing_order =
      amflow::PickCutkoskyEtaZeroTerm(
          amflow::ProjectCutkoskyResidueSeriesToEtaZeroTerms(missing_order_fixture, 0));
  Expect(!missing_order.success &&
             missing_order.failure_code == "continuation_budget_exhausted",
         "b63n synthetic missing-order fixture should fail closed");
  ExpectContains(missing_order.summary,
                 "received no propagated endpoint terms",
                 "b63n synthetic missing-order fixture must not publish an implicit term");

  const amflow::CutkoskyResidueSeries zero_fixture =
      amflow::MultiplyCutkoskyPrefactorIntoResidueSeries(
          k1,
          MakeSyntheticB63nResidueSeries({
              MakeSyntheticB63nResidueTerm(0,
                                           0,
                                           0,
                                           "integer",
                                           "0",
                                           "0",
                                           "synthetic_test_zero_eta0",
                                           "lane163-synthetic-zero-term"),
          }),
          0,
          0);
  Expect(zero_fixture.terms.empty(),
         "b63n synthetic prefactor multiplication must drop exact zero terms");
  const amflow::CutkoskyEtaZeroSelectionResult zero =
      amflow::PickCutkoskyEtaZeroTerm(
          amflow::ProjectCutkoskyResidueSeriesToEtaZeroTerms(zero_fixture, 0));
  Expect(!zero.success && zero.failure_code == "continuation_budget_exhausted",
         "b63n synthetic zero-term fixture should fail closed");
  ExpectContains(zero.summary,
                 "received no propagated endpoint terms",
                 "b63n synthetic zero-term fixture must not publish an implicit zero");

  const amflow::CutkoskyResidueSeries log_fixture =
      amflow::MultiplyCutkoskyPrefactorIntoResidueSeries(
          k1,
          MakeSyntheticB63nResidueSeries({
              MakeSyntheticB63nResidueTerm(0,
                                           0,
                                           1,
                                           "integer",
                                           "1.25",
                                           "0",
                                           "synthetic_test_log_eta0",
                                           "lane163-synthetic-log-term"),
          }),
          0,
          0);
  const amflow::CutkoskyEtaZeroSelectionResult log =
      amflow::PickCutkoskyEtaZeroTerm(
          amflow::ProjectCutkoskyResidueSeriesToEtaZeroTerms(log_fixture, 0));
  Expect(!log.success && log.failure_code == "continuation_budget_exhausted",
         "b63n synthetic log fixture should fail closed");
  ExpectContains(log.summary,
                 "rejects unresolved logarithmic eta=0 terms",
                 "b63n synthetic log fixture must preserve log rejection");

  const amflow::CutkoskyResidueSeries region_fixture =
      amflow::MultiplyCutkoskyPrefactorIntoResidueSeries(
          k1,
          MakeSyntheticB63nResidueSeries({
              MakeSyntheticB63nResidueTerm(0,
                                           0,
                                           0,
                                           "integer",
                                           "1.25",
                                           "0",
                                           "synthetic_test_region_integer",
                                           "lane163-synthetic-region-integer"),
              MakeSyntheticB63nResidueTerm(0,
                                           0,
                                           0,
                                           "fractional",
                                           "2.25",
                                           "0",
                                           "synthetic_test_region_fractional",
                                           "lane163-synthetic-region-fractional"),
          }),
          0,
          0);
  const amflow::CutkoskyEtaZeroSelectionResult region =
      amflow::PickCutkoskyEtaZeroTerm(
          amflow::ProjectCutkoskyResidueSeriesToEtaZeroTerms(region_fixture, 0));
  Expect(!region.success && region.failure_code == "continuation_budget_exhausted",
         "b63n synthetic region fixture should fail closed");
  ExpectContains(region.summary,
                 "rejects multiple endpoint regions",
                 "b63n synthetic region fixture must preserve region-choice rejection");
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
  Expect(!audit.full_eta_zero_contour_applied,
         "b63n automatic_phasespace scaffold must not claim full contour transport");
  ExpectContains(audit.cutkosky_prefactor,
                 "K_2(eps)",
                 "b63n automatic_phasespace scaffold should audit the reviewed K_2 prefactor");
  Expect(audit.residue_model_kind ==
             "automatic_phasespace::one-mass-three-body-residue",
         "b63n automatic_phasespace scaffold should parse the reviewed residue model");
  Expect(audit.uncut_denominator_indices ==
             std::vector<std::size_t>({1, 3, 5, 6}),
         "b63n automatic_phasespace scaffold should preserve D2,D4,D6,D7 as uncut weights");
  Expect(audit.endpoint_poles.size() == 6,
         "b63n automatic_phasespace scaffold should extract endpoint pole candidates");
  ExpectContains(audit.endpoint_poles[0].classification,
                 "massless-pair soft endpoint",
                 "b63n automatic_phasespace scaffold should classify the q2=0 endpoint");
  Expect(audit.eta_contour_waypoints.size() == 5,
         "b63n automatic_phasespace scaffold should plan a deterministic contour");
  ExpectContains(audit.contour_fingerprint,
                 "fnv1a64:",
                 "b63n automatic_phasespace scaffold should fingerprint the contour plan");
  ExpectContains(audit.eta_zero_selection_audit,
                 "no endpoint coefficient terms are produced",
                 "b63n automatic_phasespace scaffold should keep PickZero audit partial");
  ExpectContains(audit.coefficient_gap,
                 "Live Cutkosky residue coefficients are not yet implemented",
                 "b63n scaffold should explicitly report the remaining coefficient gap");
  ExpectContains(audit.summary,
                 "full_eta_zero_contour_applied stays false",
                 "b63n automatic_phasespace scaffold should explicitly avoid overclaiming");
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
  Expect(plus_minus.residue_model_kind ==
             "feynman_prescription::two-body-residue::plus-minus",
         "b63n feynman_prescription scaffold should parse the plus/minus residue model");
  Expect(minus_plus.residue_model_kind ==
             "feynman_prescription::two-body-residue::minus-plus",
         "b63n feynman_prescription scaffold should parse the conjugate residue model");
  ExpectContains(plus_minus.physical_integration_domain,
                 "lambda(10,1,2/5)=1809/25",
                 "b63n feynman_prescription scaffold should audit the positive Kallen "
                 "discriminant");
  Expect(plus_minus.endpoint_poles.size() == 4,
         "b63n feynman_prescription scaffold should extract endpoint and threshold poles");
  Expect(plus_minus.contour_fingerprint != minus_plus.contour_fingerprint,
         "b63n feynman_prescription conjugate ledgers should fingerprint distinct plans");
  ExpectContains(plus_minus.branch_ledger_entries.back(),
                 "T_l1=plus_i0, T_l2=minus_i0",
                 "b63n feynman_prescription scaffold should record the first uncut ledger");
  ExpectContains(minus_plus.branch_ledger_entries.back(),
                 "T_l1=minus_i0, T_l2=plus_i0",
                 "b63n feynman_prescription scaffold should record the conjugate uncut ledger");
  Expect(!plus_minus.live_coefficients_available && !minus_plus.live_coefficients_available,
         "b63n feynman_prescription scaffold must not claim coefficient parity");
}

void B63nCutkoskyBranchLedgerExposesStructuredFieldsAndLegacySerializationTest() {
  const amflow::CutkoskyEtaZeroTransportAudit automatic_audit =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(MakeB63nAutomaticPhaseSpaceSpec());
  const std::vector<std::string> expected_automatic_entries = {
      "loop_prescriptions=[0, 0]",
      "cut propagators D1,D3,D5 resolved as prescription-insensitive real "
      "phase-space cuts",
  };

  Expect(automatic_audit.branch_ledger.size() == 2,
         "b63n automatic_phasespace structured branch ledger should mirror legacy entries");
  Expect(amflow::SerializeCutkoskyBranchLedgerSummaries(
             automatic_audit.branch_ledger) == expected_automatic_entries,
         "b63n automatic_phasespace structured branch ledger should serialize to the "
         "legacy audit strings");
  Expect(automatic_audit.branch_ledger_entries == expected_automatic_entries,
         "b63n automatic_phasespace legacy branch ledger strings should remain exact");
  const amflow::CutkoskyBranchLedgerEntry& automatic_cut =
      automatic_audit.branch_ledger[1];
  Expect(automatic_cut.cut_support == std::vector<std::size_t>({0, 2, 4}),
         "b63n automatic_phasespace structured branch ledger should expose cut support");
  Expect(automatic_cut.eta_half_plane == "lower",
         "b63n automatic_phasespace structured branch ledger should expose eta half-plane");
  Expect(automatic_cut.prescriptions.size() == 3 &&
             automatic_cut.prescriptions[0].target == "D1" &&
             automatic_cut.prescriptions[0].prescription ==
                 amflow::FeynmanPrescription::None,
         "b63n automatic_phasespace structured branch ledger should expose cut "
         "prescriptions");
  ExpectContains(automatic_cut.branch_provenance,
                 "reviewed automatic_phasespace",
                 "b63n automatic_phasespace structured branch ledger should expose provenance");

  const amflow::CutkoskyEtaZeroTransportAudit plus_minus =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(
          MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::PlusI0,
                                          amflow::FeynmanPrescription::MinusI0));
  const std::vector<std::string> expected_plus_entries = {
      "loop_prescriptions=[1, -1, 0]",
      "uncut ledger: T_l1=plus_i0, T_l2=minus_i0",
  };
  Expect(amflow::SerializeCutkoskyBranchLedgerSummaries(plus_minus.branch_ledger) ==
             expected_plus_entries,
         "b63n feynman_prescription plus/minus branch ledger should serialize to the "
         "legacy audit strings");
  Expect(plus_minus.branch_ledger_entries == expected_plus_entries,
         "b63n feynman_prescription plus/minus legacy branch ledger strings should remain "
         "exact");
  const amflow::CutkoskyBranchLedgerEntry& plus_uncut = plus_minus.branch_ledger[1];
  Expect(plus_uncut.cut_support == std::vector<std::size_t>({8, 9}),
         "b63n feynman_prescription structured branch ledger should expose q-cut support");
  Expect(plus_uncut.eta_half_plane == "lower",
         "b63n feynman_prescription structured branch ledger should expose eta half-plane");
  Expect(plus_uncut.prescriptions.size() == 2 &&
             plus_uncut.prescriptions[0].target == "T_l1" &&
             plus_uncut.prescriptions[0].prescription ==
                 amflow::FeynmanPrescription::PlusI0 &&
             plus_uncut.prescriptions[1].target == "T_l2" &&
             plus_uncut.prescriptions[1].prescription ==
                 amflow::FeynmanPrescription::MinusI0,
         "b63n feynman_prescription structured branch ledger should expose uncut "
         "subintegral prescriptions");
  ExpectContains(plus_uncut.branch_provenance,
                 "conjugate loop-subintegral",
                 "b63n feynman_prescription structured branch ledger should expose provenance");

  const amflow::CutkoskyEtaZeroTransportAudit minus_plus =
      amflow::BuildCutkoskyEtaZeroTransportScaffold(
          MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::MinusI0,
                                          amflow::FeynmanPrescription::PlusI0));
  const std::vector<std::string> expected_minus_entries = {
      "loop_prescriptions=[-1, 1, 0]",
      "uncut ledger: T_l1=minus_i0, T_l2=plus_i0",
  };
  Expect(amflow::SerializeCutkoskyBranchLedgerSummaries(minus_plus.branch_ledger) ==
             expected_minus_entries,
         "b63n feynman_prescription minus/plus branch ledger should serialize to the "
         "legacy audit strings");
  Expect(minus_plus.branch_ledger_entries == expected_minus_entries,
         "b63n feynman_prescription minus/plus legacy branch ledger strings should remain "
         "exact");
  Expect(minus_plus.branch_ledger[1].prescriptions[0].prescription ==
             amflow::FeynmanPrescription::MinusI0 &&
             minus_plus.branch_ledger[1].prescriptions[1].prescription ==
                 amflow::FeynmanPrescription::PlusI0,
         "b63n feynman_prescription structured branch ledger should preserve conjugate "
         "prescription order");
}

void B63nCutkoskyResidueEndpointModelBuildsContourPlanTest() {
  const amflow::CutkoskyResidueEndpointModel automatic_model =
      amflow::BuildCutkoskyResidueEndpointModel(MakeB63nAutomaticPhaseSpaceSpec());
  const amflow::CutkoskyResidueEndpointModel feynman_model =
      amflow::BuildCutkoskyResidueEndpointModel(
          MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::PlusI0,
                                          amflow::FeynmanPrescription::MinusI0));

  Expect(automatic_model.parsed && feynman_model.parsed,
         "b63n endpoint models should parse the two reviewed surfaces");
  ExpectContains(automatic_model.kallen_discriminant,
                 "(q2-81)*(q2-121)",
                 "b63n automatic endpoint model should keep the reviewed Kallen roots");
  ExpectContains(feynman_model.kallen_discriminant,
                 "1809/25",
                 "b63n feynman endpoint model should keep the reviewed Kallen value");
  Expect(automatic_model.eta_contour_waypoints.front().eta == "eta=-64*I",
         "b63n endpoint contour should start in the lower half-plane for NegIm");
  Expect(automatic_model.eta_contour_waypoints.back().eta == "eta=0",
         "b63n endpoint contour should terminate at eta=0");
  ExpectContains(automatic_model.coefficient_gap,
                 "not AMFlow parity evidence",
                 "b63n endpoint model should state that the plan is not coefficient evidence");
}

void B63nAutomaticPhaseSpaceSymbolicIntegrandAssemblesUncutWeightsTest() {
  const amflow::CutkoskySymbolicIntegrand integrand =
      amflow::BuildAutomaticPhaseSpaceSymbolicIntegrand(
          MakeB63nAutomaticPhaseSpaceSpec());

  Expect(integrand.surface_label == "phase[1,2,1,1,1,1,1]",
         "b63n symbolic integrand should bind the accepted weighted phase surface");
  Expect(integrand.model_kind ==
             "automatic_phasespace::one-mass-three-body-residue",
         "b63n symbolic integrand should reuse the endpoint-model kind");
  Expect(integrand.residue_variables ==
             std::vector<std::string>({"q2", "cos_theta_a", "cos_theta_b"}),
         "b63n symbolic integrand should preserve endpoint-model variables");
  Expect(integrand.factors.size() == 4,
         "b63n symbolic integrand should assemble D2,D4,D6,D7 factors");
  Expect(integrand.factors[0].denominator_id == "D2" &&
             integrand.factors[0].propagator_power == 2,
         "b63n symbolic integrand should record the D2 squared weight");
  Expect(integrand.factors[1].denominator_id == "D4" &&
             integrand.factors[1].propagator_power == 1,
         "b63n symbolic integrand should record the D4 weight");
  Expect(integrand.factors[2].denominator_id == "D6" &&
             integrand.factors[2].propagator_power == 1,
         "b63n symbolic integrand should record the D6 weight");
  Expect(integrand.factors[3].denominator_id == "D7" &&
             integrand.factors[3].propagator_power == 1,
         "b63n symbolic integrand should record the D7 weight");
  Expect(integrand.factors[0].role ==
             "D2=(l1+p1)^2 angular weight with power 2",
         "b63n symbolic integrand should map from the recorded D2 role");
  ExpectContains(integrand.factors[1].structural_form,
                 "D4(q2,cos_theta_a,cos_theta_b)",
                 "b63n symbolic integrand should expose the D4 angular variable shape");
  ExpectContains(integrand.coefficient_policy,
                 "no endpoint Laurent coefficients evaluated or published",
                 "b63n symbolic integrand must remain coefficient-free");

  const std::string audit =
      amflow::SerializeCutkoskySymbolicIntegrandAudit(integrand);
  const std::string expected_audit =
      "kind=b63n-automatic-phasespace-symbolic-integrand\n"
      "surface=phase[1,2,1,1,1,1,1]\n"
      "model=automatic_phasespace::one-mass-three-body-residue\n"
      "parameterization=dPhi_3(P;m,0,0)=dq2/(2*pi)*dPhi_2(P;m,sqrt(q2))*dPhi_2(q;0,0)\n"
      "domain=q2 in [0,81] with angular moments for D2,D4,D6,D7\n"
      "variables=q2,cos_theta_a,cos_theta_b\n"
      "coefficient_policy=coefficient-free symbolic assembly; no endpoint Laurent "
      "coefficients evaluated or published\n"
      "factor_count=4\n"
      "factor[0]=denominator=D2;denominator_index=1;power=2;form="
      "inverse_denominator_weight[D2(q2,cos_theta_a)];role=D2=(l1+p1)^2 "
      "angular weight with power 2;propagator=(l1+p1)^2\n"
      "factor[1]=denominator=D4;denominator_index=3;power=1;form="
      "inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)];role="
      "D4=(l1+l2+p1)^2 angular weight;propagator=(l1+l2+p1)^2\n"
      "factor[2]=denominator=D6;denominator_index=5;power=1;form="
      "inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)];role="
      "D6=(l1+l2+p2)^2 angular weight;propagator=(l1+l2+p2)^2\n"
      "factor[3]=denominator=D7;denominator_index=6;power=1;form="
      "inverse_denominator_weight[D7(q2,cos_theta_a)];role=D7=(l1+p2)^2 "
      "angular weight;propagator=(l1+p2)^2\n";
  Expect(audit == expected_audit,
         "b63n symbolic integrand audit serialization should be deterministic");
}

void B63nPickCutkoskyEtaZeroTermSelectsOnlyLiveSymbolicTermTest() {
  const amflow::CutkoskyEtaZeroSelectionResult selected =
      amflow::PickCutkoskyEtaZeroTerm({
          {"integer", -1, 0, "symbolic-cutkosky-pole"},
          {"integer", 0, 0, "symbolic-cutkosky-eta0"},
          {"integer", 1, 0, "symbolic-cutkosky-tail"},
      });

  Expect(selected.success,
         "b63n PickCutkoskyEtaZeroTerm should select an explicit live eta^0 term");
  Expect(selected.selected_coefficient_label == "symbolic-cutkosky-eta0",
         "b63n PickCutkoskyEtaZeroTerm should preserve the selected coefficient label");
  Expect(selected.dropped_singular_terms ==
             std::vector<std::string>({"symbolic-cutkosky-pole"}),
         "b63n PickCutkoskyEtaZeroTerm should audit dropped singular powers");

  const amflow::CutkoskyEtaZeroSelectionResult missing =
      amflow::PickCutkoskyEtaZeroTerm({{"integer", -1, 0, "symbolic-cutkosky-pole"}});
  Expect(!missing.success && missing.failure_code == "continuation_budget_exhausted",
         "b63n PickCutkoskyEtaZeroTerm should fail closed when eta^0 is absent");
  ExpectContains(missing.summary,
                 "does not publish an implicit coefficient",
                 "b63n PickCutkoskyEtaZeroTerm must not invent an endpoint coefficient");

  const amflow::CutkoskyEtaZeroSelectionResult ambiguous =
      amflow::PickCutkoskyEtaZeroTerm({
          {"integer", 0, 0, "symbolic-cutkosky-eta0-a"},
          {"integer", 0, 0, "symbolic-cutkosky-eta0-b"},
      });
  Expect(!ambiguous.success &&
             ambiguous.failure_code == "continuation_budget_exhausted",
         "b63n PickCutkoskyEtaZeroTerm should fail closed on ambiguous eta^0 terms");
}

void B63nAutomaticPhaseSpaceFirstCutkoskyCoefficientAuditTest() {
  amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  spec.targets = {amflow::TargetIntegral{"phase", {1, 0, 1, 0, 1, 0, 0}}};

  const amflow::CutkoskyResidueCoefficientAudit audit =
      amflow::BuildAutomaticPhaseSpaceFirstCutkoskyCoefficientAudit(spec);

  Expect(audit.live_coefficients_available,
         "b63n first Cutkosky coefficient audit should mark the selected residue live");
  Expect(!audit.retained_solution_samples_used,
         "b63n first Cutkosky coefficient audit must not use retained solution samples");
  Expect(!audit.full_eta_zero_contour_applied,
         "b63n first Cutkosky coefficient audit must keep full contour false");
  Expect(audit.master_label == "phase[1,0,1,0,1,0,0]",
         "b63n first Cutkosky coefficient audit should name the pure cut master");
  Expect(audit.transport_scope == "eta-zero-selected-endpoint-coefficients",
         "b63n first Cutkosky coefficient audit should stay selected-master scoped");
  Expect(audit.residue_model_kind ==
             "automatic_phasespace::pure-cut-three-body-volume",
         "b63n first Cutkosky coefficient audit should use the pure phase-volume model");
  Expect(audit.endpoint_local_model_kind == "cutkosky-pure-phase-volume-r0",
         "b63n first Cutkosky coefficient audit should classify the selected endpoint");
  ExpectContains(audit.contour_fingerprint,
                 "fnv1a64:",
                 "b63n first Cutkosky coefficient audit should fingerprint the contour");
  ExpectContains(audit.extraction_fingerprint,
                 "fnv1a64:",
                 "b63n first Cutkosky coefficient audit should fingerprint the extraction");
  ExpectContains(audit.eta_zero_selection_audit,
                 "selected the unique eta^0 residue term",
                 "b63n first Cutkosky coefficient audit should use live PickZero input");
  ExpectContains(audit.summary,
                 "final_solution_samples_used_as_input=false",
                 "b63n first Cutkosky coefficient audit should publish anti-fake provenance");
  ExpectContains(audit.summary,
                 "full_eta_zero_contour_applied stays false",
                 "b63n first Cutkosky coefficient audit should avoid full-lane overclaiming");
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

amflow::TargetIntegral B64agTarget(std::vector<int> indices) {
  return amflow::TargetIntegral{"gauge", std::move(indices)};
}

amflow::MasterIntegral B64agMaster(std::vector<int> indices) {
  amflow::MasterIntegral master;
  master.family = "gauge";
  master.indices = std::move(indices);
  return master;
}

std::vector<amflow::TargetIntegral> B64agReviewedTargets() {
  return {
      B64agTarget({1, 1, 1, 1, 1, 0, -1, 0, 0}),
      B64agTarget({1, 1, 1, 1, 1, 0, 0, -1, 0}),
      B64agTarget({1, 1, 1, 1, 1, 0, 0, 0, -1}),
      B64agTarget({1, 1, 1, 0, 1, 0, 0, 0, 0}),
      B64agTarget({1, 1, 1, -1, 1, 0, 0, 0, 0}),
      B64agTarget({0, 1, 1, 1, 1, 0, 0, 0, 0}),
      B64agTarget({0, 1, 1, 1, 1, -1, 0, 0, 0}),
      B64agTarget({1, 1, 1, 1, 1, 0, 0, 0, 0}),
      B64agTarget({1, 1, 1, 1, 1, -1, 0, 0, 0}),
  };
}

std::vector<amflow::MasterIntegral> B64agReviewedReductionMasters() {
  return {
      B64agMaster({1, 1, 1, 0, 1, 0, 0, 0, 0}),
      B64agMaster({1, 1, 1, -1, 1, 0, 0, 0, 0}),
      B64agMaster({0, 1, 1, 1, 1, 0, 0, 0, 0}),
      B64agMaster({0, 1, 1, 1, 1, -1, 0, 0, 0}),
      B64agMaster({1, 1, 1, 1, 1, 0, 0, 0, 0}),
      B64agMaster({1, 1, 1, 1, 1, -1, 0, 0, 0}),
  };
}

std::string B64agMasterLabel(const amflow::MasterIntegral& master) {
  return amflow::TargetIntegral{master.family, master.indices}.Label();
}

int B64agPickZeroRuleSPower() {
  return amflow::LightlikeGaugeLinkFinitePartTerm{}.power;
}

int B64agSinglePolePower() {
  return B64agPickZeroRuleSPower() - 1;
}

amflow::LightlikeGaugeLinkFinitePartTerm B64agEndpointFixtureTerm(
    const int power,
    std::string coefficient,
    std::string region_key = "integer") {
  amflow::LightlikeGaugeLinkFinitePartTerm term;
  term.region_key = std::move(region_key);
  term.power = power;
  term.coefficient = std::move(coefficient);
  return term;
}

amflow::LightlikeGaugeLinkTargetReductionTerm B64agReductionFixtureTerm(
    const std::string& target_label,
    const std::string& master_label,
    const int gaugex_power_shift,
    std::string coefficient) {
  amflow::LightlikeGaugeLinkTargetReductionTerm term;
  term.target_label = target_label;
  term.master_label = master_label;
  term.gaugex_power_shift = gaugex_power_shift;
  term.coefficient = std::move(coefficient);
  return term;
}

amflow::LightlikeGaugeLinkTargetReductionTerm B64agReductionFixtureTerm(
    const amflow::TargetIntegral& target,
    const amflow::MasterIntegral& master,
    const int gaugex_power_shift,
    std::string coefficient) {
  return B64agReductionFixtureTerm(target.Label(),
                                   B64agMasterLabel(master),
                                   gaugex_power_shift,
                                   std::move(coefficient));
}

std::vector<amflow::LightlikeGaugeLinkSixMasterEndpointTerms>
B64agSixMasterEndpointFixture() {
  const std::vector<amflow::MasterIntegral> masters = B64agReviewedReductionMasters();
  return {
      {B64agMasterLabel(masters.front()),
       {B64agEndpointFixtureTerm(B64agSinglePolePower(), "F0_singular"),
        B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(), "F0_finite")}},
      {B64agMasterLabel(masters[1]),
       {B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(), "F1_finite")}},
      {B64agMasterLabel(masters[2]),
       {B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(), "F2_fixture")}},
      {B64agMasterLabel(masters[3]),
       {B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(), "F3_fixture")}},
      {B64agMasterLabel(masters[4]),
       {B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(), "F4_fixture")}},
      {B64agMasterLabel(masters[5]),
       {B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(), "F5_fixture")}},
  };
}

std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm>
B64agTwoMasterReductionRow(const amflow::TargetIntegral& target,
                           const int gaugex_power_shift) {
  const std::vector<amflow::MasterIntegral> masters = B64agReviewedReductionMasters();
  return {
      B64agReductionFixtureTerm(target, masters.front(), gaugex_power_shift, "R0"),
      B64agReductionFixtureTerm(target, masters[1], gaugex_power_shift, "R1"),
  };
}

std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm>
B64agTwoMasterReductionRows(const std::vector<amflow::TargetIntegral>& targets) {
  std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm> reduction_terms;
  for (const amflow::TargetIntegral& target : targets) {
    const int affected_power_sum = target.indices[3] + target.indices[4];
    const std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm> row =
        B64agTwoMasterReductionRow(target, affected_power_sum);
    reduction_terms.insert(reduction_terms.end(), row.begin(), row.end());
  }
  return reduction_terms;
}

int B64agSixMasterFiniteSourcePower(const std::size_t master_index) {
  const std::vector<int> finite_source_powers = {1, 0, 1, -1, 2, 1};
  return finite_source_powers.at(master_index);
}

std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm>
B64agSixMasterReductionRows(const std::vector<amflow::TargetIntegral>& targets) {
  const std::vector<amflow::MasterIntegral> masters = B64agReviewedReductionMasters();
  std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm> reduction_terms;
  for (const amflow::TargetIntegral& target : targets) {
    const int affected_power_sum = target.indices[3] + target.indices[4];
    for (std::size_t master_index = 0; master_index < masters.size(); ++master_index) {
      reduction_terms.push_back(B64agReductionFixtureTerm(
          target,
          masters[master_index],
          affected_power_sum - B64agSixMasterFiniteSourcePower(master_index),
          "R" + std::to_string(master_index) + "_full"));
    }
  }
  return reduction_terms;
}

std::string B64agRetainedReductionRaw() {
  return
      "{{j[gauge,1,1,1,0,1,0,0,0,0],"
      "j[gauge,1,1,1,-1,1,0,0,0,0],"
      "j[gauge,0,1,1,1,1,0,0,0,0],"
      "j[gauge,0,1,1,1,1,-1,0,0,0],"
      "j[gauge,1,1,1,1,1,0,0,0,0],"
      "j[gauge,1,1,1,1,1,-1,0,0,0]},"
      "{j[gauge,1,1,1,1,1,0,-1,0,0]->"
      "{0,0,0,0,0,gaugex^(-2)},"
      "j[gauge,1,1,1,1,1,0,0,-1,0]->"
      "{8/(3*gaugex^2),-4/gaugex,2/gaugex^2,-2/gaugex,"
      "1/(2*gaugex^3),(-1-4*gaugex)/gaugex^3},"
      "j[gauge,1,1,1,1,1,0,0,0,-1]->"
      "{-11/(3*gaugex),4,0,2,-gaugex^(-2),"
      "(1+2*gaugex)/gaugex^2},"
      "j[gauge,1,1,1,0,1,0,0,0,0]->{gaugex^(-1),0,0,0,0,0},"
      "j[gauge,1,1,1,-1,1,0,0,0,0]->{0,1,0,0,0,0},"
      "j[gauge,0,1,1,1,1,0,0,0,0]->{0,0,gaugex^(-2),0,0,0},"
      "j[gauge,0,1,1,1,1,-1,0,0,0]->{0,0,0,gaugex^(-2),0,0},"
      "j[gauge,1,1,1,1,1,0,0,0,0]->{0,0,0,0,gaugex^(-2),0},"
      "j[gauge,1,1,1,1,1,-1,0,0,0]->{0,0,0,0,0,gaugex^(-2)}}}";
}

bool HasB64agReductionTerm(
    const std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm>& terms,
    const std::string& target_label,
    const std::string& master_label,
    const int gaugex_power_shift,
    const std::string& coefficient) {
  return std::any_of(
      terms.begin(),
      terms.end(),
      [&](const amflow::LightlikeGaugeLinkTargetReductionTerm& term) {
        return term.target_label == target_label &&
               term.master_label == master_label &&
               term.gaugex_power_shift == gaugex_power_shift &&
               term.log_power == 0 &&
               term.coefficient == coefficient;
      });
}

using B64agTestBigFloat = boost::multiprecision::cpp_dec_float_100;
using B64agTestBigComplex = std::complex<B64agTestBigFloat>;

std::string B64agBigFloatText(const B64agTestBigFloat& value) {
  std::ostringstream stream;
  stream << std::setprecision(70) << value;
  return stream.str();
}

std::vector<std::string> B64agHighPrecisionBoundaryValues(
    const B64agTestBigComplex& regular_coefficient) {
  const std::string coefficient = B64agBigFloatText(regular_coefficient.real());
  return {
      "(" + coefficient + ")/40",
      "(" + coefficient + ")*5/6",
      "11",
      "13",
      "17",
      "19",
  };
}

amflow::ProblemSpec MakeB64agGaugeLinkProblemSpec() {
  amflow::ProblemSpec spec;
  spec.family.name = "gauge";
  spec.family.loop_momenta = {"l1", "l2", "l3"};
  spec.family.propagators = {
      amflow::Propagator("l1^2"),
      amflow::Propagator("l2^2"),
      amflow::Propagator("l3^2"),
      amflow::Propagator("1 + l1*n"),
      amflow::Propagator("1/2 + l1*n + l2*n + l3*n"),
      amflow::Propagator("l1^2 + 2*l1*l2 + l2^2"),
      amflow::Propagator("l1^2 + 2*l1*l3 + l3^2"),
      amflow::Propagator("l2^2 + 2*l2*l3 + l3^2"),
      amflow::Propagator("-1 + l2^2 + 2*l2*n"),
  };
  spec.kinematics.incoming_momenta = {"n"};
  spec.kinematics.scalar_product_rules = {{"n^2", "-1"}};
  spec.kinematics.invariants = {"n2"};
  spec.targets = B64agReviewedTargets();
  return spec;
}

amflow::LightlikeGaugeLinkRuntimeState MakeB64agGaugeLinkRuntimeState() {
  amflow::LightlikeGaugeLinkRuntimeState state;
  state.amflow_state_input = true;
  state.solution_sample_cache_enabled = true;
  state.benchmark_id = "linear_propagator";
  state.family = "gauge";
  state.integral_kind = "loop";
  state.variable = "gaugex";
  state.start_location = "gaugex -> 1/40";
  state.target_location = "gaugex=0";
  state.boundary_state_kind = "amflow_finite_solution_samples";
  state.boundary_point = "gaugex -> 1/40";
  state.singular_points = {"gaugex=0"};
  state.boundary_file_names = {"boundary", "diffeq", "reduction", "solution", "solve.wl"};
  state.boundary_file_raws["diffeq"] =
      "{{j[gauge,1,1,1,0,1,0,0,0,0],j[gauge,1,1,1,-1,1,0,0,0,0],"
      "j[gauge,0,1,1,1,1,0,0,0,0],j[gauge,0,1,1,1,1,-1,0,0],"
      "j[gauge,1,1,1,1,1,0,0,0,0],j[gauge,1,1,1,1,1,-1,0,0,0]},"
      "{gaugex},"
      "{{{(1 + 20*gaugex - 18*eps*gaugex)/(gaugex*(1 + 2*gaugex)),"
      "(24*(-1 + eps)*gaugex)/(1 + 2*gaugex),0,0,0,0},"
      "{(5 - 5*eps + 22*gaugex)/(gaugex^2*(1 + 2*gaugex)),"
      "(2*(-3 + 3*eps - 14*gaugex))/(gaugex*(1 + 2*gaugex)),0,0,0,0},"
      "{0,0,(-7 + 8*eps - 18*gaugex)/(gaugex*(1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(12*(-gaugex + eps*gaugex))/((1 + 2*gaugex)*(1 + 4*gaugex)),0,0},"
      "{0,0,(3 - 2*eps + 6*gaugex)/(2*gaugex^3*(1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(6*(-3 + 3*eps - 4*gaugex))/((1 + 2*gaugex)*(1 + 4*gaugex)),0,0},"
      "{(-22*(-gaugex + eps*gaugex))/(1 + 2*gaugex),"
      "(24*(-1 + eps)*gaugex^2)/(1 + 2*gaugex),"
      "(-8*(-2*gaugex + 2*eps*gaugex))/((1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(12*(-gaugex^2 + eps*gaugex^2))/((1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(2*(1 + 2*eps*gaugex))/(gaugex*(1 + 2*gaugex)),"
      "(6*(-1 + eps))/(1 + 2*gaugex)},"
      "{(4*(-7 + 7*eps - 11*gaugex))/(3*(1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(-8*(-1 + eps)*gaugex)/(1 + 4*gaugex),"
      "(-3 + 2*eps + 2*gaugex)/(gaugex*(1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(-8*(gaugex - eps*gaugex))/((1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(-2*(-1 + eps))/(gaugex*(1 + 2*gaugex)*(1 + 4*gaugex)),"
      "(2*(-1 + 2*eps - 7*gaugex))/(gaugex*(1 + 2*gaugex)*(1 + 4*gaugex))}}}}";
  state.boundary_file_raws["reduction"] = B64agRetainedReductionRaw();
  state.diffeq_variables = {"gaugex"};
  state.epsilon_samples = {"101/208000", "51/104000"};
  for (const amflow::TargetIntegral& target : B64agReviewedTargets()) {
    state.masters.push_back(B64agMaster(target.indices));
    state.targets.push_back(target);
  }
  state.reduction_masters = B64agReviewedReductionMasters();
  state.diffeq_masters = B64agReviewedReductionMasters();
  return state;
}

void B64agGaugeLinkTransportScaffoldAuditsReviewedSurfaceTest() {
  const amflow::ProblemSpec spec = MakeB64agGaugeLinkProblemSpec();
  const amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();

  const amflow::LightlikeGaugeLinkTransportAudit audit =
      amflow::BuildLightlikeGaugeLinkTransportScaffold(spec, state);

  Expect(amflow::IsLightlikeGaugeLinkEtaZeroRuntimeState(state),
         "b64ag scaffold should recognize the retained linear_propagator gaugex state");
  Expect(audit.reviewed_surface,
         "b64ag scaffold should recognize the exact gauge-link topology");
  Expect(!audit.live_coefficients_available,
         "b64ag scaffold must not claim live gauge-link endpoint coefficients");
  Expect(!audit.runtime_scaffold_consumes_retained_solution_samples,
         "b64ag scaffold must not consume retained final solution samples as runtime evidence");
  Expect(audit.retained_solution_samples_available,
         "b64ag audit should record that the retained legacy state still has solution samples");
  Expect(!audit.full_eta_zero_contour_applied,
         "b64ag partial scaffold must keep the full eta-zero contour flag false");
  Expect(!audit.ir_subtraction_applied,
         "b64ag partial scaffold must not claim endpoint IR subtraction on coefficients");
  Expect(audit.desolver_local_variable == "eta",
         "b64ag audit should preserve the gaugex to DESolver eta naming bridge");
  Expect(audit.affected_propagator_indices == std::vector<std::size_t>({3, 4}),
         "b64ag GenerateSquare scaffold should mark D4,D5 as affected");
  ExpectContains(audit.generated_square_propagators[3],
                 "(1 + l1*n)/gaugex",
                 "b64ag GenerateSquare scaffold should rewrite the first linear denominator");
  ExpectContains(audit.generated_square_propagators[4],
                 "(1/2 + l1*n + l2*n + l3*n)/gaugex",
                 "b64ag GenerateSquare scaffold should rewrite the second linear denominator");
  Expect(audit.target_normalizations.size() == 9,
         "b64ag audit should cover the retained nine-target packet surface");
  Expect(audit.target_normalizations.front().normalization_factor == "gaugex^(-2)",
         "b64ag target normalization should apply the reviewed affected-power exponent");
  Expect(audit.target_normalizations[4].normalization_factor == "1",
         "b64ag target normalization should preserve zero affected-power sums");
  Expect(audit.diffeq_masters_cover_reduction_masters,
         "b64ag state should audit that DE masters contain reduced masters");
  Expect(audit.diffeq_matrix_parsed,
         "b64ag next layer should parse the retained rational gaugex DE matrix");
  Expect(audit.diffeq_matrix_row_count == 6 && audit.diffeq_matrix_column_count == 6,
         "b64ag DE matrix audit should preserve the reviewed six-master shape");
  Expect(audit.diffeq_matrix_nonzero_cell_count > 0,
         "b64ag DE matrix audit should count parsed nonzero matrix cells");
  Expect(audit.diffeq_poles.size() == 3,
         "b64ag pole extraction should find the endpoint and two nonzero DE poles");
  Expect(audit.pole_candidates == std::vector<std::string>(
                                      {"gaugex=-0.5", "gaugex=-0.25", "gaugex=0"}),
         "b64ag pole candidates should come from parsed gaugex matrix denominators");
  Expect(audit.contour_half_plane == "lower",
         "b64ag contour planning should use the reviewed lower-half-plane route");
  Expect(audit.contour_waypoints.size() == 5,
         "b64ag contour planning should produce finite-boundary endpoint waypoints");
  Expect(audit.contour_waypoints.front().find("0.025") == 0 &&
             audit.contour_waypoints.back() == "0",
         "b64ag contour should run from gaugex=1/40 to gaugex=0");
  Expect(!audit.contour_fingerprint.empty(),
         "b64ag contour planning should fingerprint the parsed pole and waypoint audit");
  Expect(audit.endpoint_local_model_kind == "regular-singular-finite-part-r0",
         "b64ag endpoint audit should classify gaugex=0 as a finite-part singular endpoint");
  ExpectContains(audit.dropped_term_audit,
                 "PickZeroRuleS-compatible finite-part extraction",
                 "b64ag contour audit should preserve the finite-part extraction contract");
  ExpectContains(audit.coefficient_gap,
                 "Live gauge-link endpoint coefficients are not implemented",
                 "b64ag scaffold should explicitly report the remaining coefficient gap");
}

void B64agGaugeLinkDiffeqParserRejectsMalformedMatrixTest() {
  ExpectRuntimeErrorContains(
      []() {
        static_cast<void>(amflow::ParseLightlikeGaugeLinkDiffeqMatrixRaw(
            "{{j[gauge,1]}, {eta}, {{{1/eta}}}}"));
      },
      "single variable gaugex",
      "b64ag diffeq parser should reject eta-hardcoded matrix metadata");
}

void B64agGaugeLinkSquareFamilyRejectsStrictLightlikeReplacementTest() {
  amflow::ProblemSpec spec = MakeB64agGaugeLinkProblemSpec();
  spec.kinematics.scalar_product_rules = {{"n^2", "0"}};

  ExpectRuntimeErrorContains(
      [&spec]() {
        static_cast<void>(amflow::BuildLightlikeGaugeLinkSquareFamily(spec));
      },
      "requires AMFlow replacement n^2 -> -1",
      "b64ag scaffold should not accept the older strict lightlike auxiliary surface");
}

void B64agGaugeLinkSquareFamilyRejectsMutatedDenominatorTest() {
  amflow::ProblemSpec spec = MakeB64agGaugeLinkProblemSpec();
  spec.family.propagators[4].expression = "1/2 + l1*n + l2*n";

  ExpectRuntimeErrorContains(
      [&spec]() {
        static_cast<void>(amflow::BuildLightlikeGaugeLinkSquareFamily(spec));
      },
      "rejects non-reviewed denominator",
      "b64ag scaffold should fail closed on source denominator drift");
}

void B64agGaugeLinkRuntimeStateRejectsMissingBoundaryInputsTest() {
  amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();
  state.boundary_file_names = {"solution"};

  Expect(!amflow::IsLightlikeGaugeLinkEtaZeroRuntimeState(state),
         "b64ag runtime-state detection should reject solution-only retained states");
  ExpectRuntimeErrorContains(
      [&state]() {
        static_cast<void>(amflow::BuildLightlikeGaugeLinkTransportScaffold(
            MakeB64agGaugeLinkProblemSpec(),
            state));
      },
      "non-linear_propagator gaugex=0 state",
      "b64ag scaffold should fail closed when raw boundary/diffeq/reduction inputs are absent");
}

void B64agGaugeLinkRuntimeStateRejectsMasterSetDriftTest() {
  amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();
  state.diffeq_masters.pop_back();

  ExpectRuntimeErrorContains(
      [&state]() {
        static_cast<void>(amflow::BuildLightlikeGaugeLinkTransportScaffold(
            MakeB64agGaugeLinkProblemSpec(),
            state));
      },
      "master_set_instability",
      "b64ag scaffold should use the reviewed fail-closed code for DE master drift");
}

void B64agGaugeLinkInlineReductionParserExpandsSyntheticLaurentRowsTest() {
  const std::vector<amflow::TargetIntegral> targets = B64agReviewedTargets();
  const std::vector<amflow::MasterIntegral> masters = B64agReviewedReductionMasters();
  const std::string raw =
      "{{j[gauge,1,1,1,0,1,0,0,0,0],"
      "j[gauge,1,1,1,-1,1,0,0,0,0]},"
      "{j[gauge,1,1,1,1,1,0,-1,0,0]->"
      "{gaugex^(-2),(1+2*gaugex)/gaugex^2}}}";

  const std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm> terms =
      amflow::ParseLightlikeGaugeLinkTargetReductionRaw(raw);

  Expect(terms.size() == 3,
         "b64ag inline reduction parser should split synthetic Laurent cells into rows");
  Expect(HasB64agReductionTerm(terms,
                               targets.front().Label(),
                               B64agMasterLabel(masters.front()),
                               -2,
                               "1"),
         "b64ag inline reduction parser should preserve a monomial gaugex shift");
  Expect(HasB64agReductionTerm(terms,
                               targets.front().Label(),
                               B64agMasterLabel(masters[1]),
                               -2,
                               "1"),
         "b64ag inline reduction parser should split the constant numerator term");
  Expect(HasB64agReductionTerm(terms,
                               targets.front().Label(),
                               B64agMasterLabel(masters[1]),
                               -1,
                               "2"),
         "b64ag inline reduction parser should split the linear numerator term");
}

void B64agGaugeLinkRetainedReductionParserCoversInlineStateTableTest() {
  const amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();
  const std::vector<amflow::TargetIntegral> targets = B64agReviewedTargets();
  const std::vector<amflow::MasterIntegral> masters = B64agReviewedReductionMasters();

  const std::vector<amflow::LightlikeGaugeLinkTargetReductionTerm> terms =
      amflow::ParseLightlikeGaugeLinkRetainedTargetReduction(state);

  Expect(terms.size() == 20,
         "b64ag retained inline reduction parser should expand every nonzero retained row");
  Expect(std::all_of(terms.begin(),
                     terms.end(),
                     [](const amflow::LightlikeGaugeLinkTargetReductionTerm& term) {
                       return term.log_power == 0 && !term.target_label.empty() &&
                              !term.master_label.empty() &&
                              !term.coefficient.empty();
                     }),
         "b64ag retained inline reduction parser should emit complete non-log rows");
  Expect(HasB64agReductionTerm(terms,
                               targets[1].Label(),
                               B64agMasterLabel(masters[5]),
                               -3,
                               "-1"),
         "b64ag retained inline reduction parser should split the M5 cubic-pole constant");
  Expect(HasB64agReductionTerm(terms,
                               targets[1].Label(),
                               B64agMasterLabel(masters[5]),
                               -2,
                               "-4"),
         "b64ag retained inline reduction parser should split the M5 cubic-pole linear term");
  Expect(HasB64agReductionTerm(terms,
                               targets[2].Label(),
                               B64agMasterLabel(masters[5]),
                               -1,
                               "2"),
         "b64ag retained inline reduction parser should split the positive M5 linear term");
}

void B64agGaugeLinkFinitePartSelectsPowerZeroAndDropsSingularTermsTest() {
  const amflow::LightlikeGaugeLinkFinitePartResult result =
      amflow::ExtractLightlikeGaugeLinkEndpointFinitePart({
          {"integer", -2, 0, "singular_-2"},
          {"integer", -1, 0, "singular_-1"},
          {"integer", 0, 0, "finite"},
      });

  Expect(result.success,
         "b64ag finite-part helper should accept the single integer-region subset");
  Expect(result.ir_subtraction_applied,
         "b64ag finite-part helper should record finite-part subtraction");
  Expect(result.finite_part_coefficient == "finite",
         "b64ag finite-part helper should select the power-zero coefficient");
  Expect(result.dropped_singular_terms.size() == 2,
         "b64ag finite-part helper should audit dropped singular endpoint powers");
}

void B64agGaugeLinkFinitePartRejectsMultipleRegionsTest() {
  const amflow::LightlikeGaugeLinkFinitePartResult result =
      amflow::ExtractLightlikeGaugeLinkEndpointFinitePart({
          {"integer-region-a", 0, 0, "a"},
          {"integer-region-b", 0, 0, "b"},
      });

  Expect(!result.success,
         "b64ag finite-part helper should fail closed on multiple endpoint regions");
  Expect(result.failure_code == "continuation_budget_exhausted",
         "b64ag multiple-region rejection should use the reviewed continuation failure code");
  ExpectContains(result.summary,
                 "multiple integer endpoint regions",
                 "b64ag finite-part rejection should explain the unsupported structure");
}

void B64agGaugeLinkFinitePartDoesNotPublishImplicitZeroTest() {
  const amflow::LightlikeGaugeLinkFinitePartResult positive_only =
      amflow::ExtractLightlikeGaugeLinkEndpointFinitePart({
          {"integer", 2, 0, "positive"},
      });
  const amflow::LightlikeGaugeLinkFinitePartResult missing_zero =
      amflow::ExtractLightlikeGaugeLinkEndpointFinitePart({
          {"integer", -1, 0, "singular"},
      });

  Expect(!positive_only.success && !missing_zero.success,
         "b64ag finite-part helper should not publish implicit zero coefficients");
  Expect(positive_only.failure_code == "continuation_budget_exhausted" &&
             missing_zero.failure_code == "continuation_budget_exhausted",
         "b64ag implicit-zero cases should fail with the reviewed continuation code");
  Expect(positive_only.finite_part_coefficient.empty() &&
             missing_zero.finite_part_coefficient.empty(),
         "b64ag implicit-zero cases should not populate coefficient strings");
}

void B64agGaugeLinkReducedFinitePartAppliesTargetReductionBeforePickZeroRuleSTest() {
  const std::vector<amflow::TargetIntegral> targets = B64agReviewedTargets();
  const amflow::TargetIntegral target = targets.front();
  const amflow::LightlikeGaugeLinkReducedFinitePartResult result =
      amflow::EvaluateLightlikeGaugeLinkReducedFiniteParts(
          targets,
          B64agSixMasterEndpointFixture(),
          B64agTwoMasterReductionRows(targets));

  Expect(result.success,
         "b64ag reduced finite-part functional should accept fixture endpoint terms");
  Expect(result.failures.empty(),
         "b64ag reduced finite-part functional should not record target failures");
  Expect(!result.retained_solution_samples_used,
         "b64ag reduced finite-part functional must not read retained solution samples");
  Expect(!result.full_eta_zero_contour_applied,
         "b64ag reduced finite-part functional must not claim full contour success");
  Expect(result.ir_subtraction_applied,
         "b64ag reduced finite-part functional should record finite-part subtraction");
  Expect(result.targets.size() == targets.size(),
         "b64ag reduced finite-part functional should publish the reviewed target packet");

  const amflow::LightlikeGaugeLinkReducedFinitePartTarget& reduced =
      result.targets.front();
  Expect(reduced.success,
         "b64ag reduced target should succeed after row reduction and normalization");
  Expect(reduced.target_label == target.Label(),
         "b64ag reduced target should preserve the retained target label");
  Expect(reduced.affected_power_sum == 2,
         "b64ag reduced target should apply the D4,D5 affected-power sum");
  Expect(reduced.normalization_factor == "gaugex^(-2)",
         "b64ag reduced target should audit the reviewed normalization factor");
  Expect(reduced.dropped_singular_terms.size() == 1,
         "b64ag reduced target should drop the normalized singular endpoint term");
  ExpectContains(reduced.finite_part_coefficient,
                 "R0",
                 "b64ag reduced coefficient should include the first target-reduction entry");
  ExpectContains(reduced.finite_part_coefficient,
                 "F0_finite",
                 "b64ag reduced coefficient should include the first finite endpoint term");
  ExpectContains(reduced.finite_part_coefficient,
                 "R1",
                 "b64ag reduced coefficient should include the second target-reduction entry");
  ExpectContains(reduced.finite_part_coefficient,
                 "F1_finite",
                 "b64ag reduced coefficient should include the second finite endpoint term");
  ExpectContains(reduced.summary,
                 "before PickZeroRuleS-compatible finite-part extraction",
                 "b64ag target reduction must run before finite-part selection");
  ExpectContains(result.summary,
                 "retained_solution_samples_used=false",
                 "b64ag reduced result should publish non-consumption provenance");
  ExpectContains(result.summary,
                 "full_eta_zero_contour_applied=false",
                 "b64ag reduced result should publish full-contour non-promotion");
}

void B64agGaugeLinkReducedFinitePartRejectsMissingTermsTest() {
  const amflow::TargetIntegral target = B64agReviewedTargets()[3];
  std::vector<amflow::LightlikeGaugeLinkSixMasterEndpointTerms> endpoint_terms =
      B64agSixMasterEndpointFixture();
  endpoint_terms.pop_back();

  const amflow::LightlikeGaugeLinkReducedFinitePartResult missing_endpoint =
      amflow::EvaluateLightlikeGaugeLinkReducedFiniteParts(
          {target},
          endpoint_terms,
          B64agTwoMasterReductionRow(target, 2));
  const amflow::LightlikeGaugeLinkReducedFinitePartResult missing_reduction =
      amflow::EvaluateLightlikeGaugeLinkReducedFiniteParts(
          {target},
          B64agSixMasterEndpointFixture(),
          {});

  Expect(!missing_endpoint.success,
         "b64ag reduced finite-part functional should fail when a six-master term is absent");
  Expect(missing_endpoint.failures.size() == 1,
         "b64ag missing endpoint rejection should record one diagnostic");
  Expect(missing_endpoint.failures.front().failure_code == "boundary_unsolved",
         "b64ag missing endpoint rejection should use the boundary failure code");
  ExpectContains(missing_endpoint.failures.front().summary,
                 "missing supplied endpoint terms",
                 "b64ag missing endpoint rejection should explain the absent six-master term");
  Expect(!missing_reduction.success,
         "b64ag reduced finite-part functional should fail when target reduction is absent");
  Expect(missing_reduction.targets.size() == 1 && missing_reduction.failures.size() == 1,
         "b64ag missing reduction rejection should produce per-target diagnostics");
  Expect(missing_reduction.failures.front().failure_code == "boundary_unsolved",
         "b64ag missing reduction rejection should use the boundary failure code");
  ExpectContains(missing_reduction.failures.front().summary,
                 "missing the retained target-reduction row",
                 "b64ag missing reduction rejection should explain the incomplete row");
}

void B64agGaugeLinkReducedFinitePartRejectsMultipleRegionsTest() {
  const amflow::TargetIntegral target = B64agReviewedTargets()[3];
  std::vector<amflow::LightlikeGaugeLinkSixMasterEndpointTerms> endpoint_terms =
      B64agSixMasterEndpointFixture();
  endpoint_terms.front().endpoint_terms = {
      B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(),
                               "F0_region_a",
                               "integer-region-a"),
      B64agEndpointFixtureTerm(B64agPickZeroRuleSPower(),
                               "F0_region_b",
                               "integer-region-b"),
  };

  const amflow::LightlikeGaugeLinkReducedFinitePartResult result =
      amflow::EvaluateLightlikeGaugeLinkReducedFiniteParts(
          {target},
          endpoint_terms,
          {B64agReductionFixtureTerm(target.Label(),
                                     endpoint_terms.front().master_label,
                                     1,
                                     "R0")});

  Expect(!result.success,
         "b64ag reduced finite-part functional should reject multiple endpoint regions");
  Expect(result.targets.size() == 1 && result.failures.size() == 1,
         "b64ag multiple-region rejection should remain target scoped");
  Expect(result.failures.front().failure_code == "continuation_budget_exhausted",
         "b64ag multiple-region rejection should reuse the finite-part failure code");
  ExpectContains(result.failures.front().summary,
                 "multiple integer endpoint regions",
                 "b64ag multiple-region rejection should come from the PickZeroRuleS selector");
  Expect(!result.full_eta_zero_contour_applied,
         "b64ag failed reduced finite-part result must not set the full contour flag");
}

void B64agGaugeLinkReducedFinitePartSelectedPrefixKeepsFullContourFalseTest() {
  const amflow::TargetIntegral target = B64agReviewedTargets()[3];
  const amflow::LightlikeGaugeLinkReducedFinitePartResult result =
      amflow::EvaluateLightlikeGaugeLinkReducedFiniteParts(
          {target},
          B64agSixMasterEndpointFixture(),
          B64agTwoMasterReductionRow(target, 1));

  Expect(result.success,
         "b64ag reduced finite-part functional should accept the selected endpoint prefix");
  Expect(result.targets.size() == 1,
         "b64ag selected-prefix reduced functional should stay selected-target scoped");
  Expect(result.targets.front().affected_power_sum == 1,
         "b64ag selected-prefix target should apply its D4,D5 normalization");
  Expect(result.targets.front().normalization_factor == "gaugex^(-1)",
         "b64ag selected-prefix target should record the selected normalization factor");
  Expect(!result.retained_solution_samples_used,
         "b64ag selected-prefix reduced functional must not consume retained solution samples");
  Expect(!result.full_eta_zero_contour_applied,
         "b64ag selected-prefix reduced functional must not promote to full contour");
  ExpectContains(result.summary,
                 "full_eta_zero_contour_applied=false",
                 "b64ag selected-prefix summary should record no full contour success");
}

void B64agGaugeLinkFirstEndpointCoefficientAuditTest() {
  amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();
  state.targets = {B64agTarget({1, 1, 1, 0, 1, 0, 0, 0, 0})};

  const amflow::LightlikeGaugeLinkSelectedCoefficientAudit audit =
      amflow::BuildLightlikeGaugeLinkFirstEndpointCoefficientAudit(state);

  Expect(audit.live_coefficients_available,
         "b64ag first endpoint audit should mark the selected coefficient live");
  Expect(!audit.retained_solution_samples_used,
         "b64ag first endpoint audit must not use retained final solution samples");
  Expect(!audit.full_eta_zero_contour_applied,
         "b64ag first endpoint audit must keep full contour false");
  Expect(audit.ir_subtraction_applied,
         "b64ag first endpoint audit should record finite-part subtraction");
  Expect(audit.master_label == "gauge[1,1,1,0,1,0,0,0,0]",
         "b64ag first endpoint audit should name only the selected master");
  Expect(audit.transport_scope == "eta-zero-selected-endpoint-coefficients",
         "b64ag first endpoint audit should stay selected-master scoped");
  Expect(audit.endpoint_local_model_kind == "regular-singular-finite-part-r0",
         "b64ag first endpoint audit should classify the gaugex=0 endpoint");
  ExpectContains(audit.contour_fingerprint,
                 "fnv1a64:",
                 "b64ag first endpoint audit should fingerprint the contour");
  ExpectContains(audit.extraction_fingerprint,
                 "fnv1a64:",
                 "b64ag first endpoint audit should fingerprint extraction");
  ExpectContains(audit.eta_zero_selection_audit,
                 "selected the power-zero coefficient",
                 "b64ag first endpoint audit should use finite-part selection");
  ExpectContains(audit.summary,
                 "final_solution_samples_used_as_input=false",
                 "b64ag first endpoint audit should publish anti-fake provenance");
  ExpectContains(audit.summary,
                 "full_eta_zero_contour_applied stays false",
                 "b64ag first endpoint audit should avoid full-lane overclaiming");
}
void B64agGaugeLinkFiniteBoundaryTransportFeedsReducedFinitePartChainTest() {
  amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();
  state.epsilon_samples = {"1"};
  const std::vector<amflow::LightlikeGaugeLinkFiniteBoundarySample> boundary_samples = {
      {"1", {"3/20", "5", "11", "13", "17", "19"}},
  };

  const amflow::LightlikeGaugeLinkEndpointTransportResult transport =
      amflow::TransportLightlikeGaugeLinkFiniteBoundaryEndpointTerms(
          state, boundary_samples);

  Expect(transport.success,
         "b64ag finite-boundary transport should publish all six reviewed masters");
  Expect(!transport.partial_success,
         "b64ag finite-boundary transport should not be marked partial after six-master transport");
  Expect(!transport.retained_solution_samples_used,
         "b64ag finite-boundary transport must not consume retained final solution samples");
  Expect(transport.retained_solution_samples_available,
         "b64ag finite-boundary audit should record legacy solution sample availability");
  Expect(!transport.full_eta_zero_contour_applied,
         "b64ag finite-boundary transport must keep the full contour flag false");
  Expect(transport.requested_master_count == 6 && transport.transported_master_count == 6,
         "b64ag finite-boundary transport should report six of six live masters");
  Expect(transport.endpoint_terms.size() == 6,
         "b64ag finite-boundary transport should return six endpoint rows");
  Expect(transport.remaining_master_gaps.empty(),
         "b64ag finite-boundary transport should not leave explicit master gaps");
  ExpectContains(transport.extraction_fingerprint,
                 "fnv1a64:",
                 "b64ag finite-boundary transport should fingerprint extraction");
  const std::vector<amflow::MasterIntegral> masters = B64agReviewedReductionMasters();
  for (std::size_t master_index = 0; master_index < masters.size(); ++master_index) {
    Expect(transport.endpoint_terms[master_index].master_label ==
               B64agMasterLabel(masters[master_index]),
           "b64ag finite-boundary transport should preserve reviewed master order");
    Expect(!transport.endpoint_terms[master_index].endpoint_terms.empty(),
           "b64ag finite-boundary transport should publish endpoint terms for every master");
    Expect(!transport.endpoint_terms[master_index].endpoint_terms.front().coefficient.empty(),
           "b64ag finite-boundary transport should publish nonempty live coefficients");
  }
  Expect(transport.endpoint_terms[0].endpoint_terms.size() == 1 &&
             transport.endpoint_terms[0].endpoint_terms.front().power == 1 &&
             transport.endpoint_terms[0].endpoint_terms.front().coefficient == "6",
         "b64ag finite-boundary transport should recover the synthetic first coefficient");
  Expect(transport.endpoint_terms[1].endpoint_terms.size() == 1 &&
             transport.endpoint_terms[1].endpoint_terms.front().power == 0 &&
             transport.endpoint_terms[1].endpoint_terms.front().coefficient == "5",
         "b64ag finite-boundary transport should recover the synthetic companion coefficient");
  Expect(transport.endpoint_terms[2].endpoint_terms.size() >= 1 &&
             transport.endpoint_terms[2].endpoint_terms.front().power == 1 &&
             transport.endpoint_terms[2].endpoint_terms.front().log_power == 0,
         "b64ag finite-boundary transport should publish the second-block regular branch");
  bool has_second_companion_singular = false;
  bool has_second_companion_endpoint_log = false;
  for (const amflow::LightlikeGaugeLinkFinitePartTerm& term :
       transport.endpoint_terms[3].endpoint_terms) {
    has_second_companion_singular =
        has_second_companion_singular || (term.power == -1 && term.log_power == 0);
    has_second_companion_endpoint_log =
        has_second_companion_endpoint_log || (term.power == 0 && term.log_power == 1);
  }
  Expect(has_second_companion_singular && has_second_companion_endpoint_log,
         "b64ag finite-boundary transport should publish second-block companion Laurent and log terms");
  Expect(transport.endpoint_terms[4].endpoint_terms.size() >= 1 &&
             transport.endpoint_terms[4].endpoint_terms.front().power == 2 &&
             transport.endpoint_terms[4].endpoint_terms.front().log_power == 0,
         "b64ag finite-boundary transport should publish the downstream regular branch");
  Expect(transport.endpoint_terms[5].endpoint_terms.size() >= 2 &&
             transport.endpoint_terms[5].endpoint_terms[0].power == 1 &&
             transport.endpoint_terms[5].endpoint_terms[0].log_power == 0,
         "b64ag finite-boundary transport should publish downstream companion Laurent terms");
  ExpectContains(transport.summary,
                 "all six reviewed gauge-link DE masters",
                 "b64ag finite-boundary transport should report full six-master coverage");

  const std::vector<amflow::TargetIntegral> targets = B64agReviewedTargets();
  const amflow::LightlikeGaugeLinkReducedFinitePartResult reduced =
      amflow::EvaluateLightlikeGaugeLinkReducedFiniteParts(
          targets,
          transport.endpoint_terms,
          B64agSixMasterReductionRows(targets));

  Expect(reduced.success,
         "b64ag reduced finite-part primitive should accept live six-master endpoint terms: " +
             reduced.summary +
             (reduced.failures.empty() ? "" : " first_failure=" +
                                                 reduced.failures.front().summary));
  Expect(reduced.failures.empty(),
         "b64ag reduced finite-part chain should not record failures for the synthetic packet");
  Expect(!reduced.retained_solution_samples_used,
         "b64ag reduced finite-part chain must not consume retained solution samples");
  Expect(!reduced.full_eta_zero_contour_applied,
         "b64ag reduced finite-part chain must not promote full contour success");
  Expect(reduced.ir_subtraction_applied,
         "b64ag reduced finite-part chain should still run finite-part selection");
  Expect(reduced.targets.size() == targets.size(),
         "b64ag reduced finite-part chain should publish the reviewed target packet");
  Expect(reduced.targets.front().success,
         "b64ag reduced finite-part chain should publish a successful synthetic target");
  Expect(reduced.targets.front().affected_power_sum == 2 &&
             reduced.targets.front().normalization_factor == "gaugex^(-2)",
         "b64ag reduced finite-part chain should keep D4,D5 normalization");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "R0_full",
                 "b64ag reduced finite-part chain should include the live first row");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "6",
                 "b64ag reduced finite-part chain should include the live first coefficient");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "R1_full",
                 "b64ag reduced finite-part chain should include the live companion row");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "5",
                 "b64ag reduced finite-part chain should include the live companion coefficient");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "R2_full",
                 "b64ag reduced finite-part chain should include the live second-block row");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "R3_full",
                 "b64ag reduced finite-part chain should include the live second-block companion row");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "R4_full",
                 "b64ag reduced finite-part chain should include the live downstream row");
  ExpectContains(reduced.targets.front().finite_part_coefficient,
                 "R5_full",
                 "b64ag reduced finite-part chain should include the live downstream companion row");
  ExpectContains(reduced.targets.front().summary,
                 "before PickZeroRuleS-compatible finite-part extraction",
                 "b64ag reduced finite-part chain should preserve reducer ordering");
}

void B64agGaugeLinkFiniteBoundaryTransportPreservesBigComplexMultiEpsilonPrecisionTest() {
  amflow::LightlikeGaugeLinkRuntimeState state = MakeB64agGaugeLinkRuntimeState();
  state.epsilon_samples = {"1", "2/2"};
  const B64agTestBigComplex first_coefficient{
      B64agTestBigFloat(
          "1.2345678901234567890123456789012345678901234567890123456789"),
      B64agTestBigFloat(0)};
  const B64agTestBigComplex second_coefficient{
      B64agTestBigFloat(
          "9.8765432109876543210987654321098765432109876543210987654321"),
      B64agTestBigFloat(0)};
  const std::string first_text = B64agBigFloatText(first_coefficient.real());
  const std::string second_text = B64agBigFloatText(second_coefficient.real());
  const std::vector<amflow::LightlikeGaugeLinkFiniteBoundarySample> boundary_samples = {
      {"1", B64agHighPrecisionBoundaryValues(first_coefficient)},
      {"2/2", B64agHighPrecisionBoundaryValues(second_coefficient)},
  };

  const amflow::LightlikeGaugeLinkEndpointTransportResult transport =
      amflow::TransportLightlikeGaugeLinkFiniteBoundaryEndpointTerms(
          state, boundary_samples);

  Expect(transport.success,
         "b64ag high-precision multi-epsilon transport should succeed: " +
             transport.summary);
  Expect(!transport.retained_solution_samples_used,
         "b64ag high-precision multi-epsilon transport must not use retained solutions");
  Expect(!transport.full_eta_zero_contour_applied,
         "b64ag high-precision multi-epsilon transport must not promote full contour");
  Expect(transport.epsilon_sample_count == 2 &&
             transport.epsilon_endpoint_terms.size() == 2,
         "b64ag high-precision transport should carry both epsilon samples");
  Expect(transport.endpoint_terms.empty(),
         "b64ag multi-epsilon transport should avoid an ambiguous flat endpoint mirror");
  Expect(transport.epsilon_endpoint_terms[0].epsilon_sample == "1" &&
             transport.epsilon_endpoint_terms[1].epsilon_sample == "2/2",
         "b64ag high-precision transport should preserve epsilon sample labels");
  Expect(transport.epsilon_endpoint_terms[0].endpoint_terms.size() == 6 &&
             transport.epsilon_endpoint_terms[1].endpoint_terms.size() == 6,
         "b64ag high-precision transport should publish six masters per sample");
  const std::string first_published =
      transport.epsilon_endpoint_terms[0]
          .endpoint_terms[0]
          .endpoint_terms[0]
          .coefficient;
  const std::string second_published =
      transport.epsilon_endpoint_terms[1]
          .endpoint_terms[0]
          .endpoint_terms[0]
          .coefficient;
  ExpectContains(first_published,
                 first_text.substr(0, 56),
                 "b64ag high-precision transport should preserve >=50 first-sample digits");
  ExpectContains(second_published,
                 second_text.substr(0, 56),
                 "b64ag high-precision transport should preserve >=50 second-sample digits");
  ExpectContains(transport.summary,
                 "retained_solution_samples_used=false",
                 "b64ag high-precision transport summary should record non-consumption");
  ExpectContains(transport.summary,
                 "full_eta_zero_contour_applied=false",
                 "b64ag high-precision transport summary should keep full contour false");
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
    B63nCutkoskyPrefactorSeriesExpandsReviewedKFactorsTest();
    B63nSyntheticResidueSeriesPrefactorFeedsEtaZeroSelectorTest();
    B63nAutomaticPhaseSpaceCutkoskyTransportScaffoldAuditsEndpointContractTest();
    B63nFeynmanPrescriptionCutkoskyTransportScaffoldRecordsConjugateLedgersTest();
    B63nCutkoskyBranchLedgerExposesStructuredFieldsAndLegacySerializationTest();
    B63nCutkoskyResidueEndpointModelBuildsContourPlanTest();
    B63nAutomaticPhaseSpaceSymbolicIntegrandAssemblesUncutWeightsTest();
    B63nPickCutkoskyEtaZeroTermSelectsOnlyLiveSymbolicTermTest();
    B63nAutomaticPhaseSpaceFirstCutkoskyCoefficientAuditTest();
    B63nCutkoskyTransportScaffoldRejectsEtaOnCutDenominatorTest();
    B63nCutkoskyTransportScaffoldRejectsNonUnitCutPowersTest();
    B63nCutkoskyTransportScaffoldRejectsMutatedReviewedDenominatorTest();
    B63nCutkoskyTransportScaffoldRejectsMutatedReviewedMassTest();
    B64agGaugeLinkTransportScaffoldAuditsReviewedSurfaceTest();
    B64agGaugeLinkDiffeqParserRejectsMalformedMatrixTest();
    B64agGaugeLinkSquareFamilyRejectsStrictLightlikeReplacementTest();
    B64agGaugeLinkSquareFamilyRejectsMutatedDenominatorTest();
    B64agGaugeLinkRuntimeStateRejectsMissingBoundaryInputsTest();
    B64agGaugeLinkRuntimeStateRejectsMasterSetDriftTest();
    B64agGaugeLinkInlineReductionParserExpandsSyntheticLaurentRowsTest();
    B64agGaugeLinkRetainedReductionParserCoversInlineStateTableTest();
    B64agGaugeLinkFinitePartSelectsPowerZeroAndDropsSingularTermsTest();
    B64agGaugeLinkFinitePartRejectsMultipleRegionsTest();
    B64agGaugeLinkFinitePartDoesNotPublishImplicitZeroTest();
    B64agGaugeLinkReducedFinitePartAppliesTargetReductionBeforePickZeroRuleSTest();
    B64agGaugeLinkReducedFinitePartRejectsMissingTermsTest();
    B64agGaugeLinkReducedFinitePartRejectsMultipleRegionsTest();
    B64agGaugeLinkReducedFinitePartSelectedPrefixKeepsFullContourFalseTest();
    B64agGaugeLinkFirstEndpointCoefficientAuditTest();
    B64agGaugeLinkFiniteBoundaryTransportFeedsReducedFinitePartChainTest();
    B64agGaugeLinkFiniteBoundaryTransportPreservesBigComplexMultiEpsilonPrecisionTest();
  } catch (const std::exception& error) {
    std::cerr << "singular-runtime-lane-tests failed: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
