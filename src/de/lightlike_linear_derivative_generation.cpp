#include "amflow/de/lightlike_linear_derivative_generation.hpp"

#include "amflow/de/invariant_derivative_generation.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"

#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace amflow {

namespace {

bool ContainsStandaloneIdentifier(const std::string& value, const std::string& identifier) {
  if (identifier.empty()) {
    return false;
  }

  for (std::size_t index = 0; index + identifier.size() <= value.size(); ++index) {
    if (value.compare(index, identifier.size(), identifier) != 0) {
      continue;
    }

    const bool has_left_identifier =
        index > 0 &&
        (std::isalnum(static_cast<unsigned char>(value[index - 1])) != 0 ||
         value[index - 1] == '_');
    const std::size_t end = index + identifier.size();
    const bool has_right_identifier =
        end < value.size() &&
        (std::isalnum(static_cast<unsigned char>(value[end])) != 0 || value[end] == '_');
    if (!has_left_identifier && !has_right_identifier) {
      return true;
    }
  }
  return false;
}

std::string StripOneOuterParenthesisPair(const std::string& value) {
  if (value.size() < 2 || value.front() != '(' || value.back() != ')') {
    return value;
  }

  int depth = 0;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char ch = value[index];
    if (ch == '(') {
      ++depth;
    } else if (ch == ')') {
      --depth;
      if (depth == 0 && index + 1 != value.size()) {
        return value;
      }
    }
  }
  return value.substr(1, value.size() - 2);
}

std::string TrimWhitespace(const std::string& value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::optional<std::size_t> FindFirstTopLevelMultiplication(const std::string& value) {
  int depth = 0;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char ch = value[index];
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch == ')') {
      --depth;
      continue;
    }
    if (ch == '*' && depth == 0) {
      return index;
    }
  }
  return std::nullopt;
}

std::vector<std::string> SplitTopLevelAdditiveTerms(const std::string& value) {
  std::vector<std::string> terms;
  std::size_t begin = 0;
  int depth = 0;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char ch = value[index];
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch == ')') {
      --depth;
      continue;
    }
    if (depth == 0 && (ch == '+' || ch == '-') && index != begin) {
      terms.push_back(value.substr(begin, index - begin));
      begin = index;
    }
  }
  terms.push_back(value.substr(begin));
  return terms;
}

std::string AbsoluteRationalString(const ExactRational& value) {
  std::string numerator = value.numerator;
  if (!numerator.empty() && numerator.front() == '-') {
    numerator.erase(numerator.begin());
  }
  if (value.denominator == "1") {
    return numerator;
  }
  return numerator + "/" + value.denominator;
}

bool IsNegativeRational(const ExactRational& value) {
  return !value.numerator.empty() && value.numerator.front() == '-';
}

struct AdditiveDriverTerm {
  std::string magnitude;
  bool negative = false;
  std::string body;
};

std::optional<AdditiveDriverTerm> ParseCommonCoefficientAdditiveDriverTerm(
    const std::string& raw_term) {
  std::string term = TrimWhitespace(raw_term);
  if (term.empty()) {
    return std::nullopt;
  }

  bool leading_negative = false;
  if (term.front() == '+' || term.front() == '-') {
    leading_negative = term.front() == '-';
    term = TrimWhitespace(term.substr(1));
  }

  const std::optional<std::size_t> split = FindFirstTopLevelMultiplication(term);
  if (!split.has_value()) {
    return std::nullopt;
  }

  const std::string coefficient = TrimWhitespace(term.substr(0, *split));
  const std::string body = TrimWhitespace(term.substr(*split + 1));
  if (coefficient.empty() || body.empty()) {
    return std::nullopt;
  }

  try {
    const ExactRational signed_coefficient = EvaluateCoefficientExpression(
        leading_negative ? "(-1)*(" + coefficient + ")" : coefficient,
        NumericEvaluationPoint{});
    if (signed_coefficient.IsZero()) {
      return std::nullopt;
    }
    return AdditiveDriverTerm{
        AbsoluteRationalString(signed_coefficient),
        IsNegativeRational(signed_coefficient),
        body,
    };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<std::string> NormalizeCommonCoefficientAdditiveQuadraticDriver(
    const std::string& inner) {
  const std::vector<std::string> raw_terms = SplitTopLevelAdditiveTerms(inner);
  if (raw_terms.size() < 2) {
    return std::nullopt;
  }

  std::vector<AdditiveDriverTerm> terms;
  terms.reserve(raw_terms.size());
  std::string common_magnitude;
  for (const std::string& raw_term : raw_terms) {
    const std::optional<AdditiveDriverTerm> term =
        ParseCommonCoefficientAdditiveDriverTerm(raw_term);
    if (!term.has_value()) {
      return std::nullopt;
    }
    if (common_magnitude.empty()) {
      common_magnitude = term->magnitude;
    } else if (term->magnitude != common_magnitude) {
      return std::nullopt;
    }
    terms.push_back(*term);
  }

  std::string normalized_inner;
  for (std::size_t index = 0; index < terms.size(); ++index) {
    const AdditiveDriverTerm& term = terms[index];
    if (index == 0) {
      if (term.negative) {
        normalized_inner += "-";
      }
    } else {
      normalized_inner += term.negative ? "-" : "+";
    }
    normalized_inner += term.body;
  }
  if (normalized_inner.empty()) {
    return std::nullopt;
  }

  try {
    const std::string squared_coefficient =
        EvaluateCoefficientExpression("(" + common_magnitude + ")*(" + common_magnitude + ")",
                                      NumericEvaluationPoint{})
            .ToString();
    if (squared_coefficient == "1") {
      return "(" + normalized_inner + ")^2";
    }
    return "(" + squared_coefficient + ")*((" + normalized_inner + ")^2)";
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::string ExtractReviewedLightlikeQuadraticDriver(
    const LightlikeLinearAuxiliaryTransformResult& auxiliary_family,
    const std::string& x_symbol) {
  const std::string& rewritten_expression =
      auxiliary_family.transformed_spec.family
          .propagators[auxiliary_family.rewritten_propagator_index]
          .expression;
  const std::string prefix = x_symbol + "*(";
  if (rewritten_expression.compare(0, prefix.size(), prefix) != 0) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary derivative generation requires rewritten "
        "propagator expression to start with \"" +
        prefix + "\"");
  }

  const std::size_t driver_begin = prefix.size();
  std::size_t driver_end = std::string::npos;
  int depth = 1;
  for (std::size_t index = driver_begin; index < rewritten_expression.size(); ++index) {
    const char ch = rewritten_expression[index];
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch != ')') {
      continue;
    }

    --depth;
    if (depth == 0) {
      driver_end = index;
      break;
    }
  }

  if (driver_end == std::string::npos ||
      rewritten_expression.compare(driver_end + 1, 4, " + (") != 0) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary derivative generation encountered malformed "
        "rewritten propagator expression at index " +
        std::to_string(auxiliary_family.rewritten_propagator_index));
  }

  return rewritten_expression.substr(driver_begin, driver_end - driver_begin);
}

std::string NormalizeReviewedLightlikeQuadraticDriverForSeed(const std::string& driver_expression) {
  if (driver_expression.size() < 4 || driver_expression.front() != '(' ||
      driver_expression.compare(driver_expression.size() - 3, 3, ")^2") != 0) {
    return driver_expression;
  }

  const std::string inner = driver_expression.substr(1, driver_expression.size() - 4);
  int additive_depth = 0;
  for (std::size_t index = 0; index < inner.size(); ++index) {
    const char ch = inner[index];
    if (ch == '(') {
      ++additive_depth;
      continue;
    }
    if (ch == ')') {
      --additive_depth;
      continue;
    }
    if (additive_depth == 0 && (ch == '+' || ch == '-')) {
      if (const std::optional<std::string> normalized =
              NormalizeCommonCoefficientAdditiveQuadraticDriver(inner);
          normalized.has_value()) {
        return *normalized;
      }
      return driver_expression;
    }
  }

  std::size_t split = std::string::npos;
  int depth = 0;
  for (std::size_t index = 0; index < inner.size(); ++index) {
    const char ch = inner[index];
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch == ')') {
      --depth;
      continue;
    }
    if (ch == '*' && depth == 0) {
      split = index;
      break;
    }
  }

  if (split == std::string::npos) {
    return driver_expression;
  }

  const std::string coefficient = inner.substr(0, split);
  const std::string base = inner.substr(split + 1);
  if (coefficient.empty() || base.empty()) {
    return driver_expression;
  }

  try {
    const std::string squared_coefficient =
        EvaluateCoefficientExpression("(" + coefficient + ")*(" + coefficient + ")",
                                      NumericEvaluationPoint{})
            .ToString();
    if (squared_coefficient == "1") {
      return "(" + StripOneOuterParenthesisPair(base) + ")^2";
    }
    return "(" + squared_coefficient + ")*((" + StripOneOuterParenthesisPair(base) + ")^2)";
  } catch (const std::exception&) {
    return driver_expression;
  }
}

}  // namespace

GeneratedDerivativeVariable GenerateReviewedLightlikeLinearAuxiliaryDerivativeVariable(
    const ParsedMasterList& master_basis,
    const LightlikeLinearAuxiliaryTransformResult& auxiliary_family) {
  const std::string family_name = auxiliary_family.transformed_spec.family.name;
  const std::size_t propagator_count =
      auxiliary_family.transformed_spec.family.propagators.size();

  if (master_basis.family != family_name) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary derivative generation requires "
        "ParsedMasterList.family \"" +
        family_name + "\", found \"" + master_basis.family + "\"");
  }

  if (auxiliary_family.rewritten_propagator_index >= propagator_count) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary derivative generation encountered out-of-range "
        "rewritten propagator index: " +
        std::to_string(auxiliary_family.rewritten_propagator_index));
  }

  for (const auto& master : master_basis.masters) {
    if (master.family != family_name) {
      throw std::runtime_error(
          "reviewed lightlike linear auxiliary derivative generation requires master family \"" +
          family_name + "\", found \"" + master.family + "\"");
    }
    if (master.indices.size() != propagator_count) {
      throw std::runtime_error(
          "reviewed lightlike linear auxiliary derivative generation requires master index count "
          "to match the transformed family propagator count");
    }
  }

  const std::string x_symbol = auxiliary_family.x_symbol.empty() ? "x" : auxiliary_family.x_symbol;
  const std::string injected_prefix = x_symbol + "*(";
  const std::string& rewritten_expression =
      auxiliary_family.transformed_spec.family
          .propagators[auxiliary_family.rewritten_propagator_index]
          .expression;
  if (rewritten_expression.compare(0, injected_prefix.size(), injected_prefix) == 0 &&
      ContainsStandaloneIdentifier(rewritten_expression.substr(injected_prefix.size()), x_symbol)) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary derivative generation requires the chosen x "
        "symbol to remain exclusive to the injected rewritten x prefix across the transformed "
        "family");
  }
  for (std::size_t index = 0; index < auxiliary_family.transformed_spec.family.propagators.size();
       ++index) {
    if (index == auxiliary_family.rewritten_propagator_index) {
      continue;
    }
    if (ContainsStandaloneIdentifier(
            auxiliary_family.transformed_spec.family.propagators[index].expression, x_symbol)) {
      throw std::runtime_error(
          "reviewed lightlike linear auxiliary derivative generation requires the chosen x "
          "symbol to remain exclusive to the injected rewritten x prefix across the transformed "
          "family");
    }
  }

  ProblemSpec seed_spec = auxiliary_family.transformed_spec;
  seed_spec.family.propagators[auxiliary_family.rewritten_propagator_index].expression =
      x_symbol + "*(" +
      NormalizeReviewedLightlikeQuadraticDriverForSeed(
          ExtractReviewedLightlikeQuadraticDriver(auxiliary_family, x_symbol)) +
      ")";
  InvariantDerivativeSeed seed;
  try {
    seed = BuildInvariantDerivativeSeed(seed_spec, x_symbol);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    if (message.find("could not represent derivative term using known propagator factors:") !=
        std::string::npos) {
      throw std::runtime_error(
          "reviewed lightlike linear auxiliary derivative generation requires the extracted "
          "quadratic driver to match an existing transformed-family propagator factor");
    }
    throw;
  }
  GeneratedDerivativeVariable generated_variable =
      GenerateInvariantDerivativeVariable(master_basis, seed);
  generated_variable.variable.name = x_symbol;
  generated_variable.variable.kind = DifferentiationVariableKind::Auxiliary;
  return generated_variable;
}

}  // namespace amflow
