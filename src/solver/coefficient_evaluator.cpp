#include "amflow/solver/coefficient_evaluator.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace amflow {

namespace {

std::string StripLeadingZeros(const std::string& digits) {
  const std::size_t first_nonzero = digits.find_first_not_of('0');
  if (first_nonzero == std::string::npos) {
    return "0";
  }
  return digits.substr(first_nonzero);
}

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(start, end - start);
}

struct BigInt {
  bool negative = false;
  std::string digits = "0";

  BigInt() = default;
  explicit BigInt(const long long value) { AssignFromSignedString(std::to_string(value)); }
  explicit BigInt(const std::string& value) { AssignFromSignedString(value); }

  static BigInt FromUnsignedDigits(const std::string& value) {
    BigInt result;
    result.negative = false;
    result.digits = StripLeadingZeros(value);
    return result;
  }

  bool IsZero() const { return digits == "0"; }

  std::string ToString() const {
    if (negative && !IsZero()) {
      return "-" + digits;
    }
    return digits;
  }

 private:
  void AssignFromSignedString(const std::string& value) {
    if (value.empty()) {
      throw std::invalid_argument("internal bigint parse requires a non-empty value");
    }

    std::size_t index = 0;
    if (value[index] == '+') {
      ++index;
    } else if (value[index] == '-') {
      negative = true;
      ++index;
    }

    if (index == value.size()) {
      throw std::invalid_argument("internal bigint parse requires digits");
    }

    for (std::size_t position = index; position < value.size(); ++position) {
      if (!std::isdigit(static_cast<unsigned char>(value[position]))) {
        throw std::invalid_argument("internal bigint parse encountered non-digit input");
      }
    }

    digits = StripLeadingZeros(value.substr(index));
    if (digits == "0") {
      negative = false;
    }
  }
};

int CompareAbs(const BigInt& lhs, const BigInt& rhs) {
  if (lhs.digits.size() < rhs.digits.size()) {
    return -1;
  }
  if (lhs.digits.size() > rhs.digits.size()) {
    return 1;
  }
  if (lhs.digits < rhs.digits) {
    return -1;
  }
  if (lhs.digits > rhs.digits) {
    return 1;
  }
  return 0;
}

BigInt Abs(BigInt value) {
  value.negative = false;
  return value;
}

std::string AddAbsDigits(const std::string& lhs, const std::string& rhs) {
  std::string result;
  result.reserve(std::max(lhs.size(), rhs.size()) + 1);

  int carry = 0;
  int lhs_index = static_cast<int>(lhs.size()) - 1;
  int rhs_index = static_cast<int>(rhs.size()) - 1;
  while (lhs_index >= 0 || rhs_index >= 0 || carry != 0) {
    int digit = carry;
    if (lhs_index >= 0) {
      digit += lhs[static_cast<std::size_t>(lhs_index)] - '0';
      --lhs_index;
    }
    if (rhs_index >= 0) {
      digit += rhs[static_cast<std::size_t>(rhs_index)] - '0';
      --rhs_index;
    }
    result.push_back(static_cast<char>('0' + (digit % 10)));
    carry = digit / 10;
  }

  std::reverse(result.begin(), result.end());
  return StripLeadingZeros(result);
}

std::string SubtractAbsDigits(const std::string& lhs, const std::string& rhs) {
  std::string result;
  result.reserve(lhs.size());

  int borrow = 0;
  int lhs_index = static_cast<int>(lhs.size()) - 1;
  int rhs_index = static_cast<int>(rhs.size()) - 1;
  while (lhs_index >= 0) {
    int digit = (lhs[static_cast<std::size_t>(lhs_index)] - '0') - borrow;
    if (rhs_index >= 0) {
      digit -= rhs[static_cast<std::size_t>(rhs_index)] - '0';
      --rhs_index;
    }
    if (digit < 0) {
      digit += 10;
      borrow = 1;
    } else {
      borrow = 0;
    }
    result.push_back(static_cast<char>('0' + digit));
    --lhs_index;
  }

  std::reverse(result.begin(), result.end());
  return StripLeadingZeros(result);
}

std::string MultiplyAbsDigits(const std::string& lhs, const std::string& rhs) {
  if (lhs == "0" || rhs == "0") {
    return "0";
  }

  std::vector<int> digits(lhs.size() + rhs.size(), 0);
  for (int lhs_index = static_cast<int>(lhs.size()) - 1; lhs_index >= 0; --lhs_index) {
    for (int rhs_index = static_cast<int>(rhs.size()) - 1; rhs_index >= 0; --rhs_index) {
      const int product = (lhs[static_cast<std::size_t>(lhs_index)] - '0') *
                          (rhs[static_cast<std::size_t>(rhs_index)] - '0');
      digits[static_cast<std::size_t>(lhs_index + rhs_index + 1)] += product;
    }
  }

  for (int index = static_cast<int>(digits.size()) - 1; index > 0; --index) {
    digits[static_cast<std::size_t>(index - 1)] += digits[static_cast<std::size_t>(index)] / 10;
    digits[static_cast<std::size_t>(index)] %= 10;
  }

  std::string result;
  result.reserve(digits.size());
  bool started = false;
  for (const int digit : digits) {
    if (!started && digit == 0) {
      continue;
    }
    started = true;
    result.push_back(static_cast<char>('0' + digit));
  }
  return started ? result : "0";
}

BigInt AddAbs(const BigInt& lhs, const BigInt& rhs) {
  return BigInt::FromUnsignedDigits(AddAbsDigits(lhs.digits, rhs.digits));
}

BigInt SubtractAbs(const BigInt& lhs, const BigInt& rhs) {
  return BigInt::FromUnsignedDigits(SubtractAbsDigits(lhs.digits, rhs.digits));
}

BigInt operator-(const BigInt& value) {
  BigInt result = value;
  if (!result.IsZero()) {
    result.negative = !result.negative;
  }
  return result;
}

BigInt operator+(const BigInt& lhs, const BigInt& rhs) {
  if (lhs.negative == rhs.negative) {
    BigInt result = AddAbs(lhs, rhs);
    result.negative = lhs.negative && !result.IsZero();
    return result;
  }

  const int comparison = CompareAbs(lhs, rhs);
  if (comparison == 0) {
    return BigInt(0);
  }

  if (comparison > 0) {
    BigInt result = SubtractAbs(lhs, rhs);
    result.negative = lhs.negative;
    return result;
  }

  BigInt result = SubtractAbs(rhs, lhs);
  result.negative = rhs.negative;
  return result;
}

BigInt operator-(const BigInt& lhs, const BigInt& rhs) {
  return lhs + (-rhs);
}

BigInt operator*(const BigInt& lhs, const BigInt& rhs) {
  BigInt result = BigInt::FromUnsignedDigits(MultiplyAbsDigits(lhs.digits, rhs.digits));
  result.negative = (lhs.negative != rhs.negative) && !result.IsZero();
  return result;
}

BigInt AppendDigit(const BigInt& value, const int digit) {
  if (digit < 0 || digit > 9) {
    throw std::invalid_argument("internal bigint append requires one decimal digit");
  }

  if (value.IsZero()) {
    return BigInt::FromUnsignedDigits(std::string(1, static_cast<char>('0' + digit)));
  }
  return BigInt::FromUnsignedDigits(value.digits +
                                    std::string(1, static_cast<char>('0' + digit)));
}

std::pair<BigInt, BigInt> DivideAndRemainderAbs(const BigInt& dividend, const BigInt& divisor) {
  if (divisor.IsZero()) {
    throw std::runtime_error("coefficient evaluation encountered division by zero");
  }
  if (CompareAbs(dividend, divisor) < 0) {
    return {BigInt(0), dividend};
  }

  const BigInt positive_divisor = Abs(divisor);
  BigInt remainder(0);
  std::string quotient_digits;
  quotient_digits.reserve(dividend.digits.size());
  for (const char digit : dividend.digits) {
    remainder = AppendDigit(remainder, digit - '0');
    int quotient_digit = 0;
    while (CompareAbs(remainder, positive_divisor) >= 0) {
      remainder = remainder - positive_divisor;
      ++quotient_digit;
    }
    quotient_digits.push_back(static_cast<char>('0' + quotient_digit));
  }

  return {BigInt::FromUnsignedDigits(quotient_digits), remainder};
}

BigInt operator/(const BigInt& lhs, const BigInt& rhs) {
  const auto [quotient, _] = DivideAndRemainderAbs(Abs(lhs), Abs(rhs));
  BigInt result = quotient;
  result.negative = (lhs.negative != rhs.negative) && !result.IsZero();
  return result;
}

BigInt operator%(const BigInt& lhs, const BigInt& rhs) {
  auto [_, remainder] = DivideAndRemainderAbs(Abs(lhs), Abs(rhs));
  remainder.negative = lhs.negative && !remainder.IsZero();
  return remainder;
}

BigInt Gcd(BigInt lhs, BigInt rhs) {
  lhs = Abs(std::move(lhs));
  rhs = Abs(std::move(rhs));
  while (!rhs.IsZero()) {
    const BigInt remainder = lhs % rhs;
    lhs = rhs;
    rhs = remainder;
  }
  return lhs;
}

struct BigRational {
  BigInt numerator = BigInt(0);
  BigInt denominator = BigInt(1);

  BigRational() = default;
  explicit BigRational(const long long value) : numerator(value), denominator(1) {}
  BigRational(BigInt numerator_value, BigInt denominator_value)
      : numerator(std::move(numerator_value)), denominator(std::move(denominator_value)) {
    Normalize();
  }

  void Normalize() {
    if (denominator.IsZero()) {
      throw std::runtime_error("coefficient evaluation encountered division by zero");
    }
    if (denominator.negative) {
      numerator = -numerator;
      denominator = -denominator;
    }
    if (numerator.IsZero()) {
      denominator = BigInt(1);
      return;
    }
    const BigInt divisor = Gcd(numerator, denominator);
    if (!divisor.IsZero()) {
      numerator = numerator / divisor;
      denominator = denominator / divisor;
    }
  }

  bool IsZero() const { return numerator.IsZero(); }

  ExactRational ToPublic() const { return {numerator.ToString(), denominator.ToString()}; }
};

BigRational operator+(const BigRational& lhs, const BigRational& rhs) {
  return BigRational(lhs.numerator * rhs.denominator + rhs.numerator * lhs.denominator,
                     lhs.denominator * rhs.denominator);
}

BigRational operator-(const BigRational& lhs, const BigRational& rhs) {
  return BigRational(lhs.numerator * rhs.denominator - rhs.numerator * lhs.denominator,
                     lhs.denominator * rhs.denominator);
}

BigRational operator*(const BigRational& lhs, const BigRational& rhs) {
  return BigRational(lhs.numerator * rhs.numerator, lhs.denominator * rhs.denominator);
}

BigRational operator/(const BigRational& lhs, const BigRational& rhs) {
  return BigRational(lhs.numerator * rhs.denominator, lhs.denominator * rhs.numerator);
}

struct BigComplexRational {
  BigRational real = BigRational(0);
  BigRational imaginary = BigRational(0);

  BigComplexRational() = default;
  explicit BigComplexRational(const long long value) : real(value), imaginary(0) {}
  BigComplexRational(BigRational real_value, BigRational imaginary_value)
      : real(std::move(real_value)), imaginary(std::move(imaginary_value)) {}

  bool IsZero() const { return real.IsZero() && imaginary.IsZero(); }

  ExactComplexRational ToPublic() const {
    return {real.ToPublic(), imaginary.ToPublic()};
  }
};

BigComplexRational operator+(const BigComplexRational& lhs, const BigComplexRational& rhs) {
  return BigComplexRational(lhs.real + rhs.real, lhs.imaginary + rhs.imaginary);
}

BigComplexRational operator-(const BigComplexRational& lhs, const BigComplexRational& rhs) {
  return BigComplexRational(lhs.real - rhs.real, lhs.imaginary - rhs.imaginary);
}

BigComplexRational operator*(const BigComplexRational& lhs, const BigComplexRational& rhs) {
  return BigComplexRational(lhs.real * rhs.real - lhs.imaginary * rhs.imaginary,
                            lhs.real * rhs.imaginary + lhs.imaginary * rhs.real);
}

BigComplexRational operator/(const BigComplexRational& lhs, const BigComplexRational& rhs) {
  const BigRational denominator = rhs.real * rhs.real + rhs.imaginary * rhs.imaginary;
  return BigComplexRational((lhs.real * rhs.real + lhs.imaginary * rhs.imaginary) / denominator,
                            (lhs.imaginary * rhs.real - lhs.real * rhs.imaginary) /
                                denominator);
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
        throw std::invalid_argument("coefficient evaluation encountered malformed expression: "
                                    "unexpected character \"" +
                                    std::string(1, ch) + "\" in \"" + expression + "\"");
    }
    ++index;
  }
  tokens.push_back({TokenKind::End, ""});
  return tokens;
}

using ExactBindingMap = std::map<std::string, BigRational>;
using ComplexBindingMap = std::map<std::string, BigComplexRational>;

class ExpressionParser {
 public:
  ExpressionParser(std::string expression, ExactBindingMap bindings)
      : expression_(std::move(expression)),
        tokens_(Tokenize(expression_)),
        bindings_(std::move(bindings)) {}

  BigRational Parse() {
    const BigRational value = ParseExpression();
    if (Current().kind != TokenKind::End) {
      throw Malformed("unexpected trailing token \"" + Current().text + "\"");
    }
    return value;
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
    return std::invalid_argument("coefficient evaluation encountered malformed expression: " +
                                 detail + " in \"" + expression_ + "\"");
  }

  BigRational ParseExpression() {
    BigRational value = ParseTerm();
    while (true) {
      if (Match(TokenKind::Plus)) {
        value = value + ParseTerm();
        continue;
      }
      if (Match(TokenKind::Minus)) {
        value = value - ParseTerm();
        continue;
      }
      break;
    }
    return value;
  }

  BigRational ParseTerm() {
    BigRational value = ParseUnary();
    while (true) {
      if (Match(TokenKind::Star)) {
        value = value * ParseUnary();
        continue;
      }
      if (Match(TokenKind::Slash)) {
        try {
          value = value / ParseUnary();
        } catch (const std::runtime_error&) {
          throw std::runtime_error("coefficient evaluation encountered division by zero in \"" +
                                   expression_ + "\"");
        }
        continue;
      }
      break;
    }
    return value;
  }

  BigRational ParseUnary() {
    if (Match(TokenKind::Plus)) {
      return ParseUnary();
    }
    if (Match(TokenKind::Minus)) {
      return BigRational(0) - ParseUnary();
    }
    return ParsePrimary();
  }

  BigRational ParsePrimary() {
    if (Current().kind == TokenKind::Number) {
      const Token token = Advance();
      return BigRational(BigInt::FromUnsignedDigits(token.text), BigInt(1));
    }

    if (Current().kind == TokenKind::Identifier) {
      const Token token = Advance();
      const auto binding_it = bindings_.find(token.text);
      if (binding_it == bindings_.end()) {
        throw std::invalid_argument("coefficient evaluation requires a numeric binding for "
                                    "symbol \"" +
                                    token.text + "\"");
      }
      return binding_it->second;
    }

    if (Match(TokenKind::LeftParen)) {
      const BigRational value = ParseExpression();
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
  std::vector<Token> tokens_;
  ExactBindingMap bindings_;
  std::size_t position_ = 0;
};

BigRational EvaluateExpressionToBigRational(const std::string& expression,
                                            const ExactBindingMap& bindings) {
  return ExpressionParser(expression, bindings).Parse();
}

ExactBindingMap ParseEvaluationPoint(const NumericEvaluationPoint& evaluation_point) {
  ExactBindingMap bindings;
  for (const auto& [symbol, value] : evaluation_point) {
    bindings.emplace(symbol, EvaluateExpressionToBigRational(value, ExactBindingMap{}));
  }
  return bindings;
}

class ComplexExpressionParser {
 public:
  ComplexExpressionParser(std::string expression, ComplexBindingMap bindings)
      : expression_(std::move(expression)),
        tokens_(Tokenize(expression_)),
        bindings_(std::move(bindings)) {}

  BigComplexRational Parse() {
    const BigComplexRational value = ParseExpression();
    if (Current().kind != TokenKind::End) {
      throw Malformed("unexpected trailing token \"" + Current().text + "\"");
    }
    return value;
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
    return std::invalid_argument("complex coefficient evaluation encountered malformed "
                                 "expression: " +
                                 detail + " in \"" + expression_ + "\"");
  }

  BigComplexRational ParseExpression() {
    BigComplexRational value = ParseTerm();
    while (true) {
      if (Match(TokenKind::Plus)) {
        value = value + ParseTerm();
        continue;
      }
      if (Match(TokenKind::Minus)) {
        value = value - ParseTerm();
        continue;
      }
      break;
    }
    return value;
  }

  BigComplexRational ParseTerm() {
    BigComplexRational value = ParseUnary();
    while (true) {
      if (Match(TokenKind::Star)) {
        value = value * ParseUnary();
        continue;
      }
      if (Match(TokenKind::Slash)) {
        try {
          value = value / ParseUnary();
        } catch (const std::runtime_error&) {
          throw std::runtime_error("complex coefficient evaluation encountered division by zero "
                                   "in \"" +
                                   expression_ + "\"");
        }
        continue;
      }
      break;
    }
    return value;
  }

  BigComplexRational ParseUnary() {
    if (Match(TokenKind::Plus)) {
      return ParseUnary();
    }
    if (Match(TokenKind::Minus)) {
      return BigComplexRational(0) - ParseUnary();
    }
    return ParsePrimary();
  }

  BigComplexRational ParsePrimary() {
    if (Current().kind == TokenKind::Number) {
      const Token token = Advance();
      return BigComplexRational(
          BigRational(BigInt::FromUnsignedDigits(token.text), BigInt(1)),
          BigRational(0));
    }

    if (Current().kind == TokenKind::Identifier) {
      const Token token = Advance();
      if (token.text == "I") {
        return BigComplexRational(BigRational(0), BigRational(1));
      }

      const auto binding_it = bindings_.find(token.text);
      if (binding_it == bindings_.end()) {
        throw std::invalid_argument("complex coefficient evaluation requires a numeric binding "
                                    "for symbol \"" +
                                    token.text + "\"");
      }
      return binding_it->second;
    }

    if (Match(TokenKind::LeftParen)) {
      const BigComplexRational value = ParseExpression();
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
  std::vector<Token> tokens_;
  ComplexBindingMap bindings_;
  std::size_t position_ = 0;
};

BigComplexRational EvaluateExpressionToBigComplexRational(
    const std::string& expression,
    const ComplexBindingMap& bindings) {
  return ComplexExpressionParser(expression, bindings).Parse();
}

ComplexBindingMap ParseComplexEvaluationPoint(const NumericEvaluationPoint& evaluation_point) {
  ComplexBindingMap bindings;
  for (const auto& [symbol, value] : evaluation_point) {
    if (symbol == "I") {
      throw std::invalid_argument(
          "complex coefficient evaluation reserves symbol \"I\" for the imaginary unit");
    }
    bindings.emplace(symbol, EvaluateExpressionToBigComplexRational(value, ComplexBindingMap{}));
  }
  return bindings;
}

BigComplexRational EvaluateComplexPointExpressionImpl(const std::string& variable_name,
                                                      const std::string& point_expression,
                                                      const ComplexBindingMap& bindings) {
  const std::string trimmed = Trim(point_expression);
  if (trimmed.empty()) {
    throw std::invalid_argument(
        "complex point parsing encountered malformed point expression: empty input");
  }

  const std::size_t separator = trimmed.find('=');
  if (separator == std::string::npos) {
    return EvaluateExpressionToBigComplexRational(trimmed, bindings);
  }

  if (trimmed.find('=', separator + 1) != std::string::npos) {
    throw std::invalid_argument("complex point parsing encountered malformed point expression: \"" +
                                point_expression + "\"");
  }

  const std::string lhs = Trim(trimmed.substr(0, separator));
  const std::string rhs = Trim(trimmed.substr(separator + 1));
  if (lhs != variable_name || rhs.empty()) {
    throw std::invalid_argument("complex point parsing encountered malformed point expression: \"" +
                                point_expression + "\"");
  }

  return EvaluateExpressionToBigComplexRational(rhs, bindings);
}

}  // namespace

bool ExactRational::IsZero() const {
  return numerator == "0";
}

std::string ExactRational::ToString() const {
  if (denominator == "1") {
    return numerator;
  }
  return numerator + "/" + denominator;
}

bool operator==(const ExactRational& lhs, const ExactRational& rhs) {
  return lhs.numerator == rhs.numerator && lhs.denominator == rhs.denominator;
}

bool operator!=(const ExactRational& lhs, const ExactRational& rhs) {
  return !(lhs == rhs);
}

bool ExactComplexRational::IsReal() const {
  return imaginary.IsZero();
}

std::string ExactComplexRational::ToString() const {
  if (imaginary.IsZero()) {
    return real.ToString();
  }
  if (real.IsZero()) {
    return "(" + imaginary.ToString() + ")*I";
  }
  return "(" + real.ToString() + ") + (" + imaginary.ToString() + ")*I";
}

bool operator==(const ExactComplexRational& lhs, const ExactComplexRational& rhs) {
  return lhs.real == rhs.real && lhs.imaginary == rhs.imaginary;
}

bool operator!=(const ExactComplexRational& lhs, const ExactComplexRational& rhs) {
  return !(lhs == rhs);
}

ExactRational EvaluateCoefficientExpression(const std::string& expression,
                                            const NumericEvaluationPoint& evaluation_point) {
  return EvaluateExpressionToBigRational(expression, ParseEvaluationPoint(evaluation_point))
      .ToPublic();
}

ExactRationalMatrix EvaluateCoefficientMatrix(const DESystem& system,
                                              const std::string& variable_name,
                                              const NumericEvaluationPoint& evaluation_point) {
  const auto matrix_it = system.coefficient_matrices.find(variable_name);
  if (matrix_it == system.coefficient_matrices.end()) {
    throw std::invalid_argument("coefficient evaluation requires coefficient matrix for "
                                "variable \"" +
                                variable_name + "\"");
  }

  const ExactBindingMap bindings = ParseEvaluationPoint(evaluation_point);
  ExactRationalMatrix evaluated_matrix;
  evaluated_matrix.reserve(matrix_it->second.size());
  for (const auto& row : matrix_it->second) {
    std::vector<ExactRational> evaluated_row;
    evaluated_row.reserve(row.size());
    for (const auto& cell : row) {
      evaluated_row.push_back(EvaluateExpressionToBigRational(cell, bindings).ToPublic());
    }
    evaluated_matrix.push_back(std::move(evaluated_row));
  }
  return evaluated_matrix;
}

NumericEvaluationPoint BuildComplexNumericEvaluationPoint(const ProblemSpec& spec) {
  if (!spec.kinematics.complex_numeric_substitutions.empty() && !spec.complex_mode) {
    throw std::invalid_argument(
        "kinematics.complex_numeric_substitutions require complex_mode: true");
  }

  NumericEvaluationPoint evaluation_point;
  for (const auto& [name, value] : spec.kinematics.numeric_substitutions) {
    static_cast<void>(EvaluateCoefficientExpression(value, NumericEvaluationPoint{}));
    evaluation_point.emplace(name, value);
  }
  for (const auto& [name, value] : spec.kinematics.complex_numeric_substitutions) {
    if (!evaluation_point.emplace(name, value).second) {
      throw std::invalid_argument("kinematics.complex_numeric_substitutions entry for \"" + name +
                                  "\" must not also appear in kinematics.numeric_substitutions");
    }
  }
  return evaluation_point;
}

ExactComplexRational EvaluateComplexCoefficientExpression(
    const std::string& expression,
    const NumericEvaluationPoint& evaluation_point) {
  return EvaluateExpressionToBigComplexRational(expression,
                                                ParseComplexEvaluationPoint(evaluation_point))
      .ToPublic();
}

ExactComplexRationalMatrix EvaluateComplexCoefficientMatrix(
    const DESystem& system,
    const std::string& variable_name,
    const NumericEvaluationPoint& evaluation_point) {
  const auto matrix_it = system.coefficient_matrices.find(variable_name);
  if (matrix_it == system.coefficient_matrices.end()) {
    throw std::invalid_argument("complex coefficient evaluation requires coefficient matrix for "
                                "variable \"" +
                                variable_name + "\"");
  }

  const ComplexBindingMap bindings = ParseComplexEvaluationPoint(evaluation_point);
  ExactComplexRationalMatrix evaluated_matrix;
  evaluated_matrix.reserve(matrix_it->second.size());
  for (const auto& row : matrix_it->second) {
    std::vector<ExactComplexRational> evaluated_row;
    evaluated_row.reserve(row.size());
    for (const auto& cell : row) {
      evaluated_row.push_back(EvaluateExpressionToBigComplexRational(cell, bindings).ToPublic());
    }
    evaluated_matrix.push_back(std::move(evaluated_row));
  }
  return evaluated_matrix;
}

ExactComplexRational EvaluateComplexPointExpression(
    const std::string& variable_name,
    const std::string& point_expression,
    const NumericEvaluationPoint& evaluation_point) {
  return EvaluateComplexPointExpressionImpl(variable_name,
                                            point_expression,
                                            ParseComplexEvaluationPoint(evaluation_point))
      .ToPublic();
}

}  // namespace amflow
