#include <cstddef>
#include <exception>
#include <iostream>
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
             "automatic_phasespace_D7_weighted_moment_seed",
         "scoped weighted residue evaluator should preserve the D7 eta-zero label");
  Expect(!evaluation.publication_gate_passed &&
             evaluation.failure_code == "boundary_unsolved",
         "D7 scoped candidate should remain publication-gated");
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
    ScopedAutomaticPhaseSpaceWeightedResidueRejectsCutDenominatorTest();
  } catch (const std::exception& error) {
    std::cerr << "cutkosky-weighted-residue-tests failed: " << error.what()
              << '\n';
    return 1;
  }
  return 0;
}
