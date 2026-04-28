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

struct ExplicitLocationAssignment {
  std::string variable;
  std::string expression;
};

struct ReviewedInvariantSegment {
  ExactRational start;
  ExactRational target;
};

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(start, end - start);
}

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

ExactRational DivideRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "/" + Parenthesize(rhs));
}

bool IsNegative(const ExactRational& value) {
  return !value.IsZero() && !value.numerator.empty() && value.numerator.front() == '-';
}

bool IsPositive(const ExactRational& value) {
  return !value.IsZero() && !IsNegative(value);
}

ExactRational AbsoluteValue(const ExactRational& value) {
  return IsNegative(value) ? EvaluateExactArithmetic("-" + Parenthesize(value)) : value;
}

bool IsLessThan(const ExactRational& lhs, const ExactRational& rhs) {
  return IsNegative(SubtractRational(lhs, rhs));
}

bool IsLessThanOrEqual(const ExactRational& lhs, const ExactRational& rhs) {
  return !IsPositive(SubtractRational(lhs, rhs));
}

bool LiesOnClosedSegment(const ExactRational& point,
                         const ExactRational& start,
                         const ExactRational& target) {
  const ExactRational& lower = IsLessThan(start, target) ? start : target;
  const ExactRational& upper = IsLessThan(start, target) ? target : start;
  return IsLessThanOrEqual(lower, point) && IsLessThanOrEqual(point, upper);
}

ExactRational DistanceToClosedSegment(const ExactRational& point,
                                      const ExactRational& start,
                                      const ExactRational& target) {
  if (LiesOnClosedSegment(point, start, target)) {
    return IntegerRational(0);
  }

  const ExactRational start_distance = AbsoluteValue(SubtractRational(start, point));
  const ExactRational target_distance = AbsoluteValue(SubtractRational(target, point));
  return IsLessThan(start_distance, target_distance) ? start_distance : target_distance;
}

ReviewedInvariantSegment ExpandClosedSegmentByMargin(const ReviewedInvariantSegment& segment,
                                                     const ExactRational& margin) {
  const ExactRational& lower =
      IsLessThan(segment.start, segment.target) ? segment.start : segment.target;
  const ExactRational& upper =
      IsLessThan(segment.start, segment.target) ? segment.target : segment.start;
  return {SubtractRational(lower, margin), AddRational(upper, margin)};
}

std::optional<ExplicitLocationAssignment> ParseExplicitLocationAssignment(
    const std::string& location) {
  const std::string trimmed = Trim(location);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  const std::size_t separator = trimmed.find('=');
  if (separator == std::string::npos ||
      trimmed.find('=', separator + 1) != std::string::npos) {
    return std::nullopt;
  }

  ExplicitLocationAssignment assignment;
  assignment.variable = Trim(trimmed.substr(0, separator));
  assignment.expression = Trim(trimmed.substr(separator + 1));
  if (assignment.variable.empty() || assignment.expression.empty()) {
    return std::nullopt;
  }
  return assignment;
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

ExactRational ComputeReviewedEndpointPolynomial(const ExactRational& s,
                                                const ExactRational& t,
                                                const ExactRational& msq) {
  const ExactRational twice_msq = MultiplyRational(IntegerRational(2), msq);
  const ExactRational t_squared = MultiplyRational(t, t);
  const ExactRational mass_squared = MultiplyRational(msq, msq);
  return AddRational(
      SubtractRational(t_squared, MultiplyRational(SubtractRational(twice_msq, s), t)),
      mass_squared);
}

ExactRational ComputeReviewedEndpointPolynomial(
    const ReviewedK0ExactSubstitutions& substitutions) {
  return ComputeReviewedEndpointPolynomial(substitutions.s,
                                           substitutions.t,
                                           substitutions.msq);
}

NumericEvaluationPoint BuildReviewedSegmentPassiveBindings(
    const ReviewedK0ExactSubstitutions& substitutions,
    const std::string& active_variable) {
  NumericEvaluationPoint passive_bindings;
  if (active_variable != "s") {
    passive_bindings.emplace("s", substitutions.s.ToString());
  }
  if (active_variable != "t") {
    passive_bindings.emplace("t", substitutions.t.ToString());
  }
  if (active_variable != "msq") {
    passive_bindings.emplace("msq", substitutions.msq.ToString());
  }
  return passive_bindings;
}

std::optional<ReviewedInvariantSegment> ParseReviewedInvariantSegment(
    const std::string& start_location,
    const std::string& target_location,
    const ReviewedK0ExactSubstitutions& substitutions,
    const std::string& active_variable,
    const bool allow_unlabeled_reviewed_raw_expressions) {
  const auto parse_location =
      [&substitutions, &active_variable, allow_unlabeled_reviewed_raw_expressions](
          const std::string& location) -> std::optional<ExactRational> {
    const NumericEvaluationPoint passive_bindings =
        BuildReviewedSegmentPassiveBindings(substitutions, active_variable);
    const std::optional<ExplicitLocationAssignment> assignment =
        ParseExplicitLocationAssignment(location);
    if (assignment.has_value()) {
      if (assignment->variable != active_variable) {
        return std::nullopt;
      }
      try {
        return EvaluateCoefficientExpression(assignment->expression, passive_bindings);
      } catch (const std::exception&) {
        return std::nullopt;
      }
    }

    if (!allow_unlabeled_reviewed_raw_expressions ||
        (active_variable != "s" && active_variable != "t" && active_variable != "msq")) {
      return std::nullopt;
    }

    const std::string trimmed = Trim(location);
    if (trimmed.empty() || trimmed.find('=') != std::string::npos) {
      return std::nullopt;
    }

    try {
      return EvaluateCoefficientExpression(trimmed, passive_bindings);
    } catch (const std::exception&) {
      return std::nullopt;
    }
  };

  const std::optional<ExactRational> start_value = parse_location(start_location);
  const std::optional<ExactRational> target_value = parse_location(target_location);
  if (!start_value.has_value() || !target_value.has_value()) {
    return std::nullopt;
  }

  ReviewedInvariantSegment segment;
  segment.start = *start_value;
  segment.target = *target_value;
  return segment;
}

bool IsUnlabeledLocationWithoutAssignment(const std::string& location) {
  const std::string trimmed = Trim(location);
  return !trimmed.empty() && trimmed.find('=') == std::string::npos;
}

bool HasAmbiguousUnlabeledReviewedSMultiInvariantLocation(
    const std::string& start_location,
    const std::string& target_location,
    const ReviewedK0ExactSubstitutions&) {
  return IsUnlabeledLocationWithoutAssignment(start_location) ||
         IsUnlabeledLocationWithoutAssignment(target_location);
}

bool HaveNonExplicitReviewedTMultiInvariantLocations(
    const std::string& start_location,
    const std::string& target_location) {
  return IsUnlabeledLocationWithoutAssignment(start_location) &&
         IsUnlabeledLocationWithoutAssignment(target_location);
}

bool HaveOppositeSigns(const ExactRational& lhs, const ExactRational& rhs) {
  return (IsNegative(lhs) && IsPositive(rhs)) ||
         (IsPositive(lhs) && IsNegative(rhs));
}

ExactRational ComputeReviewedEndpointVertexT(
    const ReviewedK0ExactSubstitutions& substitutions) {
  return DivideRational(
      SubtractRational(MultiplyRational(IntegerRational(2), substitutions.msq),
                       substitutions.s),
      IntegerRational(2));
}

bool TSegmentCrossesReviewedEndpointSurface(
    const ReviewedInvariantSegment& segment,
    const ReviewedK0ExactSubstitutions& substitutions) {
  const ExactRational start_polynomial =
      ComputeReviewedEndpointPolynomial(substitutions.s, segment.start, substitutions.msq);
  const ExactRational target_polynomial =
      ComputeReviewedEndpointPolynomial(substitutions.s, segment.target, substitutions.msq);
  if (start_polynomial.IsZero() || target_polynomial.IsZero() ||
      HaveOppositeSigns(start_polynomial, target_polynomial)) {
    return true;
  }
  if (!IsPositive(start_polynomial) || !IsPositive(target_polynomial)) {
    return false;
  }

  const ExactRational vertex_t = ComputeReviewedEndpointVertexT(substitutions);
  if (!LiesOnClosedSegment(vertex_t, segment.start, segment.target)) {
    return false;
  }

  const ExactRational vertex_polynomial =
      ComputeReviewedEndpointPolynomial(substitutions.s, vertex_t, substitutions.msq);
  return !IsPositive(vertex_polynomial);
}

ExactRational ComputeReviewedThresholdSingularMsq(
    const ReviewedK0ExactSubstitutions& substitutions) {
  return DivideRational(substitutions.s, IntegerRational(4));
}

bool MsqSegmentCrossesReviewedEndpointSurface(
    const ReviewedInvariantSegment& segment,
    const ReviewedK0ExactSubstitutions& substitutions) {
  const ExactRational start_polynomial =
      ComputeReviewedEndpointPolynomial(substitutions.s, substitutions.t, segment.start);
  const ExactRational target_polynomial =
      ComputeReviewedEndpointPolynomial(substitutions.s, substitutions.t, segment.target);
  if (start_polynomial.IsZero() || target_polynomial.IsZero() ||
      HaveOppositeSigns(start_polynomial, target_polynomial)) {
    return true;
  }
  if (!IsPositive(start_polynomial) || !IsPositive(target_polynomial)) {
    return false;
  }
  if (!LiesOnClosedSegment(substitutions.t, segment.start, segment.target)) {
    return false;
  }

  const ExactRational vertex_polynomial =
      ComputeReviewedEndpointPolynomial(substitutions.s, substitutions.t, substitutions.t);
  return !IsPositive(vertex_polynomial);
}

std::optional<ExactRational> ComputeReviewedEndpointSingularS(
    const ReviewedK0ExactSubstitutions& substitutions) {
  if (substitutions.t.IsZero()) {
    return std::nullopt;
  }

  const ExactRational t_squared =
      MultiplyRational(substitutions.t, substitutions.t);
  const ExactRational mass_squared =
      MultiplyRational(substitutions.msq, substitutions.msq);
  return SubtractRational(
      MultiplyRational(IntegerRational(2), substitutions.msq),
      DivideRational(AddRational(t_squared, mass_squared), substitutions.t));
}

ExactRational ComputeReviewedNearSingularMargin(
    const ReviewedK0ExactSubstitutions& substitutions) {
  return DivideRational(substitutions.msq, IntegerRational(4));
}

std::string DescribeReviewedClosedRealSSegment(const ExactRational& start,
                                               const ExactRational& target) {
  return "requested closed real s segment [" + start.ToString() + ", " +
         target.ToString() + "]";
}

std::string DescribeReviewedClosedRealTSegment(const ExactRational& start,
                                               const ExactRational& target) {
  return "requested closed real t segment [" + start.ToString() + ", " +
         target.ToString() + "]";
}

std::string DescribeReviewedClosedRealMsqSegment(const ExactRational& start,
                                                 const ExactRational& target) {
  return "requested closed real msq segment [" + start.ToString() + ", " +
         target.ToString() + "]";
}

std::string DescribeReviewedNearSingularMargin(const ExactRational& margin) {
  return "conservative reviewed near-singular margin of width " + margin.ToString();
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

PhysicalKinematicsGuardrailAssessment
AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
    const ProblemSpec& spec,
    const std::string& invariant_name,
    const std::string& start_location,
    const std::string& target_location,
    const bool allow_unlabeled_reviewed_raw_expressions) {
  PhysicalKinematicsGuardrailAssessment assessment =
      AssessPhysicalKinematicsForBatch62(spec);
  if (assessment.verdict !=
      PhysicalKinematicsGuardrailVerdict::SupportedReviewedSubset) {
    return assessment;
  }

  const std::optional<ReviewedK0ExactSubstitutions> exact_substitutions =
      LoadReviewedExactNumericSubstitutions(spec.kinematics.numeric_substitutions);
  if (!exact_substitutions.has_value()) {
    return assessment;
  }

  if (invariant_name != "s" && invariant_name != "t" && invariant_name != "msq") {
    return assessment;
  }

  const std::optional<ReviewedInvariantSegment> segment =
      ParseReviewedInvariantSegment(start_location,
                                    target_location,
                                    *exact_substitutions,
                                    invariant_name,
                                    allow_unlabeled_reviewed_raw_expressions);
  if (!segment.has_value()) {
    if (invariant_name == "s" && !allow_unlabeled_reviewed_raw_expressions &&
        HasAmbiguousUnlabeledReviewedSMultiInvariantLocation(start_location,
                                                            target_location,
                                                            *exact_substitutions)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
      assessment.detail =
          "ambiguous unlabeled continuation locations remain unsupported when s participates in "
          "a reviewed multi-invariant solve request; spell the reviewed s segment explicitly as "
          "s=...";
    } else if (invariant_name == "t" && !allow_unlabeled_reviewed_raw_expressions &&
               HaveNonExplicitReviewedTMultiInvariantLocations(start_location,
                                                               target_location)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
      assessment.detail =
          "non-explicit continuation locations remain unsupported on the reviewed multi-invariant "
          "t surface; spell the reviewed t segment explicitly as t=...";
    }
    return assessment;
  }

  if (invariant_name == "t") {
    if (TSegmentCrossesReviewedEndpointSurface(*segment, *exact_substitutions)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
      assessment.detail =
          DescribeReviewedClosedRealTSegment(segment->start, segment->target) +
          " crosses the reviewed 2->2 endpoint polynomial "
          "t^2 - (2*msq - s)*t + msq^2 = 0";
      return assessment;
    }
    const ExactRational near_singular_margin =
        ComputeReviewedNearSingularMargin(*exact_substitutions);
    if (TSegmentCrossesReviewedEndpointSurface(
            ExpandClosedSegmentByMargin(*segment, near_singular_margin),
            *exact_substitutions)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::NearSingularSurface;
      assessment.detail =
          DescribeReviewedClosedRealTSegment(segment->start, segment->target) +
          " enters the " + DescribeReviewedNearSingularMargin(near_singular_margin) +
          " around the reviewed 2->2 endpoint polynomial "
          "t^2 - (2*msq - s)*t + msq^2 = 0";
    }
    return assessment;
  }

  if (invariant_name == "msq") {
    const ExactRational threshold_msq =
        ComputeReviewedThresholdSingularMsq(*exact_substitutions);
    if (LiesOnClosedSegment(threshold_msq, segment->start, segment->target)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
      assessment.detail =
          DescribeReviewedClosedRealMsqSegment(segment->start, segment->target) +
          " crosses the reviewed pair-production threshold s = 4*msq";
      return assessment;
    }
    if (MsqSegmentCrossesReviewedEndpointSurface(*segment, *exact_substitutions)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
      assessment.detail =
          DescribeReviewedClosedRealMsqSegment(segment->start, segment->target) +
          " crosses the reviewed 2->2 endpoint polynomial "
          "t^2 - (2*msq - s)*t + msq^2 = 0";
      return assessment;
    }

    const ExactRational near_singular_margin =
        ComputeReviewedNearSingularMargin(*exact_substitutions);
    if (IsLessThanOrEqual(DistanceToClosedSegment(threshold_msq,
                                                  segment->start,
                                                  segment->target),
                          near_singular_margin)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::NearSingularSurface;
      assessment.detail =
          DescribeReviewedClosedRealMsqSegment(segment->start, segment->target) +
          " enters the " + DescribeReviewedNearSingularMargin(near_singular_margin) +
          " around the reviewed pair-production threshold s = 4*msq";
      return assessment;
    }
    if (MsqSegmentCrossesReviewedEndpointSurface(
            ExpandClosedSegmentByMargin(*segment, near_singular_margin),
            *exact_substitutions)) {
      assessment.verdict = PhysicalKinematicsGuardrailVerdict::NearSingularSurface;
      assessment.detail =
          DescribeReviewedClosedRealMsqSegment(segment->start, segment->target) +
          " enters the " + DescribeReviewedNearSingularMargin(near_singular_margin) +
          " around the reviewed 2->2 endpoint polynomial "
          "t^2 - (2*msq - s)*t + msq^2 = 0";
    }
    return assessment;
  }

  const ExactRational threshold_s =
      MultiplyRational(IntegerRational(4), exact_substitutions->msq);
  if (LiesOnClosedSegment(threshold_s, segment->start, segment->target)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
    assessment.detail =
        DescribeReviewedClosedRealSSegment(segment->start, segment->target) +
        " crosses the reviewed pair-production threshold s = 4*msq";
    return assessment;
  }

  const std::optional<ExactRational> endpoint_s =
      ComputeReviewedEndpointSingularS(*exact_substitutions);
  if (endpoint_s.has_value() &&
      LiesOnClosedSegment(*endpoint_s, segment->start, segment->target)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::SingularSurface;
    assessment.detail =
        DescribeReviewedClosedRealSSegment(segment->start, segment->target) +
        " crosses the reviewed 2->2 endpoint polynomial "
        "t^2 - (2*msq - s)*t + msq^2 = 0";
    return assessment;
  }

  const ExactRational near_singular_margin =
      ComputeReviewedNearSingularMargin(*exact_substitutions);
  if (IsLessThanOrEqual(DistanceToClosedSegment(threshold_s, segment->start, segment->target),
                        near_singular_margin)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::NearSingularSurface;
    assessment.detail =
        DescribeReviewedClosedRealSSegment(segment->start, segment->target) +
        " enters the " + DescribeReviewedNearSingularMargin(near_singular_margin) +
        " around the reviewed pair-production threshold s = 4*msq";
    return assessment;
  }
  if (endpoint_s.has_value() &&
      IsLessThanOrEqual(
          DistanceToClosedSegment(*endpoint_s, segment->start, segment->target),
          near_singular_margin)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::NearSingularSurface;
    assessment.detail =
        DescribeReviewedClosedRealSSegment(segment->start, segment->target) +
        " enters the " + DescribeReviewedNearSingularMargin(near_singular_margin) +
        " around the reviewed 2->2 endpoint polynomial "
        "t^2 - (2*msq - s)*t + msq^2 = 0";
    return assessment;
  }
  return assessment;
}

PhysicalKinematicsGuardrailAssessment
AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
    const ProblemSpec& spec,
    const std::string& start_location,
    const std::string& target_location,
    const bool allow_unlabeled_reviewed_raw_expressions) {
  return AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
      spec,
      "s",
      start_location,
      target_location,
      allow_unlabeled_reviewed_raw_expressions);
}

PhysicalKinematicsGuardrailAssessment
AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
    const ProblemSpec& spec,
    const std::string& start_location,
    const std::string& target_location) {
  return AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
      spec, "s", start_location, target_location, true);
}

}  // namespace amflow
