#include "amflow/de/invariant_derivative_generation.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace amflow {

namespace {

struct Rational {
  long long numerator = 0;
  long long denominator = 1;

  Rational() = default;
  explicit Rational(const long long value) : numerator(value), denominator(1) {}
  Rational(long long numerator_value, long long denominator_value)
      : numerator(numerator_value), denominator(denominator_value) {
    Normalize();
  }

  void Normalize() {
    if (denominator == 0) {
      throw std::runtime_error("automatic invariant seed construction encountered division by "
                               "zero");
    }
    if (denominator < 0) {
      numerator = -numerator;
      denominator = -denominator;
    }
    const long long divisor = std::gcd(std::llabs(numerator), std::llabs(denominator));
    if (divisor > 1) {
      numerator /= divisor;
      denominator /= divisor;
    }
  }

  bool IsZero() const {
    return numerator == 0;
  }

  bool IsOne() const {
    return numerator == denominator;
  }

  std::string ToString() const {
    if (denominator == 1) {
      return std::to_string(numerator);
    }
    return std::to_string(numerator) + "/" + std::to_string(denominator);
  }

  Rational Reciprocal() const {
    return Rational(denominator, numerator);
  }
};

Rational operator+(const Rational& left, const Rational& right) {
  return Rational(left.numerator * right.denominator + right.numerator * left.denominator,
                  left.denominator * right.denominator);
}

Rational operator*(const Rational& left, const Rational& right) {
  return Rational(left.numerator * right.numerator, left.denominator * right.denominator);
}

enum class ExprKind {
  Number,
  Symbol,
  Add,
  Multiply,
  MomentumSquare,
};

struct Expr {
  ExprKind kind = ExprKind::Number;
  Rational number;
  std::string symbol;
  std::vector<Expr> terms;
  std::map<std::string, int> momentum_terms;
};

Expr MakeNumber(const Rational& value) {
  Expr expr;
  expr.kind = ExprKind::Number;
  expr.number = value;
  return expr;
}

Expr MakeSymbol(std::string value) {
  Expr expr;
  expr.kind = ExprKind::Symbol;
  expr.symbol = std::move(value);
  return expr;
}

Expr MakeMomentumSquare(std::map<std::string, int> momentum_terms) {
  for (auto it = momentum_terms.begin(); it != momentum_terms.end();) {
    if (it->second == 0) {
      it = momentum_terms.erase(it);
    } else {
      ++it;
    }
  }
  if (!momentum_terms.empty() && momentum_terms.begin()->second < 0) {
    for (auto& [_, multiplier] : momentum_terms) {
      multiplier = -multiplier;
    }
  }

  Expr expr;
  expr.kind = ExprKind::MomentumSquare;
  expr.momentum_terms = std::move(momentum_terms);
  return expr;
}

bool IsZero(const Expr& expr) {
  return expr.kind == ExprKind::Number && expr.number.IsZero();
}

Expr MakeAdd(std::vector<Expr> terms) {
  std::vector<Expr> flattened;
  Rational numeric_total(0);
  for (auto& term : terms) {
    if (term.kind == ExprKind::Add) {
      for (auto& nested : term.terms) {
        flattened.push_back(std::move(nested));
      }
      continue;
    }
    if (term.kind == ExprKind::Number) {
      numeric_total = numeric_total + term.number;
      continue;
    }
    if (IsZero(term)) {
      continue;
    }
    flattened.push_back(std::move(term));
  }

  if (!numeric_total.IsZero()) {
    flattened.push_back(MakeNumber(numeric_total));
  }
  if (flattened.empty()) {
    return MakeNumber(Rational(0));
  }
  if (flattened.size() == 1) {
    return std::move(flattened.front());
  }

  Expr expr;
  expr.kind = ExprKind::Add;
  expr.terms = std::move(flattened);
  return expr;
}

Expr MakeMultiply(std::vector<Expr> terms) {
  std::vector<Expr> flattened;
  Rational numeric_total(1);
  for (auto& term : terms) {
    if (term.kind == ExprKind::Multiply) {
      for (auto& nested : term.terms) {
        flattened.push_back(std::move(nested));
      }
      continue;
    }
    if (term.kind == ExprKind::Number) {
      numeric_total = numeric_total * term.number;
      continue;
    }
    if (IsZero(term)) {
      return MakeNumber(Rational(0));
    }
    flattened.push_back(std::move(term));
  }

  if (numeric_total.IsZero()) {
    return MakeNumber(Rational(0));
  }
  if (!numeric_total.IsOne()) {
    flattened.insert(flattened.begin(), MakeNumber(numeric_total));
  }
  if (flattened.empty()) {
    return MakeNumber(Rational(1));
  }
  if (flattened.size() == 1) {
    return std::move(flattened.front());
  }

  Expr expr;
  expr.kind = ExprKind::Multiply;
  expr.terms = std::move(flattened);
  return expr;
}

bool ExprEquals(const Expr& left, const Expr& right) {
  if (left.kind != right.kind) {
    return false;
  }
  switch (left.kind) {
    case ExprKind::Number:
      return left.number.numerator == right.number.numerator &&
             left.number.denominator == right.number.denominator;
    case ExprKind::Symbol:
      return left.symbol == right.symbol;
    case ExprKind::MomentumSquare:
      return left.momentum_terms == right.momentum_terms;
    case ExprKind::Add:
    case ExprKind::Multiply:
      if (left.terms.size() != right.terms.size()) {
        return false;
      }
      for (std::size_t index = 0; index < left.terms.size(); ++index) {
        if (!ExprEquals(left.terms[index], right.terms[index])) {
          return false;
        }
      }
      return true;
  }
  return false;
}

std::string DescribeMomentumSquare(const std::map<std::string, int>& momentum_terms) {
  std::ostringstream out;
  out << "(";
  bool first = true;
  for (const auto& [symbol, multiplier] : momentum_terms) {
    if (multiplier == 0) {
      continue;
    }
    const int magnitude = std::abs(multiplier);
    if (first) {
      if (multiplier < 0) {
        out << "-";
      }
    } else {
      out << (multiplier < 0 ? "-" : "+");
    }
    if (magnitude != 1) {
      out << magnitude << "*";
    }
    out << symbol;
    first = false;
  }
  if (first) {
    out << "0";
  }
  out << ")^2";
  return out.str();
}

std::string DescribeExpr(const Expr& expr) {
  switch (expr.kind) {
    case ExprKind::Number:
      return expr.number.ToString();
    case ExprKind::Symbol:
      return expr.symbol;
    case ExprKind::MomentumSquare:
      return DescribeMomentumSquare(expr.momentum_terms);
    case ExprKind::Add: {
      std::ostringstream out;
      for (std::size_t index = 0; index < expr.terms.size(); ++index) {
        if (index > 0) {
          out << " + ";
        }
        out << DescribeExpr(expr.terms[index]);
      }
      return out.str();
    }
    case ExprKind::Multiply: {
      std::ostringstream out;
      for (std::size_t index = 0; index < expr.terms.size(); ++index) {
        if (index > 0) {
          out << "*";
        }
        out << "(" << DescribeExpr(expr.terms[index]) << ")";
      }
      return out.str();
    }
  }
  return "";
}

enum class TokenKind {
  Identifier,
  Integer,
  Plus,
  Minus,
  Star,
  Slash,
  LParen,
  RParen,
  Caret,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
};

std::vector<Token> Tokenize(const std::string& input) {
  std::vector<Token> tokens;
  for (std::size_t index = 0; index < input.size();) {
    const unsigned char ch = static_cast<unsigned char>(input[index]);
    if (std::isspace(ch) != 0) {
      ++index;
      continue;
    }
    if (std::isalpha(ch) != 0 || input[index] == '_') {
      std::size_t end = index + 1;
      while (end < input.size()) {
        const unsigned char next = static_cast<unsigned char>(input[end]);
        if (std::isalnum(next) == 0 && input[end] != '_') {
          break;
        }
        ++end;
      }
      tokens.push_back({TokenKind::Identifier, input.substr(index, end - index)});
      index = end;
      continue;
    }
    if (std::isdigit(ch) != 0) {
      std::size_t end = index + 1;
      while (end < input.size() &&
             std::isdigit(static_cast<unsigned char>(input[end])) != 0) {
        ++end;
      }
      tokens.push_back({TokenKind::Integer, input.substr(index, end - index)});
      index = end;
      continue;
    }

    Token token;
    token.text = input.substr(index, 1);
    switch (input[index]) {
      case '+':
        token.kind = TokenKind::Plus;
        break;
      case '-':
        token.kind = TokenKind::Minus;
        break;
      case '*':
        token.kind = TokenKind::Star;
        break;
      case '/':
        token.kind = TokenKind::Slash;
        break;
      case '(':
        token.kind = TokenKind::LParen;
        break;
      case ')':
        token.kind = TokenKind::RParen;
        break;
      case '^':
        token.kind = TokenKind::Caret;
        break;
      default:
        throw std::runtime_error("automatic invariant seed construction encountered unsupported "
                                 "character in expression: " +
                                 token.text);
    }
    tokens.push_back(std::move(token));
    ++index;
  }
  tokens.push_back({TokenKind::End, ""});
  return tokens;
}

std::optional<std::map<std::string, int>> ExtractLinearMomentum(const Expr& expr);

class ExprParser {
 public:
  explicit ExprParser(const std::string& input)
      : source_(input), tokens_(Tokenize(input)) {}

  Expr Parse() {
    Expr expr = ParseExpression();
    Expect(TokenKind::End, "trailing tokens");
    return expr;
  }

 private:
  Expr ParseExpression() {
    Expr expr = ParseProduct();
    while (Match(TokenKind::Plus) || Match(TokenKind::Minus)) {
      const TokenKind op = Previous().kind;
      Expr right = ParseProduct();
      if (op == TokenKind::Plus) {
        expr = MakeAdd({std::move(expr), std::move(right)});
      } else {
        expr = MakeAdd(
            {std::move(expr), MakeMultiply({MakeNumber(Rational(-1)), std::move(right)})});
      }
    }
    return expr;
  }

  Expr ParseProduct() {
    Expr expr = ParseUnary();
    while (Match(TokenKind::Star) || Match(TokenKind::Slash)) {
      const TokenKind op = Previous().kind;
      Expr right = ParseUnary();
      if (op == TokenKind::Star) {
        expr = MakeMultiply({std::move(expr), std::move(right)});
        continue;
      }
      if (right.kind != ExprKind::Number) {
        throw std::runtime_error("automatic invariant seed construction supports division by "
                                 "numeric constants only");
      }
      expr = MakeMultiply({std::move(expr), MakeNumber(right.number.Reciprocal())});
    }
    return expr;
  }

  Expr ParseUnary() {
    if (Match(TokenKind::Plus)) {
      return ParseUnary();
    }
    if (Match(TokenKind::Minus)) {
      return MakeMultiply({MakeNumber(Rational(-1)), ParseUnary()});
    }
    return ParsePrimary();
  }

  Expr ParsePrimary() {
    if (Match(TokenKind::Integer)) {
      return MakeNumber(Rational(std::stoll(Previous().text)));
    }
    if (Match(TokenKind::Identifier)) {
      return MakeSymbol(Previous().text);
    }
    if (Match(TokenKind::LParen)) {
      Expr expr = ParseExpression();
      Expect(TokenKind::RParen, "closing ')'");
      if (Match(TokenKind::Caret)) {
        if (!Match(TokenKind::Integer) || Previous().text != "2") {
          throw std::runtime_error(
              "automatic invariant seed construction supports squared linear momentum "
              "combinations only");
        }
        const auto momentum_terms = ExtractLinearMomentum(expr);
        if (!momentum_terms.has_value()) {
          throw std::runtime_error(
              "automatic invariant seed construction supports squared linear momentum "
              "combinations only");
        }
        return MakeMomentumSquare(*momentum_terms);
      }
      return expr;
    }
    throw std::runtime_error("automatic invariant seed construction encountered malformed "
                             "expression: " +
                             source_);
  }

  bool Match(const TokenKind kind) {
    if (Peek().kind != kind) {
      return false;
    }
    ++position_;
    return true;
  }

  void Expect(const TokenKind kind, const std::string& detail) {
    if (!Match(kind)) {
      throw std::runtime_error("automatic invariant seed construction expected " + detail +
                               " in expression: " + source_);
    }
  }

  const Token& Peek() const {
    return tokens_[position_];
  }

  const Token& Previous() const {
    return tokens_[position_ - 1];
  }

  std::string source_;
  std::vector<Token> tokens_;
  std::size_t position_ = 0;
};

std::optional<std::map<std::string, int>> ExtractLinearMomentum(const Expr& expr) {
  std::map<std::string, int> result;

  const auto append = [&](const auto& self, const Expr& node, const int multiplier) -> bool {
    switch (node.kind) {
      case ExprKind::Symbol:
        result[node.symbol] += multiplier;
        return true;
      case ExprKind::Number:
        return node.number.IsZero();
      case ExprKind::Add:
        for (const auto& term : node.terms) {
          if (!self(self, term, multiplier)) {
            return false;
          }
        }
        return true;
      case ExprKind::Multiply: {
        Rational numeric(1);
        std::optional<std::string> symbol;
        for (const auto& factor : node.terms) {
          if (factor.kind == ExprKind::Number) {
            numeric = numeric * factor.number;
            continue;
          }
          if (factor.kind == ExprKind::Symbol && !symbol.has_value()) {
            symbol = factor.symbol;
            continue;
          }
          return false;
        }
        if (!symbol.has_value() || numeric.denominator != 1) {
          return false;
        }
        result[*symbol] += multiplier * static_cast<int>(numeric.numerator);
        return true;
      }
      case ExprKind::MomentumSquare:
        return false;
    }
    return false;
  };

  if (!append(append, expr, 1)) {
    return std::nullopt;
  }
  for (auto it = result.begin(); it != result.end();) {
    if (it->second == 0) {
      it = result.erase(it);
    } else {
      ++it;
    }
  }
  return result;
}

struct LinearScalarExpression {
  Rational constant;
  std::map<std::string, Rational> coefficients;
};

bool AppendLinearScalar(const Expr& expr,
                        const std::set<std::string>& allowed_symbols,
                        LinearScalarExpression& out,
                        const Rational& multiplier) {
  switch (expr.kind) {
    case ExprKind::Number:
      out.constant = out.constant + (multiplier * expr.number);
      return true;
    case ExprKind::Symbol:
      if (!allowed_symbols.count(expr.symbol)) {
        return false;
      }
      out.coefficients[expr.symbol] = out.coefficients[expr.symbol] + multiplier;
      return true;
    case ExprKind::Add:
      for (const auto& term : expr.terms) {
        if (!AppendLinearScalar(term, allowed_symbols, out, multiplier)) {
          return false;
        }
      }
      return true;
    case ExprKind::Multiply: {
      Rational factor = multiplier;
      std::optional<std::string> symbol;
      for (const auto& term : expr.terms) {
        if (term.kind == ExprKind::Number) {
          factor = factor * term.number;
          continue;
        }
        if (term.kind == ExprKind::Symbol && allowed_symbols.count(term.symbol) > 0 &&
            !symbol.has_value()) {
          symbol = term.symbol;
          continue;
        }
        return false;
      }
      if (!symbol.has_value()) {
        out.constant = out.constant + factor;
      } else {
        out.coefficients[*symbol] = out.coefficients[*symbol] + factor;
      }
      return true;
    }
    case ExprKind::MomentumSquare:
      return false;
  }
  return false;
}

std::pair<std::string, std::string> CanonicalPair(const std::string& left,
                                                  const std::string& right) {
  if (left <= right) {
    return {left, right};
  }
  return {right, left};
}

std::pair<std::string, std::string> ParseScalarProductLeft(const std::string& value,
                                                           const std::set<std::string>& external) {
  const std::vector<Token> tokens = Tokenize(value);
  if (tokens.size() != 4 || tokens[0].kind != TokenKind::Identifier ||
      tokens[1].kind != TokenKind::Star || tokens[2].kind != TokenKind::Identifier ||
      tokens[3].kind != TokenKind::End) {
    throw std::runtime_error("automatic invariant seed construction supports scalar-product-rule "
                             "left sides of the form p1*p2 only");
  }
  if (external.count(tokens[0].text) == 0 || external.count(tokens[2].text) == 0) {
    throw std::runtime_error("automatic invariant seed construction supports scalar-product-rule "
                             "left sides over known external momenta only");
  }
  return CanonicalPair(tokens[0].text, tokens[2].text);
}

struct DerivationContext {
  std::set<std::string> invariant_symbols;
  std::set<std::string> loop_momenta;
  std::set<std::string> external_momenta;
  std::map<std::pair<std::string, std::string>, Rational> scalar_pair_derivatives;
};

void ValidateExpressionSymbols(const Expr& expr,
                               const std::set<std::string>& allowed_scalar_symbols) {
  switch (expr.kind) {
    case ExprKind::Number:
      return;
    case ExprKind::Symbol:
      if (allowed_scalar_symbols.count(expr.symbol) == 0) {
        throw std::runtime_error("automatic invariant seed construction encountered unknown "
                                 "coefficient symbol: " +
                                 expr.symbol);
      }
      return;
    case ExprKind::Add:
    case ExprKind::Multiply:
      for (const auto& term : expr.terms) {
        ValidateExpressionSymbols(term, allowed_scalar_symbols);
      }
      return;
    case ExprKind::MomentumSquare:
      return;
  }
}

Expr DifferentiateExpr(const Expr& expr,
                       const std::string& invariant_name,
                       const DerivationContext& context) {
  switch (expr.kind) {
    case ExprKind::Number:
      return MakeNumber(Rational(0));
    case ExprKind::Symbol:
      return MakeNumber(Rational(expr.symbol == invariant_name ? 1 : 0));
    case ExprKind::Add: {
      std::vector<Expr> terms;
      for (const auto& term : expr.terms) {
        terms.push_back(DifferentiateExpr(term, invariant_name, context));
      }
      return MakeAdd(std::move(terms));
    }
    case ExprKind::Multiply: {
      std::vector<Expr> sum_terms;
      for (std::size_t index = 0; index < expr.terms.size(); ++index) {
        const Expr derivative = DifferentiateExpr(expr.terms[index], invariant_name, context);
        if (IsZero(derivative)) {
          continue;
        }
        std::vector<Expr> product_terms;
        product_terms.reserve(expr.terms.size());
        for (std::size_t factor_index = 0; factor_index < expr.terms.size(); ++factor_index) {
          if (factor_index == index) {
            product_terms.push_back(derivative);
          } else {
            product_terms.push_back(expr.terms[factor_index]);
          }
        }
        sum_terms.push_back(MakeMultiply(std::move(product_terms)));
      }
      return MakeAdd(std::move(sum_terms));
    }
    case ExprKind::MomentumSquare: {
      Rational total(0);
      std::vector<std::pair<std::string, int>> external_terms;
      for (const auto& [symbol, multiplier] : expr.momentum_terms) {
        if (context.loop_momenta.count(symbol) > 0 || context.external_momenta.count(symbol) > 0) {
          if (context.external_momenta.count(symbol) > 0) {
            external_terms.emplace_back(symbol, multiplier);
          }
          continue;
        }
        throw std::runtime_error("automatic invariant seed construction encountered unknown "
                                 "momentum symbol: " +
                                 symbol);
      }

      for (std::size_t left_index = 0; left_index < external_terms.size(); ++left_index) {
        const auto& [left_symbol, left_multiplier] = external_terms[left_index];
        for (std::size_t right_index = left_index; right_index < external_terms.size();
             ++right_index) {
          const auto& [right_symbol, right_multiplier] = external_terms[right_index];
          const long long coefficient =
              (left_index == right_index) ? static_cast<long long>(left_multiplier) * left_multiplier
                                          : 2LL * left_multiplier * right_multiplier;
          if (coefficient == 0) {
            continue;
          }
          const auto pair = CanonicalPair(left_symbol, right_symbol);
          const auto pair_it = context.scalar_pair_derivatives.find(pair);
          if (pair_it == context.scalar_pair_derivatives.end()) {
            throw std::runtime_error("automatic invariant seed construction requires scalar-"
                                     "product rule data for external pair: " +
                                     pair.first + "*" + pair.second);
          }
          total = total + (Rational(coefficient) * pair_it->second);
        }
      }
      return MakeNumber(total);
    }
  }
  return MakeNumber(Rational(0));
}

struct Monomial {
  Rational numeric = Rational(1);
  std::vector<std::string> scalar_symbols;
  std::vector<int> factor_indices;
};

std::vector<Monomial> ExpandMonomials(
    const Expr& expr,
    const std::vector<Expr>& propagator_expressions,
    const std::set<std::string>& allowed_scalar_symbols,
    const std::size_t factor_count) {
  if (expr.kind != ExprKind::Number && expr.kind != ExprKind::Symbol) {
    for (std::size_t index = 0; index < propagator_expressions.size(); ++index) {
      if (!ExprEquals(expr, propagator_expressions[index])) {
        continue;
      }
      Monomial monomial;
      monomial.factor_indices.assign(factor_count, 0);
      monomial.factor_indices[index] = -1;
      return {monomial};
    }
  }
  if (expr.kind == ExprKind::Multiply && !expr.terms.empty() &&
      expr.terms.front().kind == ExprKind::Number && expr.terms.size() > 1) {
    std::vector<Expr> remainder_terms(expr.terms.begin() + 1, expr.terms.end());
    const Expr remainder = MakeMultiply(std::move(remainder_terms));
    for (std::size_t index = 0; index < propagator_expressions.size(); ++index) {
      if (!ExprEquals(remainder, propagator_expressions[index])) {
        continue;
      }
      Monomial monomial;
      monomial.numeric = expr.terms.front().number;
      monomial.factor_indices.assign(factor_count, 0);
      monomial.factor_indices[index] = -1;
      return {monomial};
    }
  }

  switch (expr.kind) {
    case ExprKind::Number:
      if (expr.number.IsZero()) {
        return {};
      }
      return {Monomial{expr.number, {}, std::vector<int>(factor_count, 0)}};
    case ExprKind::Symbol:
      if (allowed_scalar_symbols.count(expr.symbol) == 0) {
        throw std::runtime_error("automatic invariant seed construction encountered unsupported "
                                 "coefficient symbol in derivative: " +
                                 expr.symbol);
      }
      return {Monomial{Rational(1), {expr.symbol}, std::vector<int>(factor_count, 0)}};
    case ExprKind::MomentumSquare:
      throw std::runtime_error("automatic invariant seed construction could not represent "
                               "derivative term using known propagator factors: " +
                               DescribeExpr(expr));
    case ExprKind::Add: {
      std::vector<Monomial> monomials;
      for (const auto& term : expr.terms) {
        auto term_monomials =
            ExpandMonomials(term, propagator_expressions, allowed_scalar_symbols, factor_count);
        monomials.insert(monomials.end(),
                         std::make_move_iterator(term_monomials.begin()),
                         std::make_move_iterator(term_monomials.end()));
      }
      return monomials;
    }
    case ExprKind::Multiply: {
      std::vector<Monomial> monomials = {
          {Rational(1), {}, std::vector<int>(factor_count, 0)}};
      for (const auto& factor : expr.terms) {
        const auto factor_monomials =
            ExpandMonomials(factor, propagator_expressions, allowed_scalar_symbols, factor_count);
        if (factor_monomials.empty()) {
          return {};
        }

        std::vector<Monomial> next;
        for (const auto& left : monomials) {
          for (const auto& right : factor_monomials) {
            Monomial combined;
            combined.numeric = left.numeric * right.numeric;
            combined.scalar_symbols = left.scalar_symbols;
            combined.scalar_symbols.insert(combined.scalar_symbols.end(),
                                           right.scalar_symbols.begin(),
                                           right.scalar_symbols.end());
            combined.factor_indices = left.factor_indices;
            for (std::size_t index = 0; index < combined.factor_indices.size(); ++index) {
              combined.factor_indices[index] += right.factor_indices[index];
            }
            next.push_back(std::move(combined));
          }
        }
        monomials = std::move(next);
      }
      return monomials;
    }
  }
  return {};
}

std::string RenderCoefficient(const Monomial& monomial) {
  if (monomial.numeric.IsZero()) {
    return "0";
  }

  std::vector<std::string> factors;
  if (!monomial.numeric.IsOne() || monomial.scalar_symbols.empty()) {
    factors.push_back(monomial.numeric.ToString());
  }
  for (const auto& symbol : monomial.scalar_symbols) {
    factors.push_back(symbol);
  }

  if (factors.empty()) {
    return "1";
  }
  if (factors.size() == 1) {
    return factors.front();
  }

  std::ostringstream out;
  for (std::size_t index = 0; index < factors.size(); ++index) {
    if (index > 0) {
      out << "*";
    }
    out << "(" << factors[index] << ")";
  }
  return out.str();
}

struct CollectedCoefficientPiece {
  std::string factor_coefficient = "1";
  int multiplier = 0;
};

struct CollectedRowTerm {
  TargetIntegral target;
  std::vector<CollectedCoefficientPiece> coefficient_pieces;
  std::map<std::string, std::size_t> coefficient_piece_index;
};

std::string NormalizeFactorCoefficient(const std::string& factor_coefficient) {
  if (factor_coefficient.empty()) {
    return "1";
  }
  return factor_coefficient;
}

std::string RenderCollectedCoefficient(
    const std::vector<CollectedCoefficientPiece>& coefficient_pieces) {
  std::string rendered;
  for (const auto& piece : coefficient_pieces) {
    if (piece.multiplier == 0) {
      continue;
    }

    std::string contribution;
    if (piece.factor_coefficient == "1") {
      contribution = std::to_string(piece.multiplier);
    } else {
      contribution = "(" + std::to_string(piece.multiplier) + ")*(" + piece.factor_coefficient +
                     ")";
    }

    if (!rendered.empty()) {
      rendered += " + ";
    }
    rendered += contribution;
  }
  return rendered;
}

TargetIntegral ComposeGeneratedTarget(const TargetIntegral& master,
                                      const std::size_t propagator_index,
                                      const std::vector<int>& factor_indices) {
  TargetIntegral target;
  target.family = master.family;
  target.indices = master.indices;
  ++target.indices[propagator_index];
  for (std::size_t index = 0; index < factor_indices.size(); ++index) {
    target.indices[index] += factor_indices[index];
  }
  return target;
}

void AppendContribution(std::vector<CollectedRowTerm>& row_terms,
                        std::map<std::string, std::size_t>& row_target_index,
                        const int multiplier,
                        const std::string& factor_coefficient,
                        const TargetIntegral& target) {
  const std::string label = target.Label();
  const auto it = row_target_index.find(label);
  const std::string normalized_factor = NormalizeFactorCoefficient(factor_coefficient);
  if (it == row_target_index.end()) {
    CollectedRowTerm row_term;
    row_term.target = target;
    row_term.coefficient_piece_index.emplace(normalized_factor, 0);
    row_term.coefficient_pieces.push_back({normalized_factor, multiplier});
    row_target_index.emplace(label, row_terms.size());
    row_terms.push_back(std::move(row_term));
    return;
  }

  CollectedRowTerm& row_term = row_terms[it->second];
  const auto piece_it = row_term.coefficient_piece_index.find(normalized_factor);
  if (piece_it == row_term.coefficient_piece_index.end()) {
    row_term.coefficient_piece_index.emplace(normalized_factor,
                                             row_term.coefficient_pieces.size());
    row_term.coefficient_pieces.push_back({normalized_factor, multiplier});
    return;
  }

  row_term.coefficient_pieces[piece_it->second].multiplier += multiplier;
}

void AppendCollectedRowTerms(const std::vector<CollectedRowTerm>& collected_terms,
                             GeneratedDerivativeRow& row,
                             std::set<std::string>& seen_targets,
                             std::vector<TargetIntegral>& reduction_targets) {
  for (const auto& collected_term : collected_terms) {
    const std::string coefficient =
        RenderCollectedCoefficient(collected_term.coefficient_pieces);
    if (coefficient.empty()) {
      continue;
    }

    GeneratedDerivativeTerm term;
    term.coefficient = coefficient;
    term.target = collected_term.target;
    row.terms.push_back(std::move(term));

    const std::string label = collected_term.target.Label();
    if (seen_targets.insert(label).second) {
      reduction_targets.push_back(collected_term.target);
    }
  }
}

}  // namespace

InvariantDerivativeSeed BuildInvariantDerivativeSeed(const ProblemSpec& spec,
                                                     const std::string& invariant_name) {
  if (invariant_name.empty()) {
    throw std::runtime_error(
        "automatic invariant seed construction requires a non-empty invariant name");
  }
  if (invariant_name == "eta") {
    throw std::runtime_error(
        "automatic invariant seed construction does not accept eta through the invariant seam");
  }
  if (std::find(spec.kinematics.invariants.begin(),
                spec.kinematics.invariants.end(),
                invariant_name) == spec.kinematics.invariants.end()) {
    throw std::runtime_error("automatic invariant seed construction requires invariant \"" +
                             invariant_name + "\" in spec.kinematics.invariants");
  }

  InvariantDerivativeSeed seed;
  seed.family = spec.family.name;
  seed.variable = {invariant_name, DifferentiationVariableKind::Invariant};
  seed.propagator_derivatives.resize(spec.family.propagators.size());

  std::set<std::string> invariant_symbols(spec.kinematics.invariants.begin(),
                                          spec.kinematics.invariants.end());
  std::set<std::string> loop_momenta(spec.family.loop_momenta.begin(),
                                     spec.family.loop_momenta.end());
  std::set<std::string> external_momenta(spec.kinematics.incoming_momenta.begin(),
                                         spec.kinematics.incoming_momenta.end());
  external_momenta.insert(spec.kinematics.outgoing_momenta.begin(),
                          spec.kinematics.outgoing_momenta.end());

  DerivationContext context;
  context.invariant_symbols = invariant_symbols;
  context.loop_momenta = loop_momenta;
  context.external_momenta = external_momenta;

  for (const auto& rule : spec.kinematics.scalar_product_rules) {
    const auto pair = ParseScalarProductLeft(rule.left, external_momenta);
    Expr rhs = ExprParser(rule.right).Parse();
    LinearScalarExpression linear_rhs;
    if (!AppendLinearScalar(rhs, invariant_symbols, linear_rhs, Rational(1))) {
      throw std::runtime_error("automatic invariant seed construction supports only linear "
                               "scalar-product-rule expressions");
    }
    const auto [_, inserted] = context.scalar_pair_derivatives.emplace(
        pair,
        linear_rhs.coefficients.count(invariant_name) > 0 ? linear_rhs.coefficients[invariant_name]
                                                          : Rational(0));
    if (!inserted) {
      throw std::runtime_error("automatic invariant seed construction encountered duplicate "
                               "scalar-product rule for external pair: " +
                               pair.first + "*" + pair.second);
    }
  }

  std::vector<Expr> propagator_expressions;
  propagator_expressions.reserve(spec.family.propagators.size());
  for (const auto& propagator : spec.family.propagators) {
    if (propagator.kind != PropagatorKind::Standard) {
      throw std::runtime_error("automatic invariant seed construction supports Standard "
                               "propagators only");
    }
    if (propagator.mass != "0") {
      throw std::runtime_error("automatic invariant seed construction requires propagator mass "
                               "== \"0\" in the bootstrap subset");
    }
    Expr expression = ExprParser(propagator.expression).Parse();
    ValidateExpressionSymbols(expression, invariant_symbols);
    propagator_expressions.push_back(std::move(expression));
  }

  for (std::size_t index = 0; index < propagator_expressions.size(); ++index) {
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (ExprEquals(propagator_expressions[index], propagator_expressions[prior])) {
        throw std::runtime_error("automatic invariant seed construction requires unique "
                                 "propagator expressions for factor matching");
      }
    }
  }

  for (std::size_t index = 0; index < propagator_expressions.size(); ++index) {
    const Expr derivative =
        DifferentiateExpr(propagator_expressions[index], invariant_name, context);
    const auto monomials = ExpandMonomials(
        derivative, propagator_expressions, invariant_symbols, propagator_expressions.size());
    for (const auto& monomial : monomials) {
      const std::string coefficient = RenderCoefficient(monomial);
      if (coefficient == "0") {
        continue;
      }
      InvariantPropagatorDerivativeTerm term;
      term.coefficient = coefficient;
      term.factor_indices = monomial.factor_indices;
      seed.propagator_derivatives[index].terms.push_back(std::move(term));
    }
  }

  return seed;
}

GeneratedDerivativeVariable GenerateInvariantDerivativeVariable(
    const ParsedMasterList& master_basis,
    const InvariantDerivativeSeed& seed) {
  if (seed.variable.kind != DifferentiationVariableKind::Invariant) {
    throw std::runtime_error(
        "invariant derivative generation requires DifferentiationVariableKind::Invariant");
  }
  if (seed.variable.name.empty()) {
    throw std::runtime_error(
        "invariant derivative generation requires a non-empty invariant variable name");
  }
  if (seed.variable.name == "eta") {
    throw std::runtime_error(
        "invariant derivative generation does not accept eta through the invariant seam");
  }
  if (master_basis.family != seed.family) {
    throw std::runtime_error("invariant derivative generation requires ParsedMasterList.family \"" +
                             seed.family + "\", found \"" + master_basis.family + "\"");
  }

  GeneratedDerivativeVariable generated_variable;
  generated_variable.variable = seed.variable;

  const std::size_t propagator_count = seed.propagator_derivatives.size();
  std::optional<std::size_t> factor_count;
  for (const auto& propagator_derivative : seed.propagator_derivatives) {
    for (const auto& factor_term : propagator_derivative.terms) {
      if (!factor_count.has_value()) {
        factor_count = factor_term.factor_indices.size();
      } else if (factor_term.factor_indices.size() != *factor_count) {
        throw std::runtime_error(
            "invariant derivative generation requires derivative factor index count to match "
            "the propagator count");
      }
    }
  }

  const std::size_t expected_master_index_count =
      factor_count.has_value() ? *factor_count : propagator_count;
  if (factor_count.has_value() && propagator_count != *factor_count) {
    throw std::runtime_error(
        "invariant derivative generation requires propagator derivative count to match the master "
        "index count");
  }
  for (const auto& propagator_derivative : seed.propagator_derivatives) {
    for (const auto& factor_term : propagator_derivative.terms) {
      if (factor_term.factor_indices.size() != expected_master_index_count) {
        throw std::runtime_error(
            "invariant derivative generation requires derivative factor index count to match "
            "the propagator count");
      }
    }
  }
  if (!master_basis.masters.empty() &&
      master_basis.masters.front().indices.size() != expected_master_index_count) {
    throw std::runtime_error(
        "invariant derivative generation requires master index count to match the propagator "
        "derivative count");
  }
  std::set<std::string> seen_targets;

  generated_variable.rows.reserve(master_basis.masters.size());
  for (const auto& master : master_basis.masters) {
    if (master.family != seed.family) {
      throw std::runtime_error("invariant derivative generation requires master family \"" +
                               seed.family + "\", found \"" + master.family + "\"");
    }
    if (master.indices.size() != expected_master_index_count) {
      throw std::runtime_error(
          "invariant derivative generation requires master index count to match the propagator "
          "derivative count");
    }

    GeneratedDerivativeRow row;
    row.source_master = master;
    std::vector<CollectedRowTerm> collected_terms;
    std::map<std::string, std::size_t> row_target_index;

    for (std::size_t propagator_index = 0; propagator_index < propagator_count;
         ++propagator_index) {
      const int exponent = master.indices[propagator_index];
      if (exponent == 0) {
        continue;
      }

      for (const auto& factor_term : seed.propagator_derivatives[propagator_index].terms) {
        const TargetIntegral target =
            ComposeGeneratedTarget(master, propagator_index, factor_term.factor_indices);

        AppendContribution(collected_terms,
                           row_target_index,
                           -exponent,
                           factor_term.coefficient,
                           target);
      }
    }

    AppendCollectedRowTerms(
        collected_terms, row, seen_targets, generated_variable.reduction_targets);
    generated_variable.rows.push_back(std::move(row));
  }

  return generated_variable;
}

}  // namespace amflow
