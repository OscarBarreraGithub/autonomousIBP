#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/cutkosky_transport.hpp"

namespace {

void Expect(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void ExpectContains(const std::string& value,
                    const std::string& needle,
                    const std::string& message) {
  Expect(value.find(needle) != std::string::npos, message);
}

constexpr const char* kD246ReferenceEvidenceSidecar =
    "tools/reference-harness/specs/m6/lane146/"
    "b63n-d246-weighted-residue-reference-evidence.json";

std::filesystem::path LocateRepositoryRoot() {
  std::filesystem::path current = std::filesystem::current_path();
  for (int depth = 0; depth < 5; ++depth) {
    if (std::filesystem::exists(current / "tools/reference-harness") &&
        std::filesystem::exists(
            current / "tests/cutkosky_weighted_residue_tests.cpp")) {
      return current;
    }
    if (!current.has_parent_path() || current == current.parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return std::filesystem::current_path();
}

std::string ReadTextFileIfPresent(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read b63n D2/D4/D6 evidence sidecar: " +
                             path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool TextContains(const std::string& value, const std::string& needle) {
  return value.find(needle) != std::string::npos;
}

bool HasD246PublishedReferenceSidecarEvidence() {
  const std::filesystem::path sidecar =
      LocateRepositoryRoot() / kD246ReferenceEvidenceSidecar;
  const std::string evidence = ReadTextFileIfPresent(sidecar);
  return TextContains(evidence, "b63n") &&
         TextContains(evidence, "automatic_phasespace") &&
         TextContains(evidence, "phase[1,2,1,1,1,1,1]") &&
         TextContains(evidence, "D2") &&
         TextContains(evidence, "D4") &&
         TextContains(evidence, "D6") &&
         (TextContains(evidence, "amflow") ||
          TextContains(evidence, "AMFlow")) &&
         (TextContains(evidence, "reference") ||
          TextContains(evidence, "Reference")) &&
         TextContains(evidence, "coefficient") &&
         TextContains(evidence, "passed") &&
         TextContains(evidence, "final_solution_samples_used_as_input") &&
         TextContains(evidence, "false");
}

bool IsSolvedScopedWeight(
    const amflow::CutkoskyScopedWeightedResidueEvaluation& evaluation) {
  return evaluation.publication_gate_passed ||
         evaluation.live_coefficients_available ||
         evaluation.reference_validation_passed ||
         evaluation.failure_code.empty();
}

template <typename Callable>
void ExpectExceptionContains(Callable&& callable,
                             const std::string& expected_substring,
                             const std::string& message) {
  try {
    callable();
  } catch (const std::exception& error) {
    Expect(std::string(error.what()).find(expected_substring) != std::string::npos,
           message);
    return;
  }
  throw std::runtime_error(message);
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
  for (const std::size_t index : {std::size_t{0}, std::size_t{2}, std::size_t{4}}) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Cut;
    spec.family.propagators[index].prescription =
        static_cast<int>(amflow::FeynmanPrescription::None);
  }
  spec.kinematics.invariants = {"s", "msq"};
  spec.kinematics.numeric_substitutions = {{"s", "100"}, {"msq", "1"}};
  spec.targets = {
      amflow::TargetIntegral{"phase", {1, 2, 1, 1, 1, 1, 1}}};
  return spec;
}

const amflow::CutkoskyResidueSeriesTerm& ResidueTermAt(
    const amflow::CutkoskyResidueSeries& series,
    const int eps_order) {
  for (const amflow::CutkoskyResidueSeriesTerm& term : series.terms) {
    if (term.eps_order == eps_order && term.eta_power == 0 &&
        term.log_power == 0 && term.region_key == "integer") {
      return term;
    }
  }
  throw std::runtime_error("missing scoped weighted residue eps order " +
                           std::to_string(eps_order));
}

void ScopedAutomaticPhaseSpaceWeightedResidueStopsAtPublicationGateTest() {
  const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          1,
          2,
          70);

  Expect(evaluation.reviewed_surface,
         "scoped weighted residue evaluator should bind the reviewed surface");
  Expect(evaluation.evaluation_attempted,
         "scoped weighted residue evaluator should mark the attempted evaluation");
  Expect(evaluation.publication_gate_checked,
         "scoped weighted residue evaluator should run the publication gate");
  Expect(!evaluation.publication_gate_passed,
         "synthetic weighted residue candidate must not pass publication");
  Expect(!evaluation.live_coefficients_available,
         "scoped weighted residue evaluator must not claim live coefficients");
  Expect(!evaluation.retained_solution_samples_used,
         "scoped weighted residue evaluator must not use retained final samples");
  Expect(!evaluation.full_eta_zero_contour_applied,
         "scoped weighted residue evaluator must not promote the full contour");
  Expect(evaluation.failure_code == "boundary_unsolved",
         "publication-gated synthetic weighted residue should fail as boundary_unsolved");
  Expect(evaluation.surface_label == "phase[1,2,1,1,1,1,1]",
         "scoped weighted residue evaluator should retain the weighted target");
  Expect(evaluation.selected_weight_denominator == "D2" &&
             evaluation.selected_weight_denominator_index == 1 &&
             evaluation.selected_weight_power == 2,
         "scoped weighted residue evaluator should bind the D2 squared weight");
  Expect(evaluation.moment_cross_validation_gate.passed,
         "scoped weighted residue evaluator should reuse the D2,D4,D6,D7 gate");
  Expect(evaluation.eta_zero_selection.success,
         "scoped weighted residue evaluator should reach the eta-zero selector");
  Expect(evaluation.eta_zero_selection.selected_coefficient_label ==
             "automatic_phasespace_D2_weighted_moment_seed",
         "scoped weighted residue evaluator should preserve the selected D2 label");
  Expect(evaluation.candidate_series.terms.size() == 3,
         "scoped weighted residue evaluator should carry the D2 K_2 candidate through eps^2");
  ExpectContains(ResidueTermAt(evaluation.candidate_series, 0).coefficient.real,
                 "-8.020298636472136866525611927436479798",
                 "scoped weighted residue evaluator should carry the reviewed K_2 seed");
  ExpectContains(evaluation.publication_gate_status,
                 "blocked-by-publication-gate",
                 "scoped weighted residue evaluator should preserve publication blocking");
  ExpectContains(evaluation.publication_gate_status,
                 "synthetic",
                 "scoped weighted residue evaluator should expose the synthetic blocker");

  ExpectExceptionContains(
      [&evaluation]() {
        amflow::ValidateCutkoskyResiduePublicationGate(
            evaluation.candidate_series);
      },
      "synthetic",
      "candidate series should be blocked by the existing publication validator");

  const std::string audit =
      amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(evaluation);
  ExpectContains(audit,
                 "kind=b63n-scoped-weighted-residue-evaluation",
                 "scoped weighted residue audit should identify the new evaluation");
  ExpectContains(audit,
                 "publication_gate_checked=true",
                 "scoped weighted residue audit should report publication validation");
  ExpectContains(audit,
                 "publication_gate_passed=false",
                 "scoped weighted residue audit should report the blocked gate");
  ExpectContains(audit,
                 "moment_cross_validation=passed",
                 "scoped weighted residue audit should report the existing moment gate");
  ExpectContains(audit,
                 "live_coefficients_available=false",
                 "scoped weighted residue audit must not claim live coefficients");
  ExpectContains(audit,
                 "retained_solution_samples_used=false",
                 "scoped weighted residue audit must reject retained final samples");
  ExpectContains(audit,
                 "full_eta_zero_contour_applied=false",
                 "scoped weighted residue audit must not promote M6");
}

void ScopedAutomaticPhaseSpaceWeightedResidueCanSelectD7Test() {
  const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          6,
          1,
          70);

  Expect(evaluation.selected_weight_denominator == "D7" &&
             evaluation.selected_weight_denominator_index == 6 &&
             evaluation.selected_weight_power == 1,
         "scoped weighted residue evaluator should select a requested reviewed weight");
  Expect(evaluation.eta_zero_selection.selected_coefficient_label ==
             "automatic_phasespace_D7_weighted_residue_eps0",
         "scoped weighted residue evaluator should preserve the published D7 label");
  Expect(evaluation.publication_gate_checked &&
             evaluation.publication_gate_passed &&
             evaluation.failure_code.empty(),
         "D7 scoped candidate should pass the publication gate");
  Expect(evaluation.live_coefficients_available,
         "D7 scoped candidate should publish the first reviewed weighted residue");
  Expect(!evaluation.retained_solution_samples_used,
         "D7 scoped candidate must not use retained final samples as runtime input");
  Expect(!evaluation.full_eta_zero_contour_applied,
         "D7 scoped candidate must not promote the full eta-zero contour");
  Expect(evaluation.reference_validation_passed &&
             evaluation.reference_min_digit_agreement == 999,
         "D7 scoped candidate should record the AMFlow reference validation");
  Expect(evaluation.candidate_series.terms.size() == 2,
         "D7 scoped candidate should publish the reviewed eps^0 and eps^1 terms");
  const amflow::CutkoskyResidueSeriesTerm& eps0 =
      ResidueTermAt(evaluation.candidate_series, 0);
  ExpectContains(eps0.coefficient.real,
                 "0.00003072064900647741498508445978252334878466335562820067",
                 "D7 scoped candidate should carry the reviewed AMFlow coefficient");
  const amflow::CutkoskyResidueSeriesTerm& eps1 =
      ResidueTermAt(evaluation.candidate_series, 1);
  Expect(eps1.coefficient_label ==
             "automatic_phasespace_D7_weighted_residue_eps1",
         "D7 scoped candidate should label the reviewed eps^1 coefficient");
  ExpectContains(eps1.coefficient.real,
                 "0.00007356405785821532462745545720829135530511062062212243",
                 "D7 scoped candidate should carry the reviewed eps^1 AMFlow coefficient");
  Expect(eps0.provenance.coefficient_published &&
             !eps0.provenance.synthetic_fixture &&
             !eps0.provenance.retained_solution_samples_used &&
             eps1.provenance.coefficient_published &&
             !eps1.provenance.synthetic_fixture &&
             !eps1.provenance.retained_solution_samples_used,
         "D7 scoped candidate should carry publishable non-synthetic provenance");
  amflow::ValidateCutkoskyResiduePublicationGate(evaluation.candidate_series);

  const std::string audit =
      amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(evaluation);
  ExpectContains(audit,
                 "publication_gate_passed=true",
                 "D7 scoped audit should report publication gate success");
  ExpectContains(audit,
                 "reference_validation_passed=true",
                 "D7 scoped audit should report reference validation");
  ExpectContains(audit,
                 "reference_min_digit_agreement=999",
                 "D7 scoped audit should report the stored AMFlow agreement");
  ExpectContains(audit,
                 "full_eta_zero_contour_applied=false",
                 "D7 scoped audit must not promote M6");
}

void ScopedAutomaticPhaseSpaceD246SolvedStateRequiresSidecarEvidenceTest() {
  struct ExpectedWeight {
    std::size_t denominator_index;
    std::string denominator_id;
  };
  const std::vector<ExpectedWeight> watched_weights = {
      {1, "D2"},
      {3, "D4"},
      {5, "D6"},
  };
  const bool has_sidecar_evidence =
      HasD246PublishedReferenceSidecarEvidence();

  for (const ExpectedWeight& expected : watched_weights) {
    const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
        amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
            MakeB63nAutomaticPhaseSpaceSpec(),
            expected.denominator_index,
            1,
            70);

    Expect(!IsSolvedScopedWeight(evaluation) || has_sidecar_evidence,
           expected.denominator_id +
               " scoped b63n weight was promoted to solved without the "
               "required AMFlow reference sidecar at " +
               kD246ReferenceEvidenceSidecar);
  }
}

void ScopedAutomaticPhaseSpaceWeightedResiduePinsRemainingWeightGapTest() {
  struct ExpectedWeightGap {
    std::size_t denominator_index;
    std::string denominator_id;
    int power;
  };
  const std::vector<ExpectedWeightGap> blocked_weights = {
      {1, "D2", 2},
      {3, "D4", 1},
      {5, "D6", 1},
  };

  for (const ExpectedWeightGap& expected : blocked_weights) {
    const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
        amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
            MakeB63nAutomaticPhaseSpaceSpec(),
            expected.denominator_index,
            1,
            70);

    Expect(evaluation.reviewed_surface,
           "blocked automatic weight should still bind the reviewed weighted surface");
    Expect(evaluation.selected_weight_denominator == expected.denominator_id &&
               evaluation.selected_weight_denominator_index ==
                   expected.denominator_index &&
               evaluation.selected_weight_power == expected.power,
           "scoped weighted residue evaluator should preserve the requested " +
               expected.denominator_id + " weight");
    Expect(evaluation.moment_cross_validation_gate.passed,
           expected.denominator_id +
               " scoped evaluation should pass the D2,D4,D6,D7 structural gate");
    Expect(evaluation.publication_gate_checked &&
               !evaluation.publication_gate_passed,
           expected.denominator_id +
               " scoped evaluation should stop at the publication gate");
    Expect(evaluation.failure_code == "boundary_unsolved",
           expected.denominator_id +
               " scoped evaluation should remain a boundary_unsolved gap");
    Expect(!evaluation.live_coefficients_available,
           expected.denominator_id +
               " scoped evaluation must not publish an unreviewed coefficient");
    Expect(!evaluation.retained_solution_samples_used,
           expected.denominator_id +
               " scoped evaluation must not read retained final samples");
    Expect(!evaluation.full_eta_zero_contour_applied,
           expected.denominator_id +
               " scoped evaluation must not promote full eta-zero coverage");
    ExpectContains(evaluation.publication_gate_status,
                   "blocked-by-publication-gate",
                   expected.denominator_id +
                       " scoped evaluation should expose publication blocking");
    ExpectContains(evaluation.publication_gate_status,
                   "synthetic",
                   expected.denominator_id +
                       " scoped evaluation should identify the synthetic blocker");
    Expect(evaluation.eta_zero_selection.success,
           expected.denominator_id +
               " scoped evaluation should still reach the eta-zero selector");
    Expect(evaluation.eta_zero_selection.selected_coefficient_label ==
               "automatic_phasespace_" + expected.denominator_id +
                   "_weighted_moment_seed",
           expected.denominator_id +
               " scoped evaluation should preserve the seed label");

    const std::string audit =
        amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(evaluation);
    ExpectContains(audit,
                   "selected_weight=" + expected.denominator_id,
                   expected.denominator_id +
                       " scoped audit should identify the selected weight");
    ExpectContains(audit,
                   "publication_gate_passed=false",
                   expected.denominator_id +
                       " scoped audit should report the blocked publication gate");
    ExpectContains(audit,
                   "failure_code=boundary_unsolved",
                   expected.denominator_id +
                       " scoped audit should report the remaining boundary gap");
    ExpectContains(audit,
                   "live_coefficients_available=false",
                   expected.denominator_id +
                       " scoped audit must not claim live coefficients");
  }

  const amflow::CutkoskyScopedWeightedResidueEvaluation d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          6,
          1,
          70);
  Expect(d7.publication_gate_passed && d7.live_coefficients_available,
         "D7 eps^0..eps^1 should remain the only reviewed published scoped weight");
  Expect(d7.reference_validation_passed,
         "D7 eps^0..eps^1 should remain tied to the stored AMFlow comparison");
  Expect(!d7.full_eta_zero_contour_applied,
         "the scoped D7 coefficient must not close the full weighted residue lane");
  ExpectContains(d7.summary,
                 "D2/D4/D6",
                 "D7 scoped summary should name the remaining automatic weight gap");
  ExpectContains(d7.summary,
                 "feynman_prescription",
                 "D7 scoped summary should preserve the companion b63n row gap");
}

void ScopedAutomaticPhaseSpaceWeightedResidueRejectsCutDenominatorTest() {
  ExpectExceptionContains(
      []() {
        static_cast<void>(
            amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
                MakeB63nAutomaticPhaseSpaceSpec(),
                0,
                1,
                70));
      },
      "reviewed uncut weight",
      "scoped weighted residue evaluator should reject cut denominator selection");
}

}  // namespace

int main() {
  try {
    ScopedAutomaticPhaseSpaceWeightedResidueStopsAtPublicationGateTest();
    ScopedAutomaticPhaseSpaceWeightedResidueCanSelectD7Test();
    ScopedAutomaticPhaseSpaceD246SolvedStateRequiresSidecarEvidenceTest();
    ScopedAutomaticPhaseSpaceWeightedResiduePinsRemainingWeightGapTest();
    ScopedAutomaticPhaseSpaceWeightedResidueRejectsCutDenominatorTest();
  } catch (const std::exception& error) {
    std::cerr << "cutkosky-weighted-residue-tests failed: " << error.what()
              << '\n';
    return 1;
  }
  return 0;
}
