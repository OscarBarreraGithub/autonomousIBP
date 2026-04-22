#include "amflow/runtime/physical_kinematics_guardrails.hpp"

#include <cctype>
#include <exception>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

namespace {

constexpr char kReviewedSubsetName[] = "k0_one_mass_2to2_real_v1";

struct ReviewedK0ExactSubstitutions {
  ExactRational s;
  ExactRational t;
  ExactRational msq;
};

std::string RemoveWhitespace(const std::string& value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (const char character : value) {
    if (!std::isspace(static_cast<unsigned char>(character))) {
      normalized.push_back(character);
    }
  }
  return normalized;
}

ExactRational IntegerRational(const int value) {
  return {std::to_string(value), "1"};
}

std::string Parenthesize(const ExactRational& value) {
  return "(" + value.ToString() + ")";
}

ExactRational EvaluateExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, NumericEvaluationPoint{});
}

ExactRational AddRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "+" + Parenthesize(rhs));
}

ExactRational SubtractRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "-" + Parenthesize(rhs));
}

ExactRational MultiplyRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "*" + Parenthesize(rhs));
}

bool IsNegative(const ExactRational& value) {
  return !value.IsZero() && !value.numerator.empty() && value.numerator.front() == '-';
}

bool IsPositive(const ExactRational& value) {
  return !value.IsZero() && !IsNegative(value);
}

bool MatchesExpectedMomentumLists(const ProblemSpec& spec) {
  return spec.kinematics.incoming_momenta == std::vector<std::string>{"p1", "p2"} &&
         spec.kinematics.outgoing_momenta == std::vector<std::string>{"p3", "p4"};
}

bool MatchesExpectedInvariantNamesAndMomentumConservation(const ProblemSpec& spec) {
  return RemoveWhitespace(spec.kinematics.momentum_conservation) == "p1+p2+p3+p4=0" &&
         spec.kinematics.invariants == std::vector<std::string>{"s", "t", "msq"};
}

bool MatchesK0CandidateShape(const ProblemSpec& spec) {
  if (spec.family.loop_momenta != std::vector<std::string>{"k1", "k2"} ||
      spec.family.top_level_sectors != std::vector<int>{127} ||
      spec.family.propagators.size() != 9 || spec.targets.empty()) {
    return false;
  }

  for (const auto& target : spec.targets) {
    if (target.indices.size() != 9) {
      return false;
    }
  }
  return true;
}

bool SameReviewedPropagator(const Propagator& actual, const Propagator& expected) {
  return RemoveWhitespace(actual.expression) == RemoveWhitespace(expected.expression) &&
         RemoveWhitespace(actual.mass) == RemoveWhitespace(expected.mass) &&
         actual.kind == expected.kind && actual.prescription == expected.prescription;
}

bool MatchesReviewedK0Propagators(const std::vector<Propagator>& propagators) {
  static const std::vector<Propagator> expected = {
      {"(k1)^2", "0", PropagatorKind::Standard, -1},
      {"(k1+p1)^2", "0", PropagatorKind::Standard, -1},
      {"(k1+p1+p2)^2", "0", PropagatorKind::Standard, -1},
      {"(k2)^2", "0", PropagatorKind::Standard, -1},
      {"(k2+p3)^2", "msq", PropagatorKind::Standard, -1},
      {"(k2+p3+p4)^2", "0", PropagatorKind::Standard, -1},
      {"(k1+k2)^2", "0", PropagatorKind::Standard, -1},
      {"(k1-p3)^2", "0", PropagatorKind::Standard, -1},
      {"(k2+p1)^2", "0", PropagatorKind::Standard, -1},
  };

  if (propagators.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (!SameReviewedPropagator(propagators[index], expected[index])) {
      return false;
    }
  }
  return true;
}

bool MatchesExpectedScalarProductRules(const std::vector<ScalarProductRule>& rules) {
  if (rules.size() != 6) {
    return false;
  }

  const std::set<std::string> expected = {
      "p1*p1=0",
      "p2*p2=0",
      "p3*p3=msq",
      "p1*p2=s/2",
      "p1*p3=(t-msq)/2",
      "p2*p3=(msq-s-t)/2",
  };

  std::set<std::string> actual;
  for (const auto& rule : rules) {
    actual.insert(RemoveWhitespace(rule.left) + "=" + RemoveWhitespace(rule.right));
  }
  return actual == expected;
}

std::optional<ReviewedK0ExactSubstitutions> LoadReviewedExactNumericSubstitutions(
    const std::map<std::string, std::string>& substitutions) {
  if (substitutions.size() != 3 || substitutions.count("s") != 1 ||
      substitutions.count("t") != 1 || substitutions.count("msq") != 1) {
    return std::nullopt;
  }

  ReviewedK0ExactSubstitutions exact_substitutions;
  try {
    exact_substitutions.s =
        EvaluateCoefficientExpression(substitutions.at("s"), NumericEvaluationPoint{});
    exact_substitutions.t =
        EvaluateCoefficientExpression(substitutions.at("t"), NumericEvaluationPoint{});
    exact_substitutions.msq =
        EvaluateCoefficientExpression(substitutions.at("msq"), NumericEvaluationPoint{});
  } catch (const std::exception&) {
    return std::nullopt;
  }
  return exact_substitutions;
}

ExactRational ComputeReviewedEndpointPolynomial(const ReviewedK0ExactSubstitutions& substitutions) {
  const ExactRational twice_msq =
      MultiplyRational(IntegerRational(2), substitutions.msq);
  const ExactRational t_squared =
      MultiplyRational(substitutions.t, substitutions.t);
  const ExactRational mass_squared =
      MultiplyRational(substitutions.msq, substitutions.msq);
  return AddRational(
      SubtractRational(
          t_squared,
          MultiplyRational(SubtractRational(twice_msq, substitutions.s), substitutions.t)),
      mass_squared);
}

}  // namespace

std::string DescribeReviewedPhysicalKinematicsSubset() {
  return kReviewedSubsetName;
}

PhysicalKinematicsGuardrailAssessment AssessPhysicalKinematicsForBatch62(
    const ProblemSpec& spec) {
  PhysicalKinematicsGuardrailAssessment assessment;
  assessment.reviewed_subset = DescribeReviewedPhysicalKinematicsSubset();

  if (!MatchesExpectedMomentumLists(spec) ||
      !MatchesExpectedInvariantNamesAndMomentumConservation(spec) ||
      !MatchesK0CandidateShape(spec)) {
    return assessment;
  }
  if (spec.complex_mode || !spec.kinematics.complex_numeric_substitutions.empty()) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    return assessment;
  }
  if (!MatchesReviewedK0Propagators(spec.family.propagators)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    return assessment;
  }
  if (!MatchesExpectedScalarProductRules(spec.kinematics.scalar_product_rules)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    return assessment;
  }
  const std::optional<ReviewedK0ExactSubstitutions> exact_substitutions =
      LoadReviewedExactNumericSubstitutions(spec.kinematics.numeric_substitutions);
  if (!exact_substitutions.has_value()) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    assessment.detail =
        "exact numeric substitutions must provide exact real bindings for s, t, and msq";
    return assessment;
  }

  if (!IsPositive(exact_substitutions->msq)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    assessment.detail = "the reviewed open physical region requires msq > 0";
    return assessment;
  }

  const ExactRational threshold_gap =
      SubtractRational(exact_substitutions->s,
                       MultiplyRational(IntegerRational(4), exact_substitutions->msq));
  const ExactRational endpoint_polynomial =
      ComputeReviewedEndpointPolynomial(*exact_substitutions);
  if (threshold_gap.IsZero() && endpoint_polynomial.IsZero()) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
    assessment.detail = "exact bindings hit the reviewed pair-production threshold s = 4*msq";
    return assessment;
  }
  if (!IsPositive(threshold_gap)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    assessment.detail = "exact bindings lie outside the reviewed open real 2->2 physical region";
    return assessment;
  }

  // On the reviewed equal-mass 2->2 subset, the open physical interval is the interior of the
  // exact endpoint polynomial t^2 - (2*msq - s)*t + msq^2.
  if (endpoint_polynomial.IsZero()) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
    assessment.detail =
        "exact bindings hit the reviewed 2->2 endpoint polynomial "
        "t^2 - (2*msq - s)*t + msq^2 = 0";
    return assessment;
  }
  if (!IsNegative(endpoint_polynomial)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    assessment.detail = "exact bindings lie outside the reviewed open real 2->2 physical region";
    return assessment;
  }

  assessment.verdict = PhysicalKinematicsGuardrailVerdict::SupportedReviewedSubset;
  return assessment;
}

}  // namespace amflow
