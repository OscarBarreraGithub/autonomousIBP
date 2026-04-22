#include "amflow/runtime/physical_kinematics_guardrails.hpp"

#include <cctype>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

namespace {

constexpr char kReviewedSubsetName[] = "k0_one_mass_2to2_real_v1";

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

bool MatchesExpectedExactNumericSubstitutions(
    const std::map<std::string, std::string>& substitutions) {
  if (substitutions.size() != 3 || substitutions.count("s") != 1 ||
      substitutions.count("t") != 1 || substitutions.count("msq") != 1) {
    return false;
  }

  for (const std::string& symbol : std::vector<std::string>{"s", "t", "msq"}) {
    const auto value_it = substitutions.find(symbol);
    try {
      static_cast<void>(EvaluateCoefficientExpression(value_it->second, NumericEvaluationPoint{}));
    } catch (const std::exception&) {
      return false;
    }
  }

  return true;
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
  if (!MatchesExpectedExactNumericSubstitutions(spec.kinematics.numeric_substitutions)) {
    assessment.verdict = PhysicalKinematicsGuardrailVerdict::UnsupportedSurface;
    return assessment;
  }

  assessment.verdict = PhysicalKinematicsGuardrailVerdict::SupportedReviewedSubset;
  return assessment;
}

}  // namespace amflow
