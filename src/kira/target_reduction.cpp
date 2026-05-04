#include "amflow/kira/target_reduction.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

namespace {

std::string IntegralLabel(const std::string& family, const std::vector<int>& indices) {
  std::ostringstream out;
  out << family << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index != 0) {
      out << ",";
    }
    out << indices[index];
  }
  out << "]";
  return out.str();
}

std::string IntegralLabel(const MasterIntegral& integral) {
  return IntegralLabel(integral.family, integral.indices);
}

ExactRational ZeroRational() {
  return {"0", "1"};
}

ExactRational OneRational() {
  return {"1", "1"};
}

std::string Parenthesize(const ExactRational& value) {
  return "(" + value.ToString() + ")";
}

ExactRational ExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, NumericEvaluationPoint{});
}

ExactRational ParseExactRational(const std::string& expression) {
  if (expression.empty()) {
    return ZeroRational();
  }
  return ExactArithmetic(expression);
}

ExactRational AddRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "+" + Parenthesize(rhs));
}

ExactRational SubtractRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "-" + Parenthesize(rhs));
}

ExactRational NegateRational(const ExactRational& value) {
  return ExactArithmetic("-" + Parenthesize(value));
}

ExactRational MultiplyRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "*" + Parenthesize(rhs));
}

ExactRational DivideRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "/" + Parenthesize(rhs));
}

using LaurentSeries = std::map<int, ExactRational>;

struct ComplexLaurentSeries {
  LaurentSeries real;
  LaurentSeries imaginary;
};

std::optional<int> LeadingOrder(const LaurentSeries& series) {
  if (series.empty()) {
    return std::nullopt;
  }
  return series.begin()->first;
}

ExactRational LaurentCoefficient(const LaurentSeries& series, const int order) {
  const auto coefficient_it = series.find(order);
  if (coefficient_it == series.end()) {
    return ZeroRational();
  }
  return coefficient_it->second;
}

void SetLaurentCoefficient(LaurentSeries& series,
                           const int order,
                           const ExactRational& coefficient) {
  if (coefficient.IsZero()) {
    series.erase(order);
    return;
  }
  series[order] = coefficient;
}

void AddLaurentContribution(LaurentSeries& series,
                            const int order,
                            const ExactRational& coefficient) {
  if (coefficient.IsZero()) {
    return;
  }
  const auto coefficient_it = series.find(order);
  if (coefficient_it == series.end()) {
    series.emplace(order, coefficient);
    return;
  }
  const ExactRational sum = AddRational(coefficient_it->second, coefficient);
  if (sum.IsZero()) {
    series.erase(coefficient_it);
  } else {
    coefficient_it->second = sum;
  }
}

LaurentSeries TruncateLaurentSeries(const LaurentSeries& series, const int max_order) {
  LaurentSeries truncated;
  for (const auto& [order, coefficient] : series) {
    if (order <= max_order) {
      SetLaurentCoefficient(truncated, order, coefficient);
    }
  }
  return truncated;
}

LaurentSeries MakeConstantSeries(const ExactRational& value) {
  LaurentSeries series;
  SetLaurentCoefficient(series, 0, value);
  return series;
}

LaurentSeries MakeEpsilonSeries() {
  LaurentSeries series;
  SetLaurentCoefficient(series, 1, OneRational());
  return series;
}

LaurentSeries AddLaurentSeries(const LaurentSeries& lhs,
                               const LaurentSeries& rhs,
                               const int max_order) {
  LaurentSeries result = lhs;
  for (const auto& [order, coefficient] : rhs) {
    AddLaurentContribution(result, order, coefficient);
  }
  return TruncateLaurentSeries(result, max_order);
}

LaurentSeries SubtractLaurentSeries(const LaurentSeries& lhs,
                                    const LaurentSeries& rhs,
                                    const int max_order) {
  LaurentSeries result = lhs;
  for (const auto& [order, coefficient] : rhs) {
    AddLaurentContribution(result, order, NegateRational(coefficient));
  }
  return TruncateLaurentSeries(result, max_order);
}

LaurentSeries NegateLaurentSeries(const LaurentSeries& series, const int max_order) {
  LaurentSeries result;
  for (const auto& [order, coefficient] : series) {
    if (order <= max_order) {
      SetLaurentCoefficient(result, order, NegateRational(coefficient));
    }
  }
  return result;
}

LaurentSeries MultiplyLaurentSeries(const LaurentSeries& lhs,
                                    const LaurentSeries& rhs,
                                    const int max_order) {
  LaurentSeries result;
  for (const auto& [lhs_order, lhs_coefficient] : lhs) {
    for (const auto& [rhs_order, rhs_coefficient] : rhs) {
      const int order = lhs_order + rhs_order;
      if (order > max_order) {
        continue;
      }
      AddLaurentContribution(result,
                             order,
                             MultiplyRational(lhs_coefficient, rhs_coefficient));
    }
  }
  return result;
}

LaurentSeries DivideLaurentSeries(const LaurentSeries& numerator,
                                  const LaurentSeries& denominator,
                                  const std::string& expression,
                                  const int max_order) {
  const std::optional<int> denominator_leading = LeadingOrder(denominator);
  if (!denominator_leading.has_value()) {
    throw std::invalid_argument(
        "target reduction coefficient denominator has no nonzero epsilon-series term through "
        "the requested order in \"" +
        expression + "\"");
  }

  const std::optional<int> numerator_leading = LeadingOrder(numerator);
  if (!numerator_leading.has_value()) {
    return LaurentSeries{};
  }

  const int shift = *numerator_leading - *denominator_leading;
  if (shift > max_order) {
    return LaurentSeries{};
  }

  LaurentSeries quotient;
  const ExactRational denominator_leading_coefficient =
      LaurentCoefficient(denominator, *denominator_leading);
  for (int normalized_order = 0; normalized_order <= max_order - shift;
       ++normalized_order) {
    ExactRational remainder =
        LaurentCoefficient(numerator, *numerator_leading + normalized_order);
    for (int denominator_offset = 1; denominator_offset <= normalized_order;
         ++denominator_offset) {
      const ExactRational denominator_term =
          LaurentCoefficient(denominator, *denominator_leading + denominator_offset);
      const ExactRational quotient_term =
          LaurentCoefficient(quotient, shift + normalized_order - denominator_offset);
      if (!denominator_term.IsZero() && !quotient_term.IsZero()) {
        remainder =
            SubtractRational(remainder,
                             MultiplyRational(denominator_term, quotient_term));
      }
    }
    SetLaurentCoefficient(
        quotient,
        shift + normalized_order,
        DivideRational(remainder, denominator_leading_coefficient));
  }
  return quotient;
}

LaurentSeries PowerLaurentSeries(LaurentSeries base,
                                 const int exponent,
                                 const int max_order) {
  LaurentSeries result = MakeConstantSeries(OneRational());
  for (int index = 0; index < exponent; ++index) {
    result = MultiplyLaurentSeries(result, base, max_order);
  }
  return result;
}

enum class TokenKind {
  Identifier,
  Number,
  Plus,
  Minus,
  Star,
  Slash,
  Caret,
  LeftParen,
  RightParen,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
};

std::vector<Token> Tokenize(const std::string& expression) {
  std::vector<Token> tokens;
  for (std::size_t index = 0; index < expression.size();) {
    const char ch = expression[index];
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      ++index;
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      const std::size_t begin = index;
      while (index < expression.size() &&
             std::isdigit(static_cast<unsigned char>(expression[index])) != 0) {
        ++index;
      }
      tokens.push_back({TokenKind::Number, expression.substr(begin, index - begin)});
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
      const std::size_t begin = index;
      while (index < expression.size() &&
             (std::isalnum(static_cast<unsigned char>(expression[index])) != 0 ||
              expression[index] == '_')) {
        ++index;
      }
      tokens.push_back({TokenKind::Identifier, expression.substr(begin, index - begin)});
      continue;
    }

    switch (ch) {
      case '+':
        tokens.push_back({TokenKind::Plus, "+"});
        break;
      case '-':
        tokens.push_back({TokenKind::Minus, "-"});
        break;
      case '*':
        tokens.push_back({TokenKind::Star, "*"});
        break;
      case '/':
        tokens.push_back({TokenKind::Slash, "/"});
        break;
      case '^':
        tokens.push_back({TokenKind::Caret, "^"});
        break;
      case '(':
        tokens.push_back({TokenKind::LeftParen, "("});
        break;
      case ')':
        tokens.push_back({TokenKind::RightParen, ")"});
        break;
      default:
        throw std::invalid_argument(
            "target reduction coefficient parser encountered unexpected character \"" +
            std::string(1, ch) + "\" in \"" + expression + "\"");
    }
    ++index;
  }
  tokens.push_back({TokenKind::End, ""});
  return tokens;
}

class LaurentExpressionParser {
 public:
  LaurentExpressionParser(std::string expression,
                          std::optional<LaurentSeries> dimension_series,
                          const int max_order)
      : expression_(std::move(expression)),
        dimension_series_(std::move(dimension_series)),
        tokens_(Tokenize(expression_)),
        max_order_(max_order) {}

  LaurentSeries Parse() {
    LaurentSeries result = ParseExpression();
    if (Current().kind != TokenKind::End) {
      throw Malformed("unexpected trailing token \"" + Current().text + "\"");
    }
    return result;
  }

 private:
  const Token& Current() const { return tokens_[position_]; }

  const Token& Advance() {
    const Token& current = Current();
    if (position_ < tokens_.size()) {
      ++position_;
    }
    return current;
  }

  bool Match(const TokenKind kind) {
    if (Current().kind != kind) {
      return false;
    }
    Advance();
    return true;
  }

  std::invalid_argument Malformed(const std::string& detail) const {
    return std::invalid_argument("target reduction coefficient parser encountered " + detail +
                                 " in \"" + expression_ + "\"");
  }

  LaurentSeries ParseExpression() {
    LaurentSeries value = ParseTerm();
    while (true) {
      if (Match(TokenKind::Plus)) {
        value = AddLaurentSeries(value, ParseTerm(), max_order_);
        continue;
      }
      if (Match(TokenKind::Minus)) {
        value = SubtractLaurentSeries(value, ParseTerm(), max_order_);
        continue;
      }
      break;
    }
    return value;
  }

  LaurentSeries ParseTerm() {
    LaurentSeries value = ParseFactor();
    while (true) {
      if (Match(TokenKind::Star)) {
        value = MultiplyLaurentSeries(value, ParseFactor(), max_order_);
        continue;
      }
      if (Match(TokenKind::Slash)) {
        value = DivideLaurentSeries(value, ParseFactor(), expression_, max_order_);
        continue;
      }
      break;
    }
    return value;
  }

  LaurentSeries ParseFactor() {
    if (Match(TokenKind::Plus)) {
      return ParseFactor();
    }
    if (Match(TokenKind::Minus)) {
      return NegateLaurentSeries(ParseFactor(), max_order_);
    }
    return ParsePower();
  }

  LaurentSeries ParsePower() {
    LaurentSeries value = ParsePrimary();
    if (!Match(TokenKind::Caret)) {
      return value;
    }
    if (Current().kind != TokenKind::Number) {
      throw Malformed("expected a non-negative integer exponent");
    }
    const std::string exponent_text = Advance().text;
    std::size_t consumed = 0;
    int exponent = 0;
    try {
      exponent = std::stoi(exponent_text, &consumed);
    } catch (const std::exception&) {
      throw Malformed("invalid exponent \"" + exponent_text + "\"");
    }
    if (consumed != exponent_text.size()) {
      throw Malformed("invalid exponent \"" + exponent_text + "\"");
    }
    return PowerLaurentSeries(std::move(value), exponent, max_order_);
  }

  LaurentSeries ParsePrimary() {
    if (Current().kind == TokenKind::Number) {
      const std::string number = Advance().text;
      return MakeConstantSeries({number, "1"});
    }
    if (Current().kind == TokenKind::Identifier) {
      const std::string identifier = Advance().text;
      if (identifier == "eps") {
        return MakeEpsilonSeries();
      }
      if ((identifier == "dimension" || identifier == "d") && dimension_series_.has_value()) {
        return *dimension_series_;
      }
      throw std::invalid_argument(
          "target reduction coefficient parser requires a numeric or epsilon-series binding "
          "for symbol \"" +
          identifier + "\"");
    }
    if (Match(TokenKind::LeftParen)) {
      LaurentSeries value = ParseExpression();
      if (!Match(TokenKind::RightParen)) {
        throw Malformed("expected ')'");
      }
      return value;
    }
    if (Current().kind == TokenKind::End) {
      throw Malformed("unexpected end of expression");
    }
    throw Malformed("unexpected token \"" + Current().text + "\"");
  }

  std::string expression_;
  std::optional<LaurentSeries> dimension_series_;
  std::vector<Token> tokens_;
  std::size_t position_ = 0;
  int max_order_ = 0;
};

LaurentSeries ParseLaurentExpression(const std::string& expression,
                                     const std::optional<LaurentSeries>& dimension_series,
                                     const int max_order) {
  return LaurentExpressionParser(expression, dimension_series, max_order).Parse();
}

ComplexLaurentSeries ParseMasterEpsilonCoefficients(
    const std::vector<SolverDiagnostics::EpsilonCoefficient>& coefficients) {
  ComplexLaurentSeries series;
  for (const auto& coefficient : coefficients) {
    SetLaurentCoefficient(series.real,
                          coefficient.order,
                          ParseExactRational(coefficient.real.empty() ? "0" :
                                                               coefficient.real));
    SetLaurentCoefficient(series.imaginary,
                          coefficient.order,
                          ParseExactRational(coefficient.imaginary.empty() ? "0" :
                                                                    coefficient.imaginary));
  }
  return series;
}

std::optional<int> MinimumOrder(const ComplexLaurentSeries& series) {
  const std::optional<int> real_order = LeadingOrder(series.real);
  const std::optional<int> imaginary_order = LeadingOrder(series.imaginary);
  if (!real_order.has_value()) {
    return imaginary_order;
  }
  if (!imaginary_order.has_value()) {
    return real_order;
  }
  return std::min(*real_order, *imaginary_order);
}

ComplexLaurentSeries ScaleComplexLaurentSeries(const ComplexLaurentSeries& series,
                                               const LaurentSeries& coefficient,
                                               const int max_order) {
  return {MultiplyLaurentSeries(coefficient, series.real, max_order),
          MultiplyLaurentSeries(coefficient, series.imaginary, max_order)};
}

void AddComplexLaurentSeries(ComplexLaurentSeries& target,
                             const ComplexLaurentSeries& contribution,
                             const int max_order) {
  target.real = AddLaurentSeries(target.real, contribution.real, max_order);
  target.imaginary =
      AddLaurentSeries(target.imaginary, contribution.imaginary, max_order);
}

std::vector<SolverDiagnostics::EpsilonCoefficient> SerializeComplexLaurentSeries(
    const ComplexLaurentSeries& series,
    const int max_order) {
  int min_order = 0;
  bool found = false;
  for (const auto* component : {&series.real, &series.imaginary}) {
    for (const auto& [order, coefficient] : *component) {
      if (order <= max_order && !coefficient.IsZero()) {
        min_order = found ? std::min(min_order, order) : order;
        found = true;
      }
    }
  }

  std::vector<SolverDiagnostics::EpsilonCoefficient> coefficients;
  for (int order = min_order; order <= max_order; ++order) {
    coefficients.push_back({order,
                            LaurentCoefficient(series.real, order).ToString(),
                            LaurentCoefficient(series.imaginary, order).ToString()});
  }
  return coefficients;
}

const ParsedReductionRule& FindRuleForTarget(const ParsedReductionResult& reduction_result,
                                             const TargetIntegral& target) {
  const std::string target_label = target.Label();
  for (const auto& rule : reduction_result.rules) {
    if (rule.target.Label() == target_label) {
      return rule;
    }
  }
  throw std::invalid_argument("target reduction has no rule for requested target " +
                              target_label);
}

}  // namespace

std::vector<std::vector<SolverDiagnostics::EpsilonCoefficient>>
ApplyParsedTargetReductionToEpsilonCoefficients(
    const ParsedReductionResult& reduction_result,
    const std::vector<TargetIntegral>& requested_targets,
    const std::vector<MasterIntegral>& available_masters,
    const std::vector<std::vector<SolverDiagnostics::EpsilonCoefficient>>&
        available_master_coefficients,
    const std::string& dimension_expression,
    const int max_eps_order) {
  if (max_eps_order < 0) {
    throw std::invalid_argument("target reduction application requires max_eps_order >= 0");
  }
  if (available_masters.size() != available_master_coefficients.size()) {
    throw std::invalid_argument(
        "target reduction application requires one epsilon-coefficient vector per available "
        "master");
  }
  if (requested_targets.empty()) {
    throw std::invalid_argument("target reduction application requires at least one target");
  }

  std::map<std::string, ComplexLaurentSeries> available_by_label;
  for (std::size_t index = 0; index < available_masters.size(); ++index) {
    const std::string label = IntegralLabel(available_masters[index]);
    if (!available_by_label.emplace(
            label, ParseMasterEpsilonCoefficients(available_master_coefficients[index]))
             .second) {
      throw std::invalid_argument("target reduction application received duplicate available "
                                  "master " +
                                  label);
    }
  }

  constexpr int kTargetReductionLaurentGuardOrders = 8;
  const int dimension_series_order = max_eps_order + 2 * kTargetReductionLaurentGuardOrders;
  const LaurentSeries dimension_series =
      ParseLaurentExpression(dimension_expression, std::nullopt, dimension_series_order);

  std::vector<std::vector<SolverDiagnostics::EpsilonCoefficient>> reduced_targets;
  reduced_targets.reserve(requested_targets.size());
  for (const TargetIntegral& target : requested_targets) {
    const ParsedReductionRule& rule = FindRuleForTarget(reduction_result, target);
    ComplexLaurentSeries reduced;
    for (const ParsedReductionTerm& term : rule.terms) {
      const std::string master_label = term.master.Label();
      const auto master_it = available_by_label.find(master_label);
      if (master_it == available_by_label.end()) {
        throw std::invalid_argument(
            "target reduction rule for " + target.Label() +
            " references master without available epsilon coefficients: " + master_label);
      }

      const std::optional<int> master_min_order = MinimumOrder(master_it->second);
      if (!master_min_order.has_value()) {
        continue;
      }
      const int coefficient_order =
          std::max(max_eps_order, max_eps_order - *master_min_order) +
          kTargetReductionLaurentGuardOrders;
      const LaurentSeries coefficient_series =
          ParseLaurentExpression(term.coefficient, dimension_series, coefficient_order);
      AddComplexLaurentSeries(
          reduced,
          ScaleComplexLaurentSeries(master_it->second, coefficient_series, max_eps_order),
          max_eps_order);
    }
    reduced_targets.push_back(SerializeComplexLaurentSeries(reduced, max_eps_order));
  }

  return reduced_targets;
}

}  // namespace amflow
