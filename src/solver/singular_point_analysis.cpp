#include "amflow/solver/singular_point_analysis.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace amflow {

namespace {

constexpr char kUnsupportedSingularFormPrefix[] =
    "singular-point analysis encountered unsupported singular-form analysis: ";

ExactRational ZeroRational() {
  return {"0", "1"};
}

ExactRational OneRational() {
  return {"1", "1"};
}

std::string Parenthesize(const ExactRational& value) {
  return "(" + value.ToString() + ")";
}

std::string RationalKey(const ExactRational& value) {
  return value.numerator + "/" + value.denominator;
}

ExactRational EvaluateExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, NumericEvaluationPoint{});
}

ExactRational NegateRational(const ExactRational& value) {
  return EvaluateExactArithmetic("-" + Parenthesize(value));
}

ExactRational AddRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "+" + Parenthesize(rhs));
}

ExactRational MultiplyRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "*" + Parenthesize(rhs));
}

ExactRational DivideRational(const ExactRational& lhs, const ExactRational& rhs) {
  return EvaluateExactArithmetic(Parenthesize(lhs) + "/" + Parenthesize(rhs));
}

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(start, end - start);
}

bool HasDeclaredVariable(const DESystem& system, const std::string& variable_name) {
  return std::any_of(system.variables.begin(),
                     system.variables.end(),
                     [&variable_name](const DifferentiationVariable& variable) {
                       return variable.name == variable_name;
                     });
}

const std::vector<std::vector<std::string>>& ResolveSelectedMatrix(const DESystem& system,
                                                                   const std::string& variable_name) {
  const auto matrix_it = system.coefficient_matrices.find(variable_name);
  if (!HasDeclaredVariable(system, variable_name) || matrix_it == system.coefficient_matrices.end()) {
    throw std::invalid_argument("singular-point analysis requires a declared coefficient matrix "
                                "for variable \"" +
                                variable_name + "\"");
  }
  return matrix_it->second;
}

std::invalid_argument UnsupportedSingularForm(const std::string& expression,
                                              const std::string& detail) {
  return std::invalid_argument(std::string(kUnsupportedSingularFormPrefix) + detail + " in \"" +
                               expression + "\"");
}

enum class TokenKind {
  Identifier,
  Number,
  Plus,
  Minus,
  Star,
  Slash,
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
  std::size_t index = 0;
  while (index < expression.size()) {
    const char ch = expression[index];
    if (std::isspace(static_cast<unsigned char>(ch))) {
      ++index;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(ch))) {
      const std::size_t start = index;
      while (index < expression.size() &&
             std::isdigit(static_cast<unsigned char>(expression[index]))) {
        ++index;
      }
      tokens.push_back({TokenKind::Number, expression.substr(start, index - start)});
      continue;
    }

    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
      const std::size_t start = index;
      while (index < expression.size() &&
             (std::isalnum(static_cast<unsigned char>(expression[index])) ||
              expression[index] == '_')) {
        ++index;
      }
      tokens.push_back({TokenKind::Identifier, expression.substr(start, index - start)});
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
      case '(':
        tokens.push_back({TokenKind::LeftParen, "("});
        break;
      case ')':
        tokens.push_back({TokenKind::RightParen, ")"});
        break;
      default:
        throw std::invalid_argument(
            "singular-point analysis encountered malformed expression: unexpected character \"" +
            std::string(1, ch) + "\" in \"" + expression + "\"");
    }
    ++index;
  }
  tokens.push_back({TokenKind::End, ""});
  return tokens;
}

struct LinearTerm {
  ExactRational variable_coefficient = ZeroRational();
  ExactRational constant = ZeroRational();

  bool IsZero() const { return variable_coefficient.IsZero() && constant.IsZero(); }
  bool IsConstant() const { return variable_coefficient.IsZero(); }
};

LinearTerm ConstantTerm(const ExactRational& value) {
  return {ZeroRational(), value};
}

LinearTerm ActiveVariableTerm() {
  return {OneRational(), ZeroRational()};
}

LinearTerm AddLinear(const LinearTerm& lhs, const LinearTerm& rhs) {
  return {AddRational(lhs.variable_coefficient, rhs.variable_coefficient),
          AddRational(lhs.constant, rhs.constant)};
}

LinearTerm ScaleLinear(const LinearTerm& value, const ExactRational& scale) {
  return {MultiplyRational(value.variable_coefficient, scale),
          MultiplyRational(value.constant, scale)};
}

bool AreProportional(const LinearTerm& lhs, const LinearTerm& rhs) {
  if (lhs.IsZero() || rhs.IsZero()) {
    return false;
  }
  if (lhs.IsConstant() && rhs.IsConstant()) {
    return true;
  }
  return MultiplyRational(lhs.variable_coefficient, rhs.constant) ==
         MultiplyRational(rhs.variable_coefficient, lhs.constant);
}

ExactRational ProportionalScale(const LinearTerm& lhs, const LinearTerm& rhs) {
  if (!rhs.variable_coefficient.IsZero()) {
    return DivideRational(lhs.variable_coefficient, rhs.variable_coefficient);
  }
  return DivideRational(lhs.constant, rhs.constant);
}

bool SameLinearTerm(const LinearTerm& lhs, const LinearTerm& rhs) {
  return lhs.variable_coefficient == rhs.variable_coefficient && lhs.constant == rhs.constant;
}

std::string LinearTermKey(const LinearTerm& value) {
  return RationalKey(value.variable_coefficient) + "|" + RationalKey(value.constant);
}

LinearTerm MakeMonicFactor(const LinearTerm& factor) {
  return {OneRational(), DivideRational(factor.constant, factor.variable_coefficient)};
}

struct FactorizedTerm {
  ExactRational coefficient = OneRational();
  std::vector<LinearTerm> numerator_factors;
  std::vector<LinearTerm> denominator_factors;
};

struct FactorizedExpression {
  std::vector<FactorizedTerm> terms;
  bool is_simple_linear_difference = false;
};

struct SimplifiedTerm {
  LinearTerm numerator = ConstantTerm(ZeroRational());
  bool has_denominator = false;
  LinearTerm denominator = ConstantTerm(OneRational());
};

struct SimplifiedExpression {
  std::vector<SimplifiedTerm> terms;
};

std::string FactorizedTermKey(const FactorizedTerm& term);
std::vector<ExactRational> ExtractFinitePoleLocations(const FactorizedExpression& expression,
                                                      const std::string& source_expression);

bool SameFactorizedExpression(const FactorizedExpression& lhs, const FactorizedExpression& rhs) {
  if (lhs.terms.size() != rhs.terms.size()) {
    return false;
  }

  std::vector<std::pair<std::string, ExactRational>> lhs_entries;
  lhs_entries.reserve(lhs.terms.size());
  for (const auto& term : lhs.terms) {
    lhs_entries.push_back({FactorizedTermKey(term), term.coefficient});
  }

  std::vector<std::pair<std::string, ExactRational>> rhs_entries;
  rhs_entries.reserve(rhs.terms.size());
  for (const auto& term : rhs.terms) {
    rhs_entries.push_back({FactorizedTermKey(term), term.coefficient});
  }

  std::sort(lhs_entries.begin(),
            lhs_entries.end(),
            [](const auto& lhs_entry, const auto& rhs_entry) {
              return lhs_entry.first < rhs_entry.first;
            });
  std::sort(rhs_entries.begin(),
            rhs_entries.end(),
            [](const auto& lhs_entry, const auto& rhs_entry) {
              return lhs_entry.first < rhs_entry.first;
            });

  return lhs_entries == rhs_entries;
}

std::optional<FactorizedTerm> CanonicalizeFactorizedTerm(FactorizedTerm term,
                                                         const std::string& expression) {
  if (term.coefficient.IsZero()) {
    return std::nullopt;
  }

  std::vector<LinearTerm> numerator_factors;
  numerator_factors.reserve(term.numerator_factors.size());
  for (const auto& factor : term.numerator_factors) {
    if (factor.IsZero()) {
      return std::nullopt;
    }
    if (factor.IsConstant()) {
      term.coefficient = MultiplyRational(term.coefficient, factor.constant);
      if (term.coefficient.IsZero()) {
        return std::nullopt;
      }
      continue;
    }
    term.coefficient = MultiplyRational(term.coefficient, factor.variable_coefficient);
    numerator_factors.push_back(MakeMonicFactor(factor));
  }

  std::vector<LinearTerm> denominator_factors;
  denominator_factors.reserve(term.denominator_factors.size());
  for (const auto& factor : term.denominator_factors) {
    if (factor.IsZero()) {
      throw std::runtime_error("singular-point analysis encountered division by zero in \"" +
                               expression + "\"");
    }
    if (factor.IsConstant()) {
      term.coefficient = DivideRational(term.coefficient, factor.constant);
      continue;
    }
    term.coefficient = DivideRational(term.coefficient, factor.variable_coefficient);
    denominator_factors.push_back(MakeMonicFactor(factor));
  }

  if (term.coefficient.IsZero()) {
    return std::nullopt;
  }

  std::sort(numerator_factors.begin(),
            numerator_factors.end(),
            [](const LinearTerm& lhs, const LinearTerm& rhs) {
              return LinearTermKey(lhs) < LinearTermKey(rhs);
            });
  std::sort(denominator_factors.begin(),
            denominator_factors.end(),
            [](const LinearTerm& lhs, const LinearTerm& rhs) {
              return LinearTermKey(lhs) < LinearTermKey(rhs);
            });

  std::vector<LinearTerm> reduced_numerator;
  std::vector<LinearTerm> reduced_denominator;
  std::size_t numerator_index = 0;
  std::size_t denominator_index = 0;
  while (numerator_index < numerator_factors.size() &&
         denominator_index < denominator_factors.size()) {
    const std::string numerator_key = LinearTermKey(numerator_factors[numerator_index]);
    const std::string denominator_key = LinearTermKey(denominator_factors[denominator_index]);
    if (numerator_key == denominator_key) {
      ++numerator_index;
      ++denominator_index;
      continue;
    }
    if (numerator_key < denominator_key) {
      reduced_numerator.push_back(numerator_factors[numerator_index]);
      ++numerator_index;
      continue;
    }
    reduced_denominator.push_back(denominator_factors[denominator_index]);
    ++denominator_index;
  }
  while (numerator_index < numerator_factors.size()) {
    reduced_numerator.push_back(numerator_factors[numerator_index]);
    ++numerator_index;
  }
  while (denominator_index < denominator_factors.size()) {
    reduced_denominator.push_back(denominator_factors[denominator_index]);
    ++denominator_index;
  }

  term.numerator_factors = std::move(reduced_numerator);
  term.denominator_factors = std::move(reduced_denominator);
  return term;
}

std::string FactorizedTermKey(const FactorizedTerm& term) {
  std::string key;
  key.reserve(64);
  key += "N:";
  for (const auto& factor : term.numerator_factors) {
    key += LinearTermKey(factor);
    key += ";";
  }
  key += "|D:";
  for (const auto& factor : term.denominator_factors) {
    key += LinearTermKey(factor);
    key += ";";
  }
  return key;
}

FactorizedExpression CanonicalizeAndCombineTerms(const FactorizedExpression& expression,
                                                 const std::string& source_expression) {
  FactorizedExpression combined;
  std::vector<std::string> keys;

  for (const auto& term : expression.terms) {
    const std::optional<FactorizedTerm> maybe_term =
        CanonicalizeFactorizedTerm(term, source_expression);
    if (!maybe_term.has_value()) {
      continue;
    }

    const std::string key = FactorizedTermKey(*maybe_term);
    const auto existing_it = std::find(keys.begin(), keys.end(), key);
    if (existing_it == keys.end()) {
      keys.push_back(key);
      combined.terms.push_back(*maybe_term);
      continue;
    }

    const std::size_t index =
        static_cast<std::size_t>(std::distance(keys.begin(), existing_it));
    combined.terms[index].coefficient =
        AddRational(combined.terms[index].coefficient, maybe_term->coefficient);
  }

  combined.terms.erase(std::remove_if(combined.terms.begin(),
                                      combined.terms.end(),
                                      [](const FactorizedTerm& term) {
                                        return term.coefficient.IsZero();
                                      }),
                       combined.terms.end());
  return combined;
}

FactorizedExpression ConstantExpression(const ExactRational& value) {
  FactorizedExpression expression;
  if (!value.IsZero()) {
    expression.terms.push_back({value, {}, {}});
  }
  return expression;
}

FactorizedExpression ActiveVariableExpression() {
  FactorizedExpression expression;
  expression.terms.push_back({OneRational(), {ActiveVariableTerm()}, {}});
  return expression;
}

FactorizedExpression NegateExpression(FactorizedExpression value) {
  for (auto& term : value.terms) {
    term.coefficient = NegateRational(term.coefficient);
  }
  return value;
}

FactorizedExpression AddExpressions(FactorizedExpression lhs, const FactorizedExpression& rhs) {
  lhs.terms.insert(lhs.terms.end(), rhs.terms.begin(), rhs.terms.end());
  lhs.is_simple_linear_difference = false;
  return lhs;
}

FactorizedExpression CollapseMultiplicativeOperand(FactorizedExpression value,
                                                   const std::string& expression);

FactorizedExpression MultiplyExpressions(FactorizedExpression lhs,
                                         FactorizedExpression rhs,
                                         const std::string& expression) {
  if (lhs.terms.empty() || rhs.terms.empty()) {
    return {};
  }

  lhs = CollapseMultiplicativeOperand(std::move(lhs), expression);
  rhs = CollapseMultiplicativeOperand(std::move(rhs), expression);
  if (lhs.terms.empty() || rhs.terms.empty()) {
    return {};
  }

  FactorizedExpression result;
  for (const auto& lhs_term : lhs.terms) {
    for (const auto& rhs_term : rhs.terms) {
      FactorizedTerm product;
      product.coefficient = MultiplyRational(lhs_term.coefficient, rhs_term.coefficient);
      product.numerator_factors = lhs_term.numerator_factors;
      product.numerator_factors.insert(product.numerator_factors.end(),
                                       rhs_term.numerator_factors.begin(),
                                       rhs_term.numerator_factors.end());
      product.denominator_factors = lhs_term.denominator_factors;
      product.denominator_factors.insert(product.denominator_factors.end(),
                                         rhs_term.denominator_factors.begin(),
                                         rhs_term.denominator_factors.end());
      result.terms.push_back(std::move(product));
    }
  }
  result.is_simple_linear_difference = false;
  return result;
}

std::optional<SimplifiedTerm> SimplifyFactorizedTerm(FactorizedTerm term,
                                                     const std::string& expression) {
  const std::optional<FactorizedTerm> canonical = CanonicalizeFactorizedTerm(term, expression);
  if (!canonical.has_value()) {
    return std::nullopt;
  }

  term = *canonical;

  if (term.numerator_factors.size() > 1) {
    throw UnsupportedSingularForm(expression, "nonlinear product exceeds Batch 35 scope");
  }
  if (term.denominator_factors.size() > 1) {
    for (std::size_t lhs_index = 0; lhs_index < term.denominator_factors.size(); ++lhs_index) {
      for (std::size_t rhs_index = lhs_index + 1; rhs_index < term.denominator_factors.size();
           ++rhs_index) {
        if (SameLinearTerm(term.denominator_factors[lhs_index],
                           term.denominator_factors[rhs_index])) {
          throw UnsupportedSingularForm(expression, "higher-order pole exceeds Batch 35 scope");
        }
      }
    }
    throw UnsupportedSingularForm(expression,
                                  "multiple surviving singular factors exceed Batch 35 scope");
  }

  LinearTerm numerator = ConstantTerm(term.coefficient);
  if (!term.numerator_factors.empty()) {
    numerator = ScaleLinear(term.numerator_factors.front(), term.coefficient);
  }
  if (numerator.IsZero()) {
    return std::nullopt;
  }

  if (term.denominator_factors.empty()) {
    return SimplifiedTerm{numerator, false, ConstantTerm(OneRational())};
  }
  if (AreProportional(numerator, term.denominator_factors.front())) {
    return SimplifiedTerm{ConstantTerm(ProportionalScale(numerator, term.denominator_factors.front())),
                          false,
                          ConstantTerm(OneRational())};
  }
  return SimplifiedTerm{numerator, true, term.denominator_factors.front()};
}

SimplifiedExpression NormalizeSimplifiedExpression(SimplifiedExpression expression) {
  LinearTerm regular_sum = ConstantTerm(ZeroRational());
  struct SingularGroup {
    LinearTerm denominator = ConstantTerm(OneRational());
    LinearTerm numerator_sum = ConstantTerm(ZeroRational());
  };
  std::vector<SingularGroup> singular_groups;

  for (const auto& term : expression.terms) {
    if (term.numerator.IsZero()) {
      continue;
    }
    if (!term.has_denominator) {
      regular_sum = AddLinear(regular_sum, term.numerator);
      continue;
    }

    const auto group_it = std::find_if(singular_groups.begin(),
                                       singular_groups.end(),
                                       [&term](const SingularGroup& group) {
                                         return SameLinearTerm(group.denominator, term.denominator);
                                       });
    if (group_it == singular_groups.end()) {
      singular_groups.push_back({term.denominator, term.numerator});
      continue;
    }
    group_it->numerator_sum = AddLinear(group_it->numerator_sum, term.numerator);
  }

  SimplifiedExpression normalized;
  for (const auto& group : singular_groups) {
    if (group.numerator_sum.IsZero()) {
      continue;
    }
    if (AreProportional(group.numerator_sum, group.denominator)) {
      regular_sum = AddLinear(regular_sum,
                              ConstantTerm(ProportionalScale(group.numerator_sum,
                                                             group.denominator)));
      continue;
    }
    normalized.terms.push_back({group.numerator_sum, true, group.denominator});
  }

  if (!regular_sum.IsZero()) {
    normalized.terms.insert(normalized.terms.begin(),
                            {regular_sum, false, ConstantTerm(OneRational())});
  }
  return normalized;
}

SimplifiedExpression SimplifyExpression(const FactorizedExpression& expression,
                                        const std::string& source_expression) {
  const FactorizedExpression canonicalized =
      CanonicalizeAndCombineTerms(expression, source_expression);
  SimplifiedExpression simplified;
  for (const auto& term : canonicalized.terms) {
    const std::optional<SimplifiedTerm> maybe_term =
        SimplifyFactorizedTerm(term, source_expression);
    if (maybe_term.has_value()) {
      simplified.terms.push_back(*maybe_term);
    }
  }
  return NormalizeSimplifiedExpression(std::move(simplified));
}

FactorizedExpression ToFactorizedExpression(const SimplifiedExpression& expression) {
  FactorizedExpression factorized;
  for (const auto& term : expression.terms) {
    if (term.numerator.IsZero()) {
      continue;
    }

    FactorizedTerm factorized_term;
    if (term.numerator.IsConstant()) {
      factorized_term.coefficient = term.numerator.constant;
    } else {
      factorized_term.coefficient = OneRational();
      factorized_term.numerator_factors.push_back(term.numerator);
    }
    if (term.has_denominator) {
      factorized_term.denominator_factors.push_back(term.denominator);
    }
    factorized.terms.push_back(std::move(factorized_term));
  }
  factorized.is_simple_linear_difference = false;
  return factorized;
}

bool IsSingleActiveVariableTerm(const FactorizedExpression& expression) {
  return expression.terms.size() == 1 && expression.terms.front().coefficient == OneRational() &&
         expression.terms.front().numerator_factors.size() == 1 &&
         SameLinearTerm(expression.terms.front().numerator_factors.front(), ActiveVariableTerm()) &&
         expression.terms.front().denominator_factors.empty();
}

bool IsConstantOnlyExpression(const FactorizedExpression& expression) {
  return std::all_of(expression.terms.begin(),
                     expression.terms.end(),
                     [](const FactorizedTerm& term) {
                       return term.denominator_factors.empty() && term.numerator_factors.empty();
                     });
}

FactorizedExpression CollapseMultiplicativeOperand(FactorizedExpression value,
                                                   const std::string& expression) {
  if (value.terms.size() <= 1) {
    return value;
  }

  try {
    const SimplifiedExpression simplified = SimplifyExpression(value, expression);
    if (simplified.terms.size() <= 1) {
      return ToFactorizedExpression(simplified);
    }
  } catch (const std::invalid_argument&) {
    return value;
  }

  return value;
}

FactorizedExpression DivideExpressions(FactorizedExpression lhs,
                                       FactorizedExpression rhs,
                                       const std::string& expression) {
  const bool rhs_was_multi_term = rhs.terms.size() != 1;
  const bool rhs_is_simple_linear_difference = rhs.is_simple_linear_difference;
  lhs = CanonicalizeAndCombineTerms(lhs, expression);
  rhs = CanonicalizeAndCombineTerms(rhs, expression);

  if (rhs.terms.empty()) {
    throw std::runtime_error("singular-point analysis encountered division by zero in \"" +
                             expression + "\"");
  }
  if (SameFactorizedExpression(lhs, rhs)) {
    if (rhs_was_multi_term) {
      const SimplifiedExpression simplified_shared = SimplifyExpression(rhs, expression);
      if (simplified_shared.terms.empty()) {
        throw std::runtime_error("singular-point analysis encountered division by zero in \"" +
                                 expression + "\"");
      }
    }
    static_cast<void>(ExtractFinitePoleLocations(rhs, expression));
    return ConstantExpression(OneRational());
  }
  if (lhs.terms.empty()) {
    if (rhs_was_multi_term) {
      const SimplifiedExpression simplified_divisor = SimplifyExpression(rhs, expression);
      if (simplified_divisor.terms.empty()) {
        throw std::runtime_error("singular-point analysis encountered division by zero in \"" +
                                 expression + "\"");
      }
    }
    return {};
  }
  if (rhs_was_multi_term) {
    const SimplifiedExpression simplified_divisor = SimplifyExpression(rhs, expression);
    if (simplified_divisor.terms.empty()) {
      throw std::runtime_error("singular-point analysis encountered division by zero in \"" +
                               expression + "\"");
    }
    if (rhs_is_simple_linear_difference) {
      rhs = ToFactorizedExpression(simplified_divisor);
    } else {
      throw UnsupportedSingularForm(expression,
                                    "division by a multi-term expression exceeds Batch 35 scope");
    }
  }
  if (rhs.terms.size() != 1) {
    throw UnsupportedSingularForm(expression,
                                  "division by a multi-term expression exceeds Batch 35 scope");
  }

  const FactorizedTerm& divisor = rhs.terms.front();
  FactorizedExpression result;
  for (const auto& lhs_term : lhs.terms) {
    FactorizedTerm quotient;
    quotient.coefficient = DivideRational(lhs_term.coefficient, divisor.coefficient);
    quotient.numerator_factors = lhs_term.numerator_factors;
    quotient.numerator_factors.insert(quotient.numerator_factors.end(),
                                      divisor.denominator_factors.begin(),
                                      divisor.denominator_factors.end());
    quotient.denominator_factors = lhs_term.denominator_factors;
    quotient.denominator_factors.insert(quotient.denominator_factors.end(),
                                        divisor.numerator_factors.begin(),
                                        divisor.numerator_factors.end());
    result.terms.push_back(std::move(quotient));
  }
  return result;
}

class SingularExpressionParser {
 public:
  SingularExpressionParser(std::string expression,
                           std::string active_variable,
                           NumericEvaluationPoint passive_bindings)
      : expression_(std::move(expression)),
        active_variable_(std::move(active_variable)),
        passive_bindings_(std::move(passive_bindings)),
        tokens_(Tokenize(expression_)) {}

  FactorizedExpression Parse() {
    FactorizedExpression value = ParseExpression();
    if (Current().kind != TokenKind::End) {
      throw Malformed("unexpected trailing token \"" + Current().text + "\"");
    }
    return value;
  }

 private:
  const Token& Current() const { return tokens_[position_]; }

  const Token& Advance() {
    const Token& token = Current();
    if (position_ < tokens_.size()) {
      ++position_;
    }
    return token;
  }

  bool Match(const TokenKind kind) {
    if (Current().kind != kind) {
      return false;
    }
    Advance();
    return true;
  }

  std::invalid_argument Malformed(const std::string& detail) const {
    return std::invalid_argument("singular-point analysis encountered malformed expression: " +
                                 detail + " in \"" + expression_ + "\"");
  }

  bool StartsDirectConstantAtomTerm() const {
    if (Current().kind == TokenKind::Number) {
      return tokens_[position_ + 1].kind != TokenKind::Star &&
             tokens_[position_ + 1].kind != TokenKind::Slash;
    }
    if (Current().kind == TokenKind::Identifier && Current().text != active_variable_) {
      return tokens_[position_ + 1].kind != TokenKind::Star &&
             tokens_[position_ + 1].kind != TokenKind::Slash;
    }
    return false;
  }

  FactorizedExpression ParseExpression() {
    FactorizedExpression value = ParseTerm();
    const bool first_term_is_active_variable = IsSingleActiveVariableTerm(value);
    bool saw_plus = false;
    bool saw_single_direct_minus_constant = false;
    int binary_operator_count = 0;
    while (true) {
      if (Match(TokenKind::Plus)) {
        saw_plus = true;
        ++binary_operator_count;
        value = AddExpressions(std::move(value), ParseTerm());
        continue;
      }
      if (Match(TokenKind::Minus)) {
        ++binary_operator_count;
        const bool is_direct_minus_constant =
            binary_operator_count == 1 && StartsDirectConstantAtomTerm();
        FactorizedExpression rhs = ParseTerm();
        if (is_direct_minus_constant && IsConstantOnlyExpression(rhs)) {
          saw_single_direct_minus_constant = true;
        }
        value = AddExpressions(std::move(value), NegateExpression(std::move(rhs)));
        continue;
      }
      break;
    }
    value.is_simple_linear_difference =
        first_term_is_active_variable && !saw_plus && binary_operator_count == 1 &&
        saw_single_direct_minus_constant;
    return value;
  }

  FactorizedExpression ParseTerm() {
    FactorizedExpression value = ParseUnary();
    while (true) {
      if (Match(TokenKind::Star)) {
        value = MultiplyExpressions(std::move(value), ParseUnary(), expression_);
        continue;
      }
      if (Match(TokenKind::Slash)) {
        value = DivideExpressions(std::move(value), ParseUnary(), expression_);
        continue;
      }
      break;
    }
    return value;
  }

  FactorizedExpression ParseUnary() {
    if (Match(TokenKind::Plus)) {
      return ParseUnary();
    }
    if (Match(TokenKind::Minus)) {
      return NegateExpression(ParseUnary());
    }
    return ParsePrimary();
  }

  FactorizedExpression ParsePrimary() {
    if (Current().kind == TokenKind::Number) {
      const Token token = Advance();
      return ConstantExpression(
          EvaluateCoefficientExpression(token.text, NumericEvaluationPoint{}));
    }

    if (Current().kind == TokenKind::Identifier) {
      const Token token = Advance();
      if (token.text == active_variable_) {
        return ActiveVariableExpression();
      }
      return ConstantExpression(EvaluateCoefficientExpression(token.text, passive_bindings_));
    }

    if (Match(TokenKind::LeftParen)) {
      FactorizedExpression value = ParseExpression();
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
  std::string active_variable_;
  NumericEvaluationPoint passive_bindings_;
  std::vector<Token> tokens_;
  std::size_t position_ = 0;
};

FactorizedExpression AnalyzeExpression(const std::string& expression,
                                       const std::string& variable_name,
                                       const NumericEvaluationPoint& passive_bindings) {
  const FactorizedExpression factorized =
      SingularExpressionParser(expression, variable_name, passive_bindings).Parse();
  return CanonicalizeAndCombineTerms(factorized, expression);
}

ExactRational ParsePointValue(const std::string& variable_name,
                              const std::string& point_expression,
                              const NumericEvaluationPoint& passive_bindings) {
  const std::string trimmed = Trim(point_expression);
  if (trimmed.empty()) {
    throw std::invalid_argument(
        "singular-point analysis encountered malformed point expression: empty input");
  }

  const std::size_t separator = trimmed.find('=');
  if (separator == std::string::npos) {
    return EvaluateCoefficientExpression(trimmed, passive_bindings);
  }

  if (trimmed.find('=', separator + 1) != std::string::npos) {
    throw std::invalid_argument("singular-point analysis encountered malformed point expression: \"" +
                                point_expression + "\"");
  }

  const std::string lhs = Trim(trimmed.substr(0, separator));
  const std::string rhs = Trim(trimmed.substr(separator + 1));
  if (lhs != variable_name || rhs.empty()) {
    throw std::invalid_argument("singular-point analysis encountered malformed point expression: \"" +
                                point_expression + "\"");
  }

  return EvaluateCoefficientExpression(rhs, passive_bindings);
}

bool ContainsLocation(const std::vector<FiniteSingularPoint>& points,
                      const ExactRational& location) {
  return std::any_of(points.begin(), points.end(), [&location](const FiniteSingularPoint& point) {
    return point.location == location;
  });
}

std::optional<LinearTerm> LinearNumeratorOfTerm(const FactorizedTerm& term) {
  if (term.numerator_factors.empty()) {
    return ConstantTerm(term.coefficient);
  }
  if (term.numerator_factors.size() == 1) {
    return ScaleLinear(term.numerator_factors.front(), term.coefficient);
  }
  return std::nullopt;
}

std::vector<ExactRational> ExtractFinitePoleLocations(const FactorizedExpression& expression,
                                                      const std::string& source_expression) {
  struct PoleSupportGroup {
    LinearTerm denominator = ConstantTerm(OneRational());
    LinearTerm linear_numerator_sum = ConstantTerm(ZeroRational());
    bool has_nonlinear_numerator = false;
    std::size_t surviving_term_count = 0;
  };

  std::vector<PoleSupportGroup> groups;
  for (const auto& term : expression.terms) {
    if (term.denominator_factors.empty()) {
      continue;
    }

    if (term.denominator_factors.size() > 1) {
      for (std::size_t lhs_index = 0; lhs_index < term.denominator_factors.size(); ++lhs_index) {
        for (std::size_t rhs_index = lhs_index + 1; rhs_index < term.denominator_factors.size();
             ++rhs_index) {
          if (SameLinearTerm(term.denominator_factors[lhs_index],
                             term.denominator_factors[rhs_index])) {
            throw UnsupportedSingularForm(source_expression,
                                          "higher-order pole exceeds Batch 35 scope");
          }
        }
      }
      throw UnsupportedSingularForm(source_expression,
                                    "multiple surviving singular factors exceed Batch 35 scope");
    }

    const LinearTerm& denominator = term.denominator_factors.front();
    std::size_t group_index = 0;
    while (group_index < groups.size() &&
           !SameLinearTerm(groups[group_index].denominator, denominator)) {
      ++group_index;
    }
    if (group_index == groups.size()) {
      groups.push_back({denominator, ConstantTerm(ZeroRational()), false, 0});
    }
    PoleSupportGroup& group = groups[group_index];
    ++group.surviving_term_count;

    const std::optional<LinearTerm> linear_numerator = LinearNumeratorOfTerm(term);
    if (!linear_numerator.has_value()) {
      group.has_nonlinear_numerator = true;
      continue;
    }
    group.linear_numerator_sum = AddLinear(group.linear_numerator_sum, *linear_numerator);
  }

  std::vector<ExactRational> locations;
  for (const auto& group : groups) {
    if (group.has_nonlinear_numerator && group.surviving_term_count > 1) {
      throw UnsupportedSingularForm(source_expression,
                                    "grouped nonlinear numerators exceed Batch 35 scope");
    }
    const bool singular_support_survives =
        group.has_nonlinear_numerator ||
        (!group.linear_numerator_sum.IsZero() &&
         !AreProportional(group.linear_numerator_sum, group.denominator));
    if (!singular_support_survives) {
      continue;
    }

    locations.push_back(
        DivideRational(NegateRational(group.denominator.constant),
                       group.denominator.variable_coefficient));
  }
  return locations;
}

}  // namespace

std::vector<FiniteSingularPoint> DetectFiniteSingularPoints(
    const DESystem& system,
    const std::string& variable_name,
    const NumericEvaluationPoint& passive_bindings) {
  const auto& matrix = ResolveSelectedMatrix(system, variable_name);

  std::vector<FiniteSingularPoint> points;
  for (const auto& row : matrix) {
    for (const auto& cell : row) {
      const FactorizedExpression analyzed = AnalyzeExpression(cell, variable_name, passive_bindings);
      for (const ExactRational& location : ExtractFinitePoleLocations(analyzed, cell)) {
        if (!ContainsLocation(points, location)) {
          points.push_back({location});
        }
      }
    }
  }

  return points;
}

PointClassification ClassifyFinitePoint(const DESystem& system,
                                        const std::string& variable_name,
                                        const std::string& point_expression,
                                        const NumericEvaluationPoint& passive_bindings) {
  const auto& matrix = ResolveSelectedMatrix(system, variable_name);
  const ExactRational point_value = ParsePointValue(variable_name, point_expression, passive_bindings);
  bool found_singular = false;

  for (const auto& row : matrix) {
    for (const auto& cell : row) {
      const FactorizedExpression analyzed = AnalyzeExpression(cell, variable_name, passive_bindings);
      for (const ExactRational& location : ExtractFinitePoleLocations(analyzed, cell)) {
        if (location == point_value) {
          found_singular = true;
        }
      }
    }
  }

  return found_singular ? PointClassification::Singular : PointClassification::Regular;
}

}  // namespace amflow
