#include "amflow/runtime/auxiliary_family.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <set>
#include <stdexcept>

#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

namespace {

std::string Trim(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(begin, end - begin);
}

bool ContainsInvariant(const std::vector<std::string>& invariants,
                       const std::string& value) {
  return std::find(invariants.begin(), invariants.end(), value) != invariants.end();
}

std::string StripOuterParentheses(const std::string& value) {
  std::string current = Trim(value);
  while (!current.empty() && current.front() == '(' && current.back() == ')') {
    int depth = 0;
    bool matches = true;
    for (std::size_t index = 0; index < current.size(); ++index) {
      const char ch = current[index];
      if (ch == '(') {
        ++depth;
      } else if (ch == ')') {
        --depth;
        if (depth == 0 && index + 1 != current.size()) {
          matches = false;
          break;
        }
        if (depth < 0) {
          matches = false;
          break;
        }
      }
    }
    if (!matches || depth != 0) {
      break;
    }
    current = Trim(current.substr(1, current.size() - 2));
  }
  return current;
}

struct SplitSequence {
  std::vector<std::string> parts;
  std::vector<char> separators;
};

SplitSequence SplitTopLevelByOperators(const std::string& expression,
                                       const std::string& context,
                                       const std::string& separators) {
  const std::string normalized = Trim(expression);
  if (normalized.empty()) {
    throw std::runtime_error("encountered empty expression in " + context);
  }

  SplitSequence result;
  std::size_t token_begin = 0;
  int depth = 0;
  for (std::size_t index = 0; index < normalized.size(); ++index) {
    const char ch = normalized[index];
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch == ')') {
      --depth;
      if (depth < 0) {
        throw std::runtime_error("encountered unbalanced parentheses in " + context + ": " +
                                 normalized);
      }
      continue;
    }
    if (depth == 0 && separators.find(ch) != std::string::npos) {
      if ((ch == '+' || ch == '-') &&
          Trim(normalized.substr(token_begin, index - token_begin)).empty()) {
        continue;
      }
      const std::string token = Trim(normalized.substr(token_begin, index - token_begin));
      if (token.empty()) {
        throw std::runtime_error("encountered malformed expression in " + context + ": " +
                                 normalized);
      }
      result.parts.push_back(token);
      result.separators.push_back(ch);
      token_begin = index + 1;
    }
  }
  if (depth != 0) {
    throw std::runtime_error("encountered unbalanced parentheses in " + context + ": " +
                             normalized);
  }
  const std::string token = Trim(normalized.substr(token_begin));
  if (token.empty()) {
    throw std::runtime_error("encountered malformed expression in " + context + ": " + normalized);
  }
  result.parts.push_back(token);
  return result;
}

struct SignedTerm {
  bool negative = false;
  std::string expression;
};

SignedTerm NormalizeSignedTerm(const std::string& expression,
                               const bool negative_from_separator,
                               const std::string& context) {
  std::string normalized = Trim(expression);
  if (normalized.empty()) {
    throw std::runtime_error("encountered empty expression in " + context);
  }

  bool negative = negative_from_separator;
  if (!normalized.empty() && (normalized.front() == '+' || normalized.front() == '-')) {
    negative = negative != (normalized.front() == '-');
    normalized = Trim(normalized.substr(1));
    if (normalized.empty()) {
      throw std::runtime_error("encountered malformed expression in " + context + ": " +
                               expression);
    }
  }

  return {negative, normalized};
}

std::vector<SignedTerm> SplitTopLevelTerms(const std::string& expression,
                                           const std::string& context) {
  const SplitSequence split = SplitTopLevelByOperators(expression, context, "+-");
  std::vector<SignedTerm> terms;
  terms.reserve(split.parts.size());
  terms.push_back(NormalizeSignedTerm(split.parts.front(), false, context));
  for (std::size_t index = 1; index < split.parts.size(); ++index) {
    terms.push_back(
        NormalizeSignedTerm(split.parts[index], split.separators[index - 1] == '-', context));
  }
  return terms;
}

struct FlatFactor {
  char separator = '*';
  std::string factor;
};

struct SignedSymbolFactor {
  std::string symbol;
  bool negative = false;
};

std::optional<SignedSymbolFactor> MatchSignedSimpleSymbolFactor(
    const std::string& expression,
    const std::set<std::string>& symbols) {
  std::string factor = StripOuterParentheses(Trim(expression));
  bool negative = false;
  if (!factor.empty() && (factor.front() == '+' || factor.front() == '-')) {
    negative = factor.front() == '-';
    factor = StripOuterParentheses(Trim(factor.substr(1)));
  }
  if (symbols.count(factor) == 0) {
    return std::nullopt;
  }
  return SignedSymbolFactor{factor, negative};
}

void AppendFlattenedFactors(const std::string& expression,
                            const std::string& context,
                            const char separator,
                            std::vector<FlatFactor>& out) {
  const std::string trimmed = Trim(expression);
  const std::string normalized = StripOuterParentheses(trimmed);
  const SplitSequence split = SplitTopLevelByOperators(normalized, context, "*/");
  if (split.separators.empty()) {
    out.push_back({separator, trimmed != normalized ? trimmed : normalized});
    return;
  }
  if (separator == '/') {
    out.push_back({separator, trimmed});
    return;
  }

  AppendFlattenedFactors(split.parts.front(), context, separator, out);
  for (std::size_t index = 1; index < split.parts.size(); ++index) {
    AppendFlattenedFactors(split.parts[index], context, split.separators[index - 1], out);
  }
}

std::set<std::string> CollectExternalMomenta(const ProblemSpec& spec) {
  std::set<std::string> external_momenta(spec.kinematics.incoming_momenta.begin(),
                                         spec.kinematics.incoming_momenta.end());
  external_momenta.insert(spec.kinematics.outgoing_momenta.begin(),
                          spec.kinematics.outgoing_momenta.end());
  return external_momenta;
}

bool HasTopLevelAdditiveOperator(const std::string& expression) {
  int depth = 0;
  for (std::size_t index = 0; index < expression.size(); ++index) {
    const char ch = expression[index];
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch == ')') {
      --depth;
      continue;
    }
    if (depth == 0 && (ch == '+' || ch == '-') &&
        !Trim(expression.substr(0, index)).empty()) {
      return true;
    }
  }
  return false;
}

struct GroupedAdditiveFactor {
  std::string expression;
  bool negative = false;
};

std::optional<GroupedAdditiveFactor> MatchGroupedAdditiveFactor(
    const std::string& raw_factor) {
  const std::string stripped_factor = StripOuterParentheses(raw_factor);
  if (HasTopLevelAdditiveOperator(stripped_factor)) {
    return GroupedAdditiveFactor{stripped_factor, false};
  }

  if (stripped_factor.empty() ||
      (stripped_factor.front() != '+' && stripped_factor.front() != '-')) {
    return std::nullopt;
  }

  const std::string signed_remainder = Trim(stripped_factor.substr(1));
  if (signed_remainder.empty() || signed_remainder.front() != '(') {
    return std::nullopt;
  }

  const std::string signed_group = StripOuterParentheses(signed_remainder);
  if (!HasTopLevelAdditiveOperator(signed_group)) {
    return std::nullopt;
  }
  return GroupedAdditiveFactor{signed_group, stripped_factor.front() == '-'};
}

ExactRational EvaluateExactConstantExpression(const std::string& expression,
                                              const std::string& context) {
  try {
    return EvaluateCoefficientExpression(expression, {});
  } catch (const std::exception& error) {
    throw std::runtime_error(context + " could not evaluate constant expression \"" +
                             expression + "\": " + error.what());
  }
}

std::pair<std::string, std::string> ParseScalarProductPair(const std::string& expression,
                                                           const std::string& context) {
  const SplitSequence split = SplitTopLevelByOperators(expression, context, "*");
  if (split.parts.size() == 2 && split.separators.size() == 1 && split.separators[0] == '*') {
    return {StripOuterParentheses(split.parts[0]), StripOuterParentheses(split.parts[1])};
  }

  const SplitSequence square_split = SplitTopLevelByOperators(expression, context, "^");
  if (square_split.parts.size() == 2 && square_split.separators.size() == 1 &&
      square_split.separators[0] == '^' && Trim(square_split.parts[1]) == "2") {
    const std::string base = StripOuterParentheses(square_split.parts[0]);
    const SplitSequence base_split = SplitTopLevelByOperators(base, context, "+-*/^");
    if (base_split.parts.size() == 1 && base_split.separators.empty()) {
      return {base, base};
    }
  }

  throw std::runtime_error(context +
                           " requires scalar-product left sides of the form a*b or a^2, found: " +
                           expression);
}

std::set<std::string> CollectDeclaredExternalFactorsInExpression(
    const ProblemSpec& spec,
    const std::string& expression) {
  const std::set<std::string> external_momenta = CollectExternalMomenta(spec);
  std::set<std::string> declared_external_factors;
  const std::string context = "reviewed lightlike linear auxiliary rewrite";

  for (const SignedTerm& term : SplitTopLevelTerms(Trim(expression), context)) {
    std::vector<FlatFactor> flat_factors;
    AppendFlattenedFactors(term.expression, context, '*', flat_factors);
    for (const FlatFactor& factor_entry : flat_factors) {
      const std::optional<SignedSymbolFactor> external_factor =
          MatchSignedSimpleSymbolFactor(factor_entry.factor, external_momenta);
      if (external_factor.has_value()) {
        declared_external_factors.insert(external_factor->symbol);
        continue;
      }

      const std::optional<GroupedAdditiveFactor> grouped_factor =
          MatchGroupedAdditiveFactor(factor_entry.factor);
      if (grouped_factor.has_value()) {
        const std::set<std::string> nested_external_factors =
            CollectDeclaredExternalFactorsInExpression(spec, grouped_factor->expression);
        declared_external_factors.insert(nested_external_factors.begin(),
                                         nested_external_factors.end());
      }
    }
  }

  return declared_external_factors;
}

std::string RequireReviewedLightlikeExternalSymbol(const ProblemSpec& spec,
                                                   const Propagator& propagator) {
  const std::set<std::string> used_external_momenta =
      CollectDeclaredExternalFactorsInExpression(spec, propagator.expression);
  if (used_external_momenta.size() != 1) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary rewrite requires exactly one declared lightlike "
        "external momentum symbol to appear in the selected linear propagator expression");
  }
  const std::string external_symbol = *used_external_momenta.begin();

  bool found_lightlike_rule = false;
  for (const auto& rule : spec.kinematics.scalar_product_rules) {
    const auto pair = ParseScalarProductPair(rule.left,
                                             "reviewed lightlike linear auxiliary rewrite");
    if (pair.first != external_symbol || pair.second != external_symbol) {
      continue;
    }
    if (found_lightlike_rule) {
      throw std::runtime_error("reviewed lightlike linear auxiliary rewrite requires one unique " +
                               external_symbol + "*" + external_symbol +
                               " scalar-product rule");
    }
    const ExactRational value = EvaluateExactConstantExpression(
        rule.right, "reviewed lightlike linear auxiliary rewrite");
    if (!value.IsZero()) {
      throw std::runtime_error("reviewed lightlike linear auxiliary rewrite requires scalar-"
                               "product rule " +
                               external_symbol + "*" + external_symbol +
                               " to evaluate exactly to 0");
    }
    found_lightlike_rule = true;
  }

  if (!found_lightlike_rule) {
    throw std::runtime_error("reviewed lightlike linear auxiliary rewrite requires scalar-product "
                             "rule " +
                             external_symbol + "*" + external_symbol);
  }

  return external_symbol;
}

void AppendCoefficientFactor(std::ostringstream& coefficient_expression,
                             bool& coefficient_started,
                             const char separator,
                             const std::string& factor) {
  if (!coefficient_started && separator == '/') {
    coefficient_expression << "1/";
  } else if (coefficient_started) {
    coefficient_expression << separator;
  }
  coefficient_expression << factor;
  coefficient_started = true;
}

std::optional<ExactRational> TryEvaluateGroupedExternalFactorCoefficient(
    const std::string& raw_factor,
    const std::string& external_symbol) {
  const std::optional<GroupedAdditiveFactor> grouped_factor =
      MatchGroupedAdditiveFactor(raw_factor);
  if (!grouped_factor.has_value()) {
    return std::nullopt;
  }

  const std::string context = "reviewed lightlike linear auxiliary rewrite";
  const std::set<std::string> selected_external_symbol = {external_symbol};
  std::vector<std::string> coefficient_terms;

  for (const SignedTerm& grouped_term : SplitTopLevelTerms(grouped_factor->expression, context)) {
    std::vector<FlatFactor> flat_factors;
    AppendFlattenedFactors(grouped_term.expression, context, '*', flat_factors);

    bool saw_external_factor = false;
    std::ostringstream coefficient_expression;
    bool coefficient_started = false;
    for (const FlatFactor& factor_entry : flat_factors) {
      const std::string raw_term_factor = Trim(factor_entry.factor);
      const std::optional<SignedSymbolFactor> external_factor =
          MatchSignedSimpleSymbolFactor(raw_term_factor, selected_external_symbol);
      if (external_factor.has_value()) {
        if (factor_entry.separator == '/') {
          throw std::runtime_error(
              "reviewed lightlike linear auxiliary rewrite keeps the external symbol out of "
              "denominators");
        }
        if (saw_external_factor) {
          throw std::runtime_error(
              "reviewed lightlike linear auxiliary rewrite requires one external factor per "
              "bilinear term");
        }
        saw_external_factor = true;
        if (external_factor->negative) {
          AppendCoefficientFactor(coefficient_expression,
                                  coefficient_started,
                                  '*',
                                  "-1");
        }
        continue;
      }

      std::optional<ExactRational> coefficient_piece;
      try {
        coefficient_piece = EvaluateExactConstantExpression(raw_term_factor, context);
      } catch (const std::runtime_error&) {
        return std::nullopt;
      }
      if (factor_entry.separator == '/' && coefficient_piece->IsZero()) {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite encountered "
                                 "division by zero in a rational coefficient factor");
      }
      AppendCoefficientFactor(coefficient_expression,
                              coefficient_started,
                              factor_entry.separator,
                              raw_term_factor);
    }

    if (!saw_external_factor) {
      return std::nullopt;
    }

    std::string coefficient_text =
        coefficient_started ? EvaluateExactConstantExpression(coefficient_expression.str(),
                                                              context)
                                  .ToString()
                            : "1";
    if (grouped_term.negative) {
      coefficient_text =
          EvaluateExactConstantExpression("-(" + coefficient_text + ")", context).ToString();
    }
    coefficient_terms.push_back(coefficient_text);
  }

  if (coefficient_terms.empty()) {
    return std::nullopt;
  }

  std::ostringstream total_expression;
  for (std::size_t index = 0; index < coefficient_terms.size(); ++index) {
    if (index > 0) {
      total_expression << "+";
    }
    total_expression << "(" << coefficient_terms[index] << ")";
  }

  std::string total = total_expression.str();
  if (grouped_factor->negative) {
    total = "-(" + total + ")";
  }
  return EvaluateExactConstantExpression(total, context);
}

std::optional<std::string> RenderReviewedLightlikeLoopLinearTerm(
    const ProblemSpec& spec,
    const SignedTerm& term,
    const std::string& external_symbol,
    const std::string& coefficient_prefix) {
  const std::set<std::string> loop_momenta(spec.family.loop_momenta.begin(),
                                           spec.family.loop_momenta.end());
  const std::set<std::string> selected_external_symbol = {external_symbol};

  std::vector<FlatFactor> flat_factors;
  AppendFlattenedFactors(term.expression,
                         "reviewed lightlike linear auxiliary rewrite",
                         '*',
                         flat_factors);

  std::string loop_factor;
  bool saw_external_factor = false;
  std::ostringstream coefficient_expression;
  bool coefficient_started = false;
  if (!coefficient_prefix.empty()) {
    coefficient_expression << "(" << coefficient_prefix << ")";
    coefficient_started = true;
  }

  for (const FlatFactor& factor_entry : flat_factors) {
    const std::string raw_factor = Trim(factor_entry.factor);
    const std::string factor = StripOuterParentheses(raw_factor);
    if (factor.empty()) {
      throw std::runtime_error(
          "reviewed lightlike linear auxiliary rewrite encountered an empty factor in propagator "
          "expression");
    }

    const std::optional<SignedSymbolFactor> external_factor =
        MatchSignedSimpleSymbolFactor(raw_factor, selected_external_symbol);
    if (external_factor.has_value()) {
      if (factor_entry.separator == '/') {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite keeps the external "
                                 "symbol out of denominators");
      }
      if (saw_external_factor) {
        throw std::runtime_error(
            "reviewed lightlike linear auxiliary rewrite requires one external factor per "
            "bilinear term");
      }
      saw_external_factor = true;
      if (external_factor->negative) {
        AppendCoefficientFactor(coefficient_expression,
                                coefficient_started,
                                '*',
                                "-1");
      }
      continue;
    }

    const std::optional<ExactRational> grouped_external_coefficient =
        TryEvaluateGroupedExternalFactorCoefficient(raw_factor, external_symbol);
    if (grouped_external_coefficient.has_value()) {
      if (factor_entry.separator == '/') {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite keeps the external "
                                 "symbol out of denominators");
      }
      if (saw_external_factor) {
        throw std::runtime_error(
            "reviewed lightlike linear auxiliary rewrite requires one external factor per "
            "bilinear term");
      }
      saw_external_factor = true;
      AppendCoefficientFactor(coefficient_expression,
                              coefficient_started,
                              '*',
                              grouped_external_coefficient->ToString());
      continue;
    }

    const std::optional<SignedSymbolFactor> loop_factor_match =
        MatchSignedSimpleSymbolFactor(raw_factor, loop_momenta);
    if (loop_factor_match.has_value()) {
      if (factor_entry.separator == '/') {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite keeps loop "
                                 "momenta out of denominators");
      }
      if (!loop_factor.empty()) {
        throw std::runtime_error(
            "reviewed lightlike linear auxiliary rewrite requires one loop factor per bilinear "
            "term");
      }
      loop_factor = loop_factor_match->symbol;
      if (loop_factor_match->negative) {
        AppendCoefficientFactor(coefficient_expression,
                                coefficient_started,
                                '*',
                                "-1");
      }
      continue;
    }

    const ExactRational coefficient_piece = EvaluateExactConstantExpression(
        raw_factor, "reviewed lightlike linear auxiliary rewrite");
    if (factor_entry.separator == '/' && coefficient_piece.IsZero()) {
      throw std::runtime_error("reviewed lightlike linear auxiliary rewrite encountered "
                               "division by zero in a rational coefficient factor");
    }
    AppendCoefficientFactor(coefficient_expression,
                            coefficient_started,
                            factor_entry.separator,
                            raw_factor);
  }

  if (loop_factor.empty() && !saw_external_factor) {
    static_cast<void>(EvaluateExactConstantExpression(
        term.negative ? "-(" + term.expression + ")" : term.expression,
        "reviewed lightlike linear auxiliary rewrite"));
    return std::nullopt;
  }

  if (loop_factor.empty() || !saw_external_factor) {
    throw std::runtime_error("reviewed lightlike linear auxiliary rewrite supports only "
                             "rational constants plus loop-" +
                             external_symbol + " bilinears");
  }

  std::string coefficient_text =
      coefficient_started
          ? EvaluateExactConstantExpression(coefficient_expression.str(),
                                            "reviewed lightlike linear auxiliary rewrite")
                .ToString()
          : "1";
  if (term.negative) {
    coefficient_text = EvaluateExactConstantExpression(
                           "-(" + coefficient_text + ")",
                           "reviewed lightlike linear auxiliary rewrite")
                           .ToString();
  }
  if (coefficient_text == "0") {
    return std::nullopt;
  }
  if (coefficient_text == "1") {
    return loop_factor;
  }
  return "(" + coefficient_text + ")*(" + loop_factor + ")";
}

std::optional<std::vector<std::string>> TryRenderGroupedLoopMomentumFactorTerms(
    const ProblemSpec& spec,
    const SignedTerm& term,
    const std::string& external_symbol,
    const std::string& coefficient_prefix);

std::optional<std::vector<std::string>> TryRenderGroupedCommonCoefficientLoopLinearTerms(
    const ProblemSpec& spec,
    const SignedTerm& term,
    const std::string& external_symbol,
    const std::string& coefficient_prefix,
    const std::size_t remaining_nested_grouped_common_layers) {
  const std::string context = "reviewed lightlike linear auxiliary rewrite";
  const SplitSequence split = SplitTopLevelByOperators(term.expression, context, "*/");

  std::string grouped_linear_combination;
  std::ostringstream coefficient_expression;
  bool coefficient_started = false;
  if (!coefficient_prefix.empty()) {
    coefficient_expression << "(" << coefficient_prefix << ")";
    coefficient_started = true;
  }

  for (std::size_t index = 0; index < split.parts.size(); ++index) {
    const char separator = index == 0 ? '*' : split.separators[index - 1];
    const std::string raw_factor = Trim(split.parts[index]);

    std::optional<ExactRational> coefficient_piece;
    try {
      coefficient_piece = EvaluateExactConstantExpression(raw_factor, context);
    } catch (const std::runtime_error&) {
    }
    if (coefficient_piece.has_value()) {
      if (separator == '/' && coefficient_piece->IsZero()) {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite encountered "
                                 "division by zero in a rational coefficient factor");
      }
      AppendCoefficientFactor(coefficient_expression,
                              coefficient_started,
                              separator,
                              raw_factor);
      continue;
    }

    const std::optional<GroupedAdditiveFactor> grouped_factor =
        MatchGroupedAdditiveFactor(raw_factor);
    if (separator == '/' || !grouped_linear_combination.empty()) {
      return std::nullopt;
    }
    if (grouped_factor.has_value()) {
      grouped_linear_combination = grouped_factor->expression;
      if (grouped_factor->negative) {
        AppendCoefficientFactor(coefficient_expression,
                                coefficient_started,
                                '*',
                                "-1");
      }
      continue;
    }

    const std::string stripped_factor = StripOuterParentheses(raw_factor);
    if (stripped_factor == raw_factor) {
      return std::nullopt;
    }
    const SplitSequence stripped_split =
        SplitTopLevelByOperators(stripped_factor, context, "*/");
    if (stripped_split.separators.empty()) {
      return std::nullopt;
    }
    grouped_linear_combination = stripped_factor;
  }

  if (grouped_linear_combination.empty()) {
    return std::nullopt;
  }

  std::string common_coefficient =
      coefficient_started
          ? EvaluateExactConstantExpression(coefficient_expression.str(), context).ToString()
          : "1";
  if (term.negative) {
    common_coefficient =
        EvaluateExactConstantExpression("-(" + common_coefficient + ")", context).ToString();
  }

  std::vector<std::string> rendered_terms;
  for (const SignedTerm& grouped_term :
       SplitTopLevelTerms(grouped_linear_combination, context)) {
    if (remaining_nested_grouped_common_layers > 0) {
      if (const std::optional<std::vector<std::string>> nested_grouped_terms =
              TryRenderGroupedCommonCoefficientLoopLinearTerms(
                  spec,
                  grouped_term,
                  external_symbol,
                  common_coefficient,
                  remaining_nested_grouped_common_layers - 1);
          nested_grouped_terms.has_value()) {
        rendered_terms.insert(rendered_terms.end(),
                              nested_grouped_terms->begin(),
                              nested_grouped_terms->end());
        continue;
      }
    }

    if (const std::optional<std::vector<std::string>> grouped_loop_terms =
            TryRenderGroupedLoopMomentumFactorTerms(
                spec, grouped_term, external_symbol, common_coefficient);
        grouped_loop_terms.has_value()) {
      rendered_terms.insert(rendered_terms.end(),
                            grouped_loop_terms->begin(),
                            grouped_loop_terms->end());
      continue;
    }

    if (std::optional<std::string> rendered_term =
            RenderReviewedLightlikeLoopLinearTerm(
                spec, grouped_term, external_symbol, common_coefficient);
        rendered_term.has_value()) {
      rendered_terms.push_back(*rendered_term);
    }
  }
  return rendered_terms;
}

std::optional<std::vector<std::string>> TryRenderGroupedLoopMomentumFactorTerms(
    const ProblemSpec& spec,
    const SignedTerm& term,
    const std::string& external_symbol,
    const std::string& coefficient_prefix) {
  const std::string context = "reviewed lightlike linear auxiliary rewrite";
  const SplitSequence split = SplitTopLevelByOperators(term.expression, context, "*/");

  std::string grouped_loop_combination;
  bool saw_external_factor = false;
  const std::set<std::string> selected_external_symbol = {external_symbol};
  std::ostringstream coefficient_expression;
  bool coefficient_started = false;
  if (!coefficient_prefix.empty()) {
    coefficient_expression << "(" << coefficient_prefix << ")";
    coefficient_started = true;
  }

  for (std::size_t index = 0; index < split.parts.size(); ++index) {
    const char separator = index == 0 ? '*' : split.separators[index - 1];
    const std::string raw_factor = Trim(split.parts[index]);

    const std::optional<SignedSymbolFactor> external_factor =
        MatchSignedSimpleSymbolFactor(raw_factor, selected_external_symbol);
    if (external_factor.has_value()) {
      if (separator == '/') {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite keeps the external "
                                 "symbol out of denominators");
      }
      if (saw_external_factor) {
        throw std::runtime_error(
            "reviewed lightlike linear auxiliary rewrite requires one external factor per "
            "bilinear term");
      }
      saw_external_factor = true;
      if (external_factor->negative) {
        AppendCoefficientFactor(coefficient_expression,
                                coefficient_started,
                                '*',
                                "-1");
      }
      continue;
    }

    const std::optional<ExactRational> grouped_external_coefficient =
        TryEvaluateGroupedExternalFactorCoefficient(raw_factor, external_symbol);
    if (grouped_external_coefficient.has_value()) {
      if (separator == '/') {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite keeps the external "
                                 "symbol out of denominators");
      }
      if (saw_external_factor) {
        throw std::runtime_error(
            "reviewed lightlike linear auxiliary rewrite requires one external factor per "
            "bilinear term");
      }
      saw_external_factor = true;
      AppendCoefficientFactor(coefficient_expression,
                              coefficient_started,
                              '*',
                              grouped_external_coefficient->ToString());
      continue;
    }

    std::optional<ExactRational> coefficient_piece;
    try {
      coefficient_piece = EvaluateExactConstantExpression(raw_factor, context);
    } catch (const std::runtime_error&) {
    }
    if (coefficient_piece.has_value()) {
      if (separator == '/' && coefficient_piece->IsZero()) {
        throw std::runtime_error("reviewed lightlike linear auxiliary rewrite encountered "
                                 "division by zero in a rational coefficient factor");
      }
      AppendCoefficientFactor(coefficient_expression,
                              coefficient_started,
                              separator,
                              raw_factor);
      continue;
    }

    const std::optional<GroupedAdditiveFactor> grouped_factor =
        MatchGroupedAdditiveFactor(raw_factor);
    if (separator == '/' || !grouped_factor.has_value() ||
        !grouped_loop_combination.empty()) {
      return std::nullopt;
    }
    grouped_loop_combination = grouped_factor->expression;
    if (grouped_factor->negative) {
      AppendCoefficientFactor(coefficient_expression,
                              coefficient_started,
                              '*',
                              "-1");
    }
  }

  if (grouped_loop_combination.empty() || !saw_external_factor) {
    return std::nullopt;
  }

  std::string common_coefficient =
      coefficient_started
          ? EvaluateExactConstantExpression(coefficient_expression.str(), context).ToString()
          : "1";
  if (term.negative) {
    common_coefficient =
        EvaluateExactConstantExpression("-(" + common_coefficient + ")", context).ToString();
  }

  std::vector<std::string> rendered_terms;
  for (const SignedTerm& grouped_term : SplitTopLevelTerms(grouped_loop_combination, context)) {
    SignedTerm expanded_term = grouped_term;
    expanded_term.expression = grouped_term.expression + "*" + external_symbol;
    if (std::optional<std::string> rendered_term = RenderReviewedLightlikeLoopLinearTerm(
            spec, expanded_term, external_symbol, common_coefficient);
        rendered_term.has_value()) {
      rendered_terms.push_back(*rendered_term);
    }
  }
  return rendered_terms;
}

std::string BuildReviewedLightlikeLoopLinearCombination(const ProblemSpec& spec,
                                                        const Propagator& propagator,
                                                        const std::string& external_symbol) {
  const std::vector<SignedTerm> terms =
      SplitTopLevelTerms(Trim(propagator.expression),
                         "reviewed lightlike linear auxiliary rewrite");

  std::vector<std::string> rendered_terms;
  bool saw_bilinear_term = false;
  for (const SignedTerm& term : terms) {
    if (const std::optional<std::vector<std::string>> grouped_terms =
            TryRenderGroupedCommonCoefficientLoopLinearTerms(spec, term, external_symbol, "", 9);
        grouped_terms.has_value()) {
      for (const std::string& grouped_term : *grouped_terms) {
        rendered_terms.push_back(grouped_term);
        saw_bilinear_term = true;
      }
      continue;
    }

    if (const std::optional<std::vector<std::string>> grouped_terms =
            TryRenderGroupedLoopMomentumFactorTerms(spec, term, external_symbol, "");
        grouped_terms.has_value()) {
      for (const std::string& grouped_term : *grouped_terms) {
        rendered_terms.push_back(grouped_term);
        saw_bilinear_term = true;
      }
      continue;
    }

    if (std::optional<std::string> rendered_term =
            RenderReviewedLightlikeLoopLinearTerm(spec, term, external_symbol, "");
        rendered_term.has_value()) {
      rendered_terms.push_back(*rendered_term);
      saw_bilinear_term = true;
    }
  }

  if (!saw_bilinear_term || rendered_terms.empty()) {
    throw std::runtime_error("reviewed lightlike linear auxiliary rewrite requires at least one "
                             "loop-" +
                             external_symbol + " bilinear term");
  }

  std::ostringstream out;
  for (std::size_t index = 0; index < rendered_terms.size(); ++index) {
    if (index > 0) {
      out << " + ";
    }
    out << rendered_terms[index];
  }
  return out.str();
}

}  // namespace

Propagator BuildReviewedLightlikeLinearAuxiliaryPropagator(const ProblemSpec& spec,
                                                           const std::size_t propagator_index,
                                                           const std::string& x_symbol) {
  const std::string trimmed_x_symbol = Trim(x_symbol);
  if (trimmed_x_symbol.empty()) {
    throw std::runtime_error("reviewed lightlike linear auxiliary rewrite symbol must not be "
                             "empty");
  }
  if (propagator_index >= spec.family.propagators.size()) {
    throw std::runtime_error("reviewed lightlike linear auxiliary rewrite propagator index out of "
                             "range: " +
                             std::to_string(propagator_index));
  }

  const Propagator& original = spec.family.propagators[propagator_index];
  if (original.kind != PropagatorKind::Linear || !original.variant.has_value() ||
      *original.variant != PropagatorVariant::Linear) {
    throw std::runtime_error("reviewed lightlike linear auxiliary rewrite requires propagator " +
                             std::to_string(propagator_index) +
                             " to carry explicit variant \"linear\" on kind \"linear\"");
  }

  const std::string external_symbol = RequireReviewedLightlikeExternalSymbol(spec, original);
  const std::string loop_linear_combination = BuildReviewedLightlikeLoopLinearCombination(
      spec, original, external_symbol);

  Propagator rewritten = original;
  rewritten.expression = trimmed_x_symbol + "*((" + loop_linear_combination + ")^2) + (" +
                         Trim(original.expression) + ")";
  rewritten.kind = PropagatorKind::Standard;
  rewritten.variant = PropagatorVariant::Quadratic;
  return rewritten;
}

std::size_t SelectReviewedLightlikeLinearAuxiliaryPropagatorIndex(const ProblemSpec& spec) {
  std::optional<std::size_t> selected_index;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Linear || !propagator.variant.has_value() ||
        *propagator.variant != PropagatorVariant::Linear) {
      continue;
    }
    if (selected_index.has_value()) {
      throw std::runtime_error(
          "reviewed lightlike linear auxiliary automatic selection requires exactly one "
          "explicit kind \"linear\" / variant \"linear\" propagator, found multiple candidates: " +
          std::to_string(*selected_index) + " and " + std::to_string(index));
    }
    selected_index = index;
  }
  if (!selected_index.has_value()) {
    throw std::runtime_error(
        "reviewed lightlike linear auxiliary automatic selection requires exactly one explicit "
        "kind \"linear\" / variant \"linear\" propagator");
  }
  return *selected_index;
}

LightlikeLinearAuxiliaryTransformResult ApplyReviewedLightlikeLinearAuxiliaryTransform(
    const ProblemSpec& spec,
    const std::size_t propagator_index,
    const std::string& x_symbol) {
  LightlikeLinearAuxiliaryTransformResult result;
  result.transformed_spec = spec;
  result.x_symbol = Trim(x_symbol);
  result.rewritten_propagator_index = propagator_index;
  result.transformed_spec.family.propagators[propagator_index] =
      BuildReviewedLightlikeLinearAuxiliaryPropagator(spec, propagator_index, x_symbol);
  if (!ContainsInvariant(result.transformed_spec.kinematics.invariants, result.x_symbol)) {
    result.transformed_spec.kinematics.invariants.push_back(result.x_symbol);
  }
  return result;
}

AuxiliaryFamilyTransformResult ApplyEtaInsertion(const ProblemSpec& spec,
                                                const EtaInsertionDecision& decision,
                                                const std::string& eta_symbol) {
  if (decision.selected_propagator_indices.empty()) {
    throw std::runtime_error("eta insertion requires at least one selected propagator index");
  }
  if (Trim(eta_symbol).empty()) {
    throw std::runtime_error("eta insertion symbol must not be empty");
  }

  AuxiliaryFamilyTransformResult result;
  result.transformed_spec = spec;
  result.eta_symbol = eta_symbol;

  std::set<std::size_t> seen_indices;
  const std::size_t propagator_count = spec.family.propagators.size();
  for (const std::size_t index : decision.selected_propagator_indices) {
    if (index >= propagator_count) {
      throw std::runtime_error("eta insertion propagator index out of range: " +
                               std::to_string(index));
    }
    if (!seen_indices.insert(index).second) {
      throw std::runtime_error("duplicate eta insertion propagator index: " +
                               std::to_string(index));
    }

    const Propagator& original = spec.family.propagators[index];
    if (original.kind == PropagatorKind::Auxiliary) {
      throw std::runtime_error("eta insertion cannot target auxiliary propagator index: " +
                               std::to_string(index));
    }
    Propagator& transformed = result.transformed_spec.family.propagators[index];
    transformed.expression = "(" + original.expression + ") + " + eta_symbol;
    transformed.mass = Trim(original.mass);
    result.rewritten_propagator_indices.push_back(index);
  }

  if (!ContainsInvariant(result.transformed_spec.kinematics.invariants, eta_symbol)) {
    result.transformed_spec.kinematics.invariants.push_back(eta_symbol);
  }
  return result;
}

}  // namespace amflow
