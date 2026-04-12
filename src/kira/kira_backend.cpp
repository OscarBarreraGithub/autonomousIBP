#include "amflow/kira/kira_backend.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace amflow {

namespace {

constexpr const char* kRequiredGeneratedFiles[] = {
    "config/integralfamilies.yaml",
    "config/kinematics.yaml",
    "jobs.yaml",
    "preferred",
    "target",
};

enum class StartupFailureStage {
  LogRedirection = 1,
  WorkingDirectory = 2,
  Execve = 3,
};

struct StartupFailureRecord {
  int stage = 0;
  int error_number = 0;
};

std::string Quote(const std::string& value) {
  return "\"" + value + "\"";
}

std::string Join(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << Quote(values[index]);
  }
  return out.str();
}

std::string SectorList(const std::vector<int>& sectors) {
  std::ostringstream out;
  for (std::size_t index = 0; index < sectors.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << sectors[index];
  }
  return out.str();
}

std::string TargetList(const std::vector<TargetIntegral>& targets) {
  std::ostringstream out;
  for (const auto& target : targets) {
    out << target.Label() << "\n";
  }
  return out.str();
}

std::string PreferredMasters(const std::vector<std::string>& preferred) {
  std::ostringstream out;
  for (const auto& value : preferred) {
    out << value << "\n\n";
  }
  return out.str();
}

std::string ShellQuote(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

std::string RenderCommand(const PreparedCommand& command) {
  std::ostringstream out;
  bool needs_separator = false;
  for (const auto& [key, value] : command.environment_overrides) {
    if (needs_separator) {
      out << " ";
    }
    out << key << "=" << ShellQuote(value);
    needs_separator = true;
  }
  if (needs_separator) {
    out << " ";
  }
  out << ShellQuote(command.executable.string());
  for (const auto& argument : command.arguments) {
    out << " " << ShellQuote(argument);
  }
  return out.str();
}

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

std::string StartupFailureStageToString(const int stage) {
  switch (static_cast<StartupFailureStage>(stage)) {
    case StartupFailureStage::LogRedirection:
      return "failed to redirect command logs";
    case StartupFailureStage::WorkingDirectory:
      return "failed to enter working directory";
    case StartupFailureStage::Execve:
      return "failed to exec kira executable";
  }
  return "failed to start kira";
}

std::string StatusToString(const CommandExecutionStatus status) {
  switch (status) {
    case CommandExecutionStatus::NotRun:
      return "not-run";
    case CommandExecutionStatus::Completed:
      return "completed";
    case CommandExecutionStatus::FailedToStart:
      return "failed-to-start";
    case CommandExecutionStatus::InvalidConfiguration:
      return "invalid-configuration";
    case CommandExecutionStatus::Signaled:
      return "signaled";
  }
  return "unknown";
}

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

bool IsIdentifierStart(const char character) {
  return std::isalpha(static_cast<unsigned char>(character)) != 0 ||
         character == '_';
}

bool IsIdentifierContinuation(const char character) {
  return std::isalnum(static_cast<unsigned char>(character)) != 0 ||
         character == '_';
}

std::string NormalizeExpression(std::string expression) {
  expression.erase(
      std::remove_if(expression.begin(),
                     expression.end(),
                     [](const char character) {
                       return std::isspace(static_cast<unsigned char>(character)) != 0;
                     }),
      expression.end());
  return expression;
}

bool EnclosesWholeExpression(const std::string& text,
                             const char open,
                             const char close) {
  if (text.size() < 2 || text.front() != open || text.back() != close) {
    return false;
  }

  int depth = 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char character = text[index];
    if (character == open) {
      ++depth;
    } else if (character == close) {
      --depth;
      if (depth == 0 && index + 1 != text.size()) {
        return false;
      }
      if (depth < 0) {
        return false;
      }
    }
  }
  return depth == 0;
}

std::string StripOuterParentheses(std::string expression) {
  expression = Trim(expression);
  while (EnclosesWholeExpression(expression, '(', ')')) {
    expression = Trim(expression.substr(1, expression.size() - 2));
  }
  return expression;
}

TargetIntegral ParseIntegralLabel(const std::string& text,
                                  const std::string& expected_family,
                                  const std::string& context,
                                  std::size_t* consumed_characters = nullptr) {
  std::size_t index = 0;
  while (index < text.size() &&
         std::isspace(static_cast<unsigned char>(text[index])) != 0) {
    ++index;
  }
  if (index >= text.size() || !IsIdentifierStart(text[index])) {
    throw std::runtime_error("expected integral label in " + context);
  }

  const std::size_t family_begin = index;
  ++index;
  while (index < text.size() && IsIdentifierContinuation(text[index])) {
    ++index;
  }
  const std::string family = text.substr(family_begin, index - family_begin);
  if (!expected_family.empty() && family != expected_family) {
    throw std::runtime_error("expected family \"" + expected_family + "\" in " + context +
                             ", found \"" + family + "\"");
  }

  while (index < text.size() &&
         std::isspace(static_cast<unsigned char>(text[index])) != 0) {
    ++index;
  }
  if (index >= text.size() || text[index] != '[') {
    throw std::runtime_error("expected '[' after integral family in " + context);
  }
  ++index;

  std::vector<int> indices;
  while (true) {
    while (index < text.size() &&
           std::isspace(static_cast<unsigned char>(text[index])) != 0) {
      ++index;
    }
    if (index >= text.size()) {
      throw std::runtime_error("unterminated integral label in " + context);
    }
    if (text[index] == ']') {
      if (indices.empty()) {
        throw std::runtime_error("integral label must contain at least one index in " +
                                 context);
      }
      ++index;
      break;
    }

    const std::size_t integer_begin = index;
    if (text[index] == '+' || text[index] == '-') {
      ++index;
    }
    const std::size_t digits_begin = index;
    while (index < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
      ++index;
    }
    if (digits_begin == index) {
      throw std::runtime_error("invalid integral index in " + context);
    }
    indices.push_back(std::stoi(text.substr(integer_begin, index - integer_begin)));

    while (index < text.size() &&
           std::isspace(static_cast<unsigned char>(text[index])) != 0) {
      ++index;
    }
    if (index >= text.size()) {
      throw std::runtime_error("unterminated integral label in " + context);
    }
    if (text[index] == ',') {
      ++index;
      continue;
    }
    if (text[index] == ']') {
      ++index;
      break;
    }
    throw std::runtime_error("expected ',' or ']' in " + context);
  }

  if (consumed_characters != nullptr) {
    *consumed_characters = index;
  } else {
    const std::string trailing = Trim(text.substr(index));
    if (!trailing.empty()) {
      throw std::runtime_error("unexpected trailing text after integral label in " +
                               context + ": " + trailing);
    }
  }

  TargetIntegral integral;
  integral.family = family;
  integral.indices = std::move(indices);
  return integral;
}

std::vector<std::string> SplitTopLevel(const std::string& text,
                                       const char delimiter,
                                       const std::string& context) {
  std::vector<std::string> values;
  std::string current;
  int parentheses_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (const char character : text) {
    switch (character) {
      case '(':
        ++parentheses_depth;
        break;
      case ')':
        --parentheses_depth;
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        --bracket_depth;
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        --brace_depth;
        break;
      default:
        break;
    }
    if (parentheses_depth < 0 || bracket_depth < 0 || brace_depth < 0) {
      throw std::runtime_error("unbalanced delimiters in " + context);
    }
    if (character == delimiter && parentheses_depth == 0 && bracket_depth == 0 &&
        brace_depth == 0) {
      const std::string trimmed = Trim(current);
      if (trimmed.empty()) {
        throw std::runtime_error("empty list element in " + context);
      }
      values.push_back(trimmed);
      current.clear();
      continue;
    }
    current.push_back(character);
  }

  if (parentheses_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
    throw std::runtime_error("unbalanced delimiters in " + context);
  }

  const std::string trimmed = Trim(current);
  if (!trimmed.empty()) {
    values.push_back(trimmed);
  } else if (!text.empty()) {
    throw std::runtime_error("empty list element in " + context);
  }
  return values;
}

bool IsUnarySign(const std::string& expression, const std::size_t index) {
  if (index == 0) {
    return true;
  }
  std::size_t previous = index;
  while (previous > 0) {
    --previous;
    if (std::isspace(static_cast<unsigned char>(expression[previous])) != 0) {
      continue;
    }
    return expression[previous] == '(' || expression[previous] == '[' ||
           expression[previous] == '{' || expression[previous] == ',' ||
           expression[previous] == '+' || expression[previous] == '-' ||
           expression[previous] == '*' || expression[previous] == '/' ||
           expression[previous] == '^';
  }
  return true;
}

std::vector<std::string> SplitTopLevelTerms(const std::string& expression,
                                            const std::string& context) {
  std::vector<std::string> terms;
  std::string current;
  int parentheses_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (std::size_t index = 0; index < expression.size(); ++index) {
    const char character = expression[index];
    if ((character == '+' || character == '-') && parentheses_depth == 0 &&
        bracket_depth == 0 && brace_depth == 0 && !IsUnarySign(expression, index)) {
      const std::string trimmed = Trim(current);
      if (trimmed.empty()) {
        throw std::runtime_error("empty reduction term in " + context);
      }
      terms.push_back(trimmed);
      current.clear();
    }

    current.push_back(character);
    switch (character) {
      case '(':
        ++parentheses_depth;
        break;
      case ')':
        --parentheses_depth;
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        --bracket_depth;
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        --brace_depth;
        break;
      default:
        break;
    }
    if (parentheses_depth < 0 || bracket_depth < 0 || brace_depth < 0) {
      throw std::runtime_error("unbalanced reduction term delimiters in " + context);
    }
  }

  if (parentheses_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
    throw std::runtime_error("unbalanced reduction term delimiters in " + context);
  }

  const std::string trimmed = Trim(current);
  if (trimmed.empty()) {
    throw std::runtime_error("empty reduction term in " + context);
  }
  terms.push_back(trimmed);
  return terms;
}

struct FactorizedExpression {
  std::vector<std::string> factors;
  std::vector<char> operators;
};

FactorizedExpression SplitTopLevelFactors(const std::string& expression,
                                          const std::string& context) {
  FactorizedExpression result;
  std::string current;
  int parentheses_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (std::size_t index = 0; index < expression.size(); ++index) {
    const char character = expression[index];
    if ((character == '*' || character == '/') && parentheses_depth == 0 &&
        bracket_depth == 0 && brace_depth == 0) {
      const std::string trimmed = Trim(current);
      if (trimmed.empty()) {
        throw std::runtime_error("empty reduction factor in " + context);
      }
      result.factors.push_back(trimmed);
      result.operators.push_back(character);
      current.clear();
      continue;
    }

    current.push_back(character);
    switch (character) {
      case '(':
        ++parentheses_depth;
        break;
      case ')':
        --parentheses_depth;
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        --bracket_depth;
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        --brace_depth;
        break;
      default:
        break;
    }
    if (parentheses_depth < 0 || bracket_depth < 0 || brace_depth < 0) {
      throw std::runtime_error("unbalanced reduction factor delimiters in " + context);
    }
  }

  if (parentheses_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
    throw std::runtime_error("unbalanced reduction factor delimiters in " + context);
  }

  const std::string trimmed = Trim(current);
  if (trimmed.empty()) {
    throw std::runtime_error("empty reduction factor in " + context);
  }
  result.factors.push_back(trimmed);
  return result;
}

std::size_t FindTopLevelArrow(const std::string& rule_text, const std::string& context) {
  int parentheses_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  std::size_t arrow_index = std::string::npos;

  for (std::size_t index = 0; index < rule_text.size(); ++index) {
    const char character = rule_text[index];
    switch (character) {
      case '(':
        ++parentheses_depth;
        break;
      case ')':
        --parentheses_depth;
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        --bracket_depth;
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        --brace_depth;
        break;
      default:
        break;
    }
    if (parentheses_depth < 0 || bracket_depth < 0 || brace_depth < 0) {
      throw std::runtime_error("unbalanced delimiters in " + context);
    }
    if (character == '-' && index + 1 < rule_text.size() && rule_text[index + 1] == '>' &&
        parentheses_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
      if (arrow_index != std::string::npos) {
        throw std::runtime_error("multiple top-level rules found in " + context);
      }
      arrow_index = index;
    }
  }

  if (parentheses_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
    throw std::runtime_error("unbalanced delimiters in " + context);
  }
  if (arrow_index == std::string::npos) {
    throw std::runtime_error("expected top-level '->' in " + context);
  }
  return arrow_index;
}

std::string NormalizeCoefficient(std::string coefficient) {
  coefficient = StripOuterParentheses(coefficient);
  coefficient = NormalizeExpression(coefficient);
  if (coefficient.empty() || coefficient == "+" || coefficient == "1" ||
      coefficient == "+1") {
    return "1";
  }
  if (coefficient == "-" || coefficient == "-1") {
    return "-1";
  }
  if (coefficient.rfind("+", 0) == 0) {
    coefficient.erase(0, 1);
  }
  while (coefficient.size() > 2 &&
         coefficient.compare(coefficient.size() - 2, 2, "*1") == 0) {
    coefficient.erase(coefficient.size() - 2, 2);
  }
  while (coefficient.size() > 2 && coefficient.compare(0, 2, "1*") == 0) {
    coefficient.erase(0, 2);
  }
  if (coefficient == "1/1") {
    return "1";
  }
  if (coefficient.size() > 2 &&
      coefficient.compare(coefficient.size() - 2, 2, "/1") == 0) {
    coefficient.erase(coefficient.size() - 2, 2);
  }
  return coefficient.empty() ? "1" : coefficient;
}

bool IsZeroExpression(const std::string& expression) {
  const std::string normalized = NormalizeExpression(StripOuterParentheses(expression));
  return normalized == "0" || normalized == "+0" || normalized == "-0";
}

bool IsIntegerLiteral(const std::string& expression) {
  if (expression.empty()) {
    return false;
  }
  return std::all_of(expression.begin(), expression.end(), [](const char character) {
    return std::isdigit(static_cast<unsigned char>(character)) != 0;
  });
}

bool ParseLongLongLiteral(const std::string& expression, long long* value) {
  if (!IsIntegerLiteral(expression)) {
    return false;
  }
  try {
    std::size_t consumed = 0;
    const long long parsed = std::stoll(expression, &consumed, 10);
    if (consumed != expression.size()) {
      return false;
    }
    if (value != nullptr) {
      *value = parsed;
    }
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool HasTopLevelAdditiveOperator(const std::string& expression) {
  int parentheses_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (std::size_t index = 0; index < expression.size(); ++index) {
    const char character = expression[index];
    switch (character) {
      case '(':
        ++parentheses_depth;
        break;
      case ')':
        --parentheses_depth;
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        --bracket_depth;
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        --brace_depth;
        break;
      default:
        break;
    }
    if (parentheses_depth < 0 || bracket_depth < 0 || brace_depth < 0) {
      return true;
    }
    if ((character == '+' || character == '-') && parentheses_depth == 0 &&
        bracket_depth == 0 && brace_depth == 0 && !IsUnarySign(expression, index)) {
      return true;
    }
  }

  return false;
}

struct LocatedIntegral {
  TargetIntegral integral;
  std::size_t begin = 0;
  std::size_t end = 0;
};

std::vector<LocatedIntegral> FindIntegralOccurrences(const std::string& term,
                                                     const std::string& family,
                                                     const std::string& context) {
  std::vector<LocatedIntegral> located;

  for (std::size_t index = 0; index < term.size();) {
    if (!IsIdentifierStart(term[index])) {
      ++index;
      continue;
    }
    std::size_t end = index + 1;
    while (end < term.size() && IsIdentifierContinuation(term[end])) {
      ++end;
    }
    if (term.substr(index, end - index) != family) {
      index = end;
      continue;
    }

    std::size_t consumed = 0;
    const TargetIntegral integral =
        ParseIntegralLabel(term.substr(index), family, context, &consumed);
    located.push_back(LocatedIntegral{integral, index, index + consumed});
    index += consumed;
  }

  return located;
}

bool ParseSignedIntegralFactor(const std::string& factor,
                               const std::string& family,
                               const std::string& context,
                               TargetIntegral* integral,
                               int* sign) {
  std::string normalized = StripOuterParentheses(Trim(factor));
  int local_sign = 1;
  if (!normalized.empty() && (normalized.front() == '+' || normalized.front() == '-')) {
    local_sign = normalized.front() == '-' ? -1 : 1;
    normalized = StripOuterParentheses(Trim(normalized.substr(1)));
  }
  if (normalized.empty()) {
    return false;
  }

  try {
    if (integral != nullptr) {
      *integral = ParseIntegralLabel(normalized, family, context);
    } else {
      static_cast<void>(ParseIntegralLabel(normalized, family, context));
    }
  } catch (const std::runtime_error&) {
    return false;
  }

  if (sign != nullptr) {
    *sign = local_sign;
  }
  return true;
}

std::string RebuildCoefficientSource(const FactorizedExpression& factors,
                                     const std::size_t integral_factor_index,
                                     const int integral_sign) {
  std::ostringstream coefficient;
  for (std::size_t index = 0; index < factors.factors.size(); ++index) {
    if (index > 0) {
      coefficient << factors.operators[index - 1];
      if (index == integral_factor_index) {
        coefficient << (integral_sign < 0 ? "-1" : "1");
      } else {
        coefficient << factors.factors[index];
      }
      continue;
    }

    if (index == integral_factor_index) {
      coefficient << (integral_sign < 0 ? "-1" : "1");
    } else {
      coefficient << factors.factors[index];
    }
  }
  return coefficient.str();
}

ParsedReductionTerm ParseLinearReductionTerm(const std::string& term,
                                             const std::string& family,
                                             const std::string& context) {
  const FactorizedExpression factors =
      SplitTopLevelFactors(StripOuterParentheses(term), context);
  std::size_t total_occurrences = 0;
  std::size_t integral_factor_index = factors.factors.size();
  int integral_sign = 1;
  TargetIntegral integral;

  for (std::size_t index = 0; index < factors.factors.size(); ++index) {
    const std::vector<LocatedIntegral> occurrences =
        FindIntegralOccurrences(factors.factors[index], family, context);
    total_occurrences += occurrences.size();
    if (occurrences.empty()) {
      continue;
    }

    TargetIntegral factor_integral;
    int factor_sign = 1;
    if (occurrences.size() != 1 ||
        !ParseSignedIntegralFactor(factors.factors[index], family, context, &factor_integral,
                                   &factor_sign) ||
        (index > 0 && factors.operators[index - 1] == '/')) {
      throw std::runtime_error("reduction term must be linear in masters in " + context +
                               ": " + Trim(term));
    }

    if (integral_factor_index != factors.factors.size()) {
      throw std::runtime_error("reduction term must be linear in masters in " + context +
                               ": " + Trim(term));
    }

    integral_factor_index = index;
    integral_sign = factor_sign;
    integral = factor_integral;
  }

  if (total_occurrences == 0) {
    throw std::runtime_error("reduction term must contain exactly one integral in " + context +
                             ": " + Trim(term));
  }
  if (total_occurrences != 1 || integral_factor_index == factors.factors.size()) {
    throw std::runtime_error("reduction term must be linear in masters in " + context +
                             ": " + Trim(term));
  }

  ParsedReductionTerm parsed;
  parsed.master = integral;
  parsed.coefficient = NormalizeCoefficient(
      RebuildCoefficientSource(factors, integral_factor_index, integral_sign));
  return parsed;
}

std::string RenderCoefficientComponent(const long long multiplicity,
                                       const std::string& body) {
  const long long absolute_multiplicity =
      multiplicity < 0 ? -multiplicity : multiplicity;
  if (absolute_multiplicity == 1) {
    if (HasTopLevelAdditiveOperator(body)) {
      return "(" + body + ")";
    }
    return body;
  }
  if (HasTopLevelAdditiveOperator(body)) {
    return std::to_string(absolute_multiplicity) + "*(" + body + ")";
  }
  return std::to_string(absolute_multiplicity) + "*" + body;
}

std::string CanonicalizeCoefficientSum(const std::vector<std::string>& coefficients) {
  long long numeric_total = 0;
  std::map<std::string, long long> symbolic_totals;

  for (const auto& raw_coefficient : coefficients) {
    const std::string coefficient = NormalizeCoefficient(raw_coefficient);
    if (IsZeroExpression(coefficient)) {
      continue;
    }

    int sign = 1;
    std::string body = coefficient;
    if (!body.empty() && (body.front() == '+' || body.front() == '-')) {
      sign = body.front() == '-' ? -1 : 1;
      body.erase(0, 1);
    }
    if (body.empty()) {
      body = "1";
    }

    long long integer_value = 0;
    if (ParseLongLongLiteral(body, &integer_value)) {
      numeric_total += sign * integer_value;
      continue;
    }

    symbolic_totals[body] += sign;
  }

  std::vector<std::pair<int, std::string>> components;
  if (numeric_total != 0) {
    components.push_back(
        {numeric_total < 0 ? -1 : 1,
         std::to_string(numeric_total < 0 ? -numeric_total : numeric_total)});
  }

  for (const auto& [body, multiplicity] : symbolic_totals) {
    if (multiplicity == 0) {
      continue;
    }
    components.push_back(
        {multiplicity < 0 ? -1 : 1, RenderCoefficientComponent(multiplicity, body)});
  }

  if (components.empty()) {
    return "0";
  }

  std::ostringstream combined;
  for (std::size_t index = 0; index < components.size(); ++index) {
    const auto& [sign, component] = components[index];
    if (index == 0) {
      if (sign < 0) {
        combined << "-";
      }
      combined << component;
      continue;
    }
    combined << (sign < 0 ? "-" : "+") << component;
  }
  return NormalizeCoefficient(combined.str());
}

ParsedReductionRule CanonicalizeReductionRule(const ParsedReductionRule& rule) {
  std::map<std::string, std::pair<TargetIntegral, std::vector<std::string>>> grouped_terms;
  for (const auto& term : rule.terms) {
    auto& grouped = grouped_terms[term.master.Label()];
    grouped.first = term.master;
    grouped.second.push_back(term.coefficient);
  }

  ParsedReductionRule canonical;
  canonical.target = rule.target;
  for (const auto& [_, grouped] : grouped_terms) {
    const std::string coefficient = CanonicalizeCoefficientSum(grouped.second);
    if (IsZeroExpression(coefficient)) {
      continue;
    }
    canonical.terms.push_back(ParsedReductionTerm{coefficient, grouped.first});
  }
  return canonical;
}

std::vector<ParsedReductionRule> MakeIdentityRules(
    const std::vector<TargetIntegral>& masters) {
  std::vector<ParsedReductionRule> rules;
  rules.reserve(masters.size());
  for (const auto& master : masters) {
    ParsedReductionRule rule;
    rule.target = master;
    rule.terms.push_back(ParsedReductionTerm{"1", master});
    rules.push_back(std::move(rule));
  }
  return rules;
}

std::filesystem::path FamilyResultsDir(const std::filesystem::path& artifact_root,
                                       const std::string& family) {
  return artifact_root / "results" / family;
}

std::string ReadTextFileOrThrow(const std::filesystem::path& path,
                                const std::string& description) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error(description + " is missing: " + path.string());
  }
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error(description + " is not a regular file: " + path.string());
  }
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open " + description + ": " + path.string());
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

std::vector<std::string> ValidateExplicitTargets(const ProblemSpec& spec,
                                                 const std::vector<TargetIntegral>& targets) {
  std::vector<std::string> messages;
  if (targets.empty()) {
    messages.emplace_back("explicit Kira target list must not be empty");
    return messages;
  }

  std::set<std::string> seen_targets;
  for (const auto& target : targets) {
    const std::string label = target.Label();
    if (!seen_targets.insert(label).second) {
      messages.emplace_back("duplicate explicit Kira target: " + label);
    }
    if (target.family != spec.family.name) {
      messages.emplace_back("explicit Kira target family does not match family.name: " +
                            label);
    }
    if (target.indices.size() != spec.family.propagators.size()) {
      messages.emplace_back("explicit Kira target index count must match family.propagators "
                            "size: " +
                            label);
    }
  }
  return messages;
}

bool IsExecutableFile(const std::filesystem::path& path) {
  return !path.empty() && std::filesystem::exists(path) && ::access(path.c_str(), X_OK) == 0;
}

std::map<std::string, std::string> MergeEnvironment(
    const std::map<std::string, std::string>& overrides) {
  std::map<std::string, std::string> merged;
  if (environ != nullptr) {
    for (char** entry = environ; *entry != nullptr; ++entry) {
      const std::string pair(*entry);
      const std::size_t separator = pair.find('=');
      if (separator == std::string::npos) {
        continue;
      }
      merged.emplace(pair.substr(0, separator), pair.substr(separator + 1));
    }
  }
  for (const auto& [key, value] : overrides) {
    merged[key] = value;
  }
  return merged;
}

bool WriteTextFile(const std::filesystem::path& path,
                   const std::string& content,
                   std::string* error_message) {
  try {
    std::filesystem::create_directories(path.parent_path());
  } catch (const std::exception& error) {
    if (error_message != nullptr) {
      *error_message =
          "failed to create log directory for " + path.string() + ": " + error.what();
    }
    return false;
  }

  std::ofstream stream(path, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    if (error_message != nullptr) {
      *error_message = "failed to open log file " + path.string();
    }
    return false;
  }
  stream << content;
  if (!stream) {
    if (error_message != nullptr) {
      *error_message = "failed to write log file " + path.string();
    }
    return false;
  }
  stream.close();
  if (!stream) {
    if (error_message != nullptr) {
      *error_message = "failed to finalize log file " + path.string();
    }
    return false;
  }
  return true;
}

bool WriteRejectedLogs(const PreparedCommand& command,
                       const CommandLogPaths& log_paths,
                       const CommandExecutionStatus status,
                       const std::string& error_message,
                       std::string* log_error) {
  std::ostringstream stdout_log;
  stdout_log << "command: " << RenderCommand(command) << "\n";
  stdout_log << "working_directory: " << command.working_directory.string() << "\n";
  stdout_log << "status: " << StatusToString(status) << "\n";
  if (!command.environment_overrides.empty()) {
    stdout_log << "environment_overrides:\n";
    for (const auto& [key, value] : command.environment_overrides) {
      stdout_log << "  " << key << "=" << value << "\n";
    }
  }
  if (!error_message.empty()) {
    stdout_log << "error: " << error_message << "\n";
  }

  std::ostringstream stderr_log;
  stderr_log << "status: " << StatusToString(status) << "\n";
  stderr_log << "working_directory: " << command.working_directory.string() << "\n";
  if (!error_message.empty()) {
    stderr_log << "error: " << error_message << "\n";
  }

  std::string first_error;
  if (!WriteTextFile(log_paths.stdout_path, stdout_log.str(), &first_error)) {
    if (log_error != nullptr) {
      *log_error = first_error;
    }
    return false;
  }
  if (!WriteTextFile(log_paths.stderr_path, stderr_log.str(), &first_error)) {
    if (log_error != nullptr) {
      *log_error = first_error;
    }
    return false;
  }
  if (log_error != nullptr) {
    log_error->clear();
  }
  return true;
}

bool SetCloseOnExec(const int file_descriptor, std::string* error_message) {
  const int flags = ::fcntl(file_descriptor, F_GETFD);
  if (flags < 0) {
    if (error_message != nullptr) {
      *error_message =
          "failed to read file-descriptor flags: " + std::string(std::strerror(errno));
    }
    return false;
  }
  if (::fcntl(file_descriptor, F_SETFD, flags | FD_CLOEXEC) != 0) {
    if (error_message != nullptr) {
      *error_message =
          "failed to set close-on-exec flag: " + std::string(std::strerror(errno));
    }
    return false;
  }
  return true;
}

void WriteStartupFailureRecord(const int file_descriptor,
                               const StartupFailureStage stage,
                               const int error_number) {
  const StartupFailureRecord record{static_cast<int>(stage), error_number};
  const auto* bytes = reinterpret_cast<const char*>(&record);
  std::size_t written = 0;
  while (written < sizeof(record)) {
    const ssize_t count =
        ::write(file_descriptor, bytes + written, sizeof(record) - written);
    if (count <= 0) {
      break;
    }
    written += static_cast<std::size_t>(count);
  }
}

std::string DescribeStartupFailure(const StartupFailureRecord& record) {
  std::string message = StartupFailureStageToString(record.stage);
  if (record.error_number != 0) {
    message += ": ";
    message += std::strerror(record.error_number);
  }
  return message;
}

bool ReadStartupFailureRecord(const int file_descriptor,
                              StartupFailureRecord* record,
                              std::string* error_message) {
  StartupFailureRecord local_record;
  const ssize_t count = ::read(file_descriptor, &local_record, sizeof(local_record));
  if (count == 0) {
    if (record != nullptr) {
      *record = StartupFailureRecord{};
    }
    return false;
  }
  if (count < 0) {
    if (error_message != nullptr) {
      *error_message =
          "failed to read startup status: " + std::string(std::strerror(errno));
    }
    return true;
  }
  if (count != static_cast<ssize_t>(sizeof(local_record))) {
    if (error_message != nullptr) {
      *error_message = "failed to read startup status: short startup-status record";
    }
    return true;
  }
  if (record != nullptr) {
    *record = local_record;
  }
  return true;
}

std::vector<std::string> ValidatePreparedFiles(const BackendPreparation& preparation,
                                               const ArtifactLayout& layout) {
  std::vector<std::string> messages;
  for (const char* required_file : kRequiredGeneratedFiles) {
    const std::string relative_path(required_file);
    if (preparation.generated_files.find(relative_path) == preparation.generated_files.end()) {
      messages.emplace_back("prepared Kira file was not generated: " + relative_path);
      continue;
    }

    const std::filesystem::path path = layout.generated_config_dir / relative_path;
    if (!std::filesystem::exists(path)) {
      messages.emplace_back("prepared Kira file is missing: " + path.string());
      continue;
    }
    if (!std::filesystem::is_regular_file(path)) {
      messages.emplace_back("prepared Kira file is not a regular file: " + path.string());
    }
  }
  return messages;
}

CommandExecutionResult MakeRejectedResult(const PreparedCommand& command,
                                          const CommandLogPaths& log_paths,
                                          CommandExecutionStatus status,
                                          const std::string& error_message) {
  CommandExecutionResult result;
  result.command = RenderCommand(command);
  result.working_directory = command.working_directory;
  result.stdout_log_path = log_paths.stdout_path;
  result.stderr_log_path = log_paths.stderr_path;
  result.attempt_number = log_paths.attempt_number;
  result.environment_overrides = command.environment_overrides;
  result.status = status;
  result.error_message = error_message;
  std::string log_error;
  if (!WriteRejectedLogs(command, log_paths, status, error_message, &log_error) &&
      !log_error.empty()) {
    result.error_message += "; failed to initialize command logs: " + log_error;
  }
  return result;
}

}  // namespace

std::string KiraBackend::Name() const {
  return "KiraBackend";
}

std::vector<std::string> KiraBackend::Validate(const ProblemSpec& spec,
                                               const ReductionOptions& options) const {
  std::vector<std::string> messages = ValidateProblemSpec(spec);
  if (options.ibp_reducer != "Kira") {
    messages.emplace_back("KiraBackend requires ReductionOptions.ibp_reducer == \"Kira\"");
  }
  if (spec.family.top_level_sectors.empty()) {
    messages.emplace_back("KiraBackend requires at least one top-level sector");
  }
  return messages;
}

KiraJobFiles KiraBackend::EmitJobFiles(const ProblemSpec& spec,
                                       const ReductionOptions& options) const {
  return EmitJobFilesForTargets(spec, options, spec.targets);
}

KiraJobFiles KiraBackend::EmitJobFilesForTargets(
    const ProblemSpec& spec,
    const ReductionOptions& options,
    const std::vector<TargetIntegral>& targets) const {
  KiraJobFiles files;

  std::ostringstream family_yaml;
  family_yaml << "integralfamilies:\n";
  family_yaml << "  - name: " << Quote(spec.family.name) << "\n";
  family_yaml << "    loop_momenta: [" << Join(spec.family.loop_momenta) << "]\n";
  family_yaml << "    top_level_sectors: [" << SectorList(spec.family.top_level_sectors) << "]\n";
  if (options.permutation_option.has_value()) {
    family_yaml << "    permutation_option: " << *options.permutation_option << "\n";
  }
  family_yaml << "    propagators:\n";
  for (const auto& propagator : spec.family.propagators) {
    family_yaml << "      - [" << Quote(propagator.expression) << ", "
                << Quote(propagator.mass) << "]\n";
  }
  files.integralfamilies_yaml = family_yaml.str();

  std::ostringstream kinematics_yaml;
  kinematics_yaml << "kinematics:\n";
  kinematics_yaml << "  incoming_momenta: [" << Join(spec.kinematics.incoming_momenta) << "]\n";
  kinematics_yaml << "  outgoing_momenta: [" << Join(spec.kinematics.outgoing_momenta) << "]\n";
  kinematics_yaml << "  momentum_conservation: [" << Quote(spec.kinematics.momentum_conservation) << "]\n";
  kinematics_yaml << "  kinematic_invariants:\n";
  for (const auto& invariant : spec.kinematics.invariants) {
    kinematics_yaml << "    - [" << Quote(invariant) << ", 2]\n";
  }
  if (!spec.kinematics.scalar_product_rules.empty()) {
    kinematics_yaml << "  scalarproduct_rules:\n";
    for (const auto& rule : spec.kinematics.scalar_product_rules) {
      kinematics_yaml << "    - [" << Quote(rule.left) << ", " << Quote(rule.right) << "]\n";
    }
  }
  files.kinematics_yaml = kinematics_yaml.str();

  std::ostringstream jobs_yaml;
  jobs_yaml << "jobs:\n";
  jobs_yaml << "  - reduce_sectors:\n";
  jobs_yaml << "      reduce:\n";
  jobs_yaml << "        - {topologies: [" << Quote(spec.family.name) << "], sectors: ["
           << SectorList(spec.family.top_level_sectors) << "], r: "
           << spec.family.propagators.size() << ", s: " << options.black_box_rank
           << ", d: " << options.black_box_dot << "}\n";
  jobs_yaml << "      select_integrals:\n";
  jobs_yaml << "        select_mandatory_list:\n";
  jobs_yaml << "          - [" << Quote(spec.family.name) << ", target]\n";
  jobs_yaml << "      preferred_masters: preferred\n";
  jobs_yaml << "      integral_ordering: " << options.integral_order << "\n";
  switch (options.reduction_mode) {
    case ReductionMode::Kira:
      jobs_yaml << "      run_initiate: true\n";
      jobs_yaml << "      run_triangular: true\n";
      jobs_yaml << "      run_back_substitution: true\n";
      break;
    case ReductionMode::FireFly:
      jobs_yaml << "      run_initiate: true\n";
      jobs_yaml << "      run_firefly: true\n";
      break;
    case ReductionMode::Mixed:
      jobs_yaml << "      run_initiate: true\n";
      jobs_yaml << "      run_triangular: true\n";
      jobs_yaml << "      run_firefly: back\n";
      break;
    case ReductionMode::NoFactorScan:
      jobs_yaml << "      run_initiate: true\n";
      jobs_yaml << "      run_triangular: true\n";
      jobs_yaml << "      run_firefly: back\n";
      jobs_yaml << "      factor_scan: false\n";
      break;
    case ReductionMode::Masters:
      jobs_yaml << "      run_initiate: masters\n";
      break;
  }
  files.jobs_yaml = jobs_yaml.str();

  files.preferred_masters = PreferredMasters(spec.family.preferred_masters);
  files.target_list = TargetList(targets);
  return files;
}

BackendPreparation KiraBackend::Prepare(const ProblemSpec& spec,
                                        const ReductionOptions& options,
                                        const ArtifactLayout& layout) const {
  return PrepareForTargets(spec, options, layout, spec.targets);
}

BackendPreparation KiraBackend::PrepareForTargets(
    const ProblemSpec& spec,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::vector<TargetIntegral>& targets) const {
  BackendPreparation preparation;
  preparation.backend_name = Name();
  preparation.validation_messages = Validate(spec, options);
  const std::vector<std::string> target_messages = ValidateExplicitTargets(spec, targets);
  preparation.validation_messages.insert(preparation.validation_messages.end(),
                                         target_messages.begin(),
                                         target_messages.end());

  const KiraJobFiles files = EmitJobFilesForTargets(spec, options, targets);
  preparation.generated_files = {
      {"config/integralfamilies.yaml", files.integralfamilies_yaml},
      {"config/kinematics.yaml", files.kinematics_yaml},
      {"jobs.yaml", files.jobs_yaml},
      {"preferred", files.preferred_masters},
      {"target", files.target_list},
  };

  std::ostringstream command;
  command << "FERMATPATH=<fermat-executable> kira "
          << (layout.generated_config_dir / "jobs.yaml").string();
  preparation.commands.push_back(command.str());
  return preparation;
}

PreparedCommand KiraBackend::MakeExecutionCommand(
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) const {
  PreparedCommand command;
  command.label = "kira";
  command.executable = std::filesystem::absolute(kira_executable);
  command.arguments = {std::filesystem::absolute(layout.generated_config_dir / "jobs.yaml").string()};
  command.working_directory = layout.generated_config_dir;
  command.environment_overrides = {
      {"FERMATPATH", std::filesystem::absolute(fermat_executable).string()},
  };
  return command;
}

ParsedMasterList KiraBackend::ParseMasterList(const std::filesystem::path& artifact_root,
                                              const std::string& family) const {
  if (family.empty()) {
    throw std::runtime_error("family must not be empty when parsing Kira masters");
  }

  ParsedMasterList result;
  result.family = family;
  result.source_path = FamilyResultsDir(artifact_root, family) / "masters";

  std::ifstream stream(result.source_path);
  if (!stream.is_open()) {
    if (!std::filesystem::exists(result.source_path)) {
      throw std::runtime_error("Kira masters file is missing: " +
                               result.source_path.string());
    }
    throw std::runtime_error("failed to open Kira masters file: " +
                             result.source_path.string());
  }

  std::set<std::string> seen_labels;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(stream, line)) {
    ++line_number;
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
      continue;
    }

    std::size_t token_end = 0;
    while (token_end < trimmed.size() &&
           std::isspace(static_cast<unsigned char>(trimmed[token_end])) == 0) {
      ++token_end;
    }
    const std::string token = trimmed.substr(0, token_end);
    TargetIntegral integral = ParseIntegralLabel(
        token, family,
        "Kira masters file " + result.source_path.string() + ":" +
            std::to_string(line_number));
    const std::string label = integral.Label();
    if (!seen_labels.insert(label).second) {
      throw std::runtime_error("duplicate master integral in Kira masters file " +
                               result.source_path.string() + ":" +
                               std::to_string(line_number) + ": " + label);
    }
    result.masters.push_back(std::move(integral));
  }

  if (!stream.eof()) {
    throw std::runtime_error("failed to read Kira masters file: " +
                             result.source_path.string());
  }
  return result;
}

ParsedReductionResult KiraBackend::ParseReductionResult(
    const std::filesystem::path& artifact_root,
    const std::string& family) const {
  ParsedReductionResult result;
  result.master_list = ParseMasterList(artifact_root, family);
  result.rule_path = FamilyResultsDir(artifact_root, family) / "kira_target.m";

  std::set<std::string> master_labels;
  for (const auto& master : result.master_list.masters) {
    master_labels.insert(master.Label());
  }

  if (!std::filesystem::exists(result.rule_path)) {
    result.status = ParsedReductionStatus::IdentityFallback;
    result.rules = MakeIdentityRules(result.master_list.masters);
    return result;
  }

  std::string content = Trim(ReadTextFileOrThrow(result.rule_path, "Kira reduction rule file"));
  if (!content.empty() && content.back() == ';') {
    content.pop_back();
    content = Trim(content);
  }
  if (content.empty() || content == "{}") {
    result.status = ParsedReductionStatus::IdentityFallback;
    result.rules = MakeIdentityRules(result.master_list.masters);
    return result;
  }
  if (content.front() != '{' || content.back() != '}') {
    throw std::runtime_error("Kira reduction rule file must be a Mathematica list: " +
                             result.rule_path.string());
  }

  const std::vector<std::string> entries = SplitTopLevel(
      content.substr(1, content.size() - 2), ',', "Kira reduction rule file " +
                                                     result.rule_path.string());
  std::set<std::string> seen_targets;
  std::vector<ParsedReductionRule> explicit_rules;
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const std::string entry_context =
        "Kira reduction rule file " + result.rule_path.string() + " entry " +
        std::to_string(index + 1);
    const std::size_t arrow_index = FindTopLevelArrow(entries[index], entry_context);
    TargetIntegral target = ParseIntegralLabel(entries[index].substr(0, arrow_index), family,
                                               entry_context + " target");
    const std::string target_label = target.Label();
    if (!seen_targets.insert(target_label).second) {
      throw std::runtime_error("duplicate reduction target in " + entry_context + ": " +
                               target_label);
    }

    std::string rhs = StripOuterParentheses(entries[index].substr(arrow_index + 2));
    if (IsZeroExpression(rhs)) {
      continue;
    }

    ParsedReductionRule rule;
    rule.target = target;
    const std::vector<std::string> terms = SplitTopLevelTerms(rhs, entry_context + " rhs");
    for (const auto& term_text : terms) {
      const ParsedReductionTerm term =
          ParseLinearReductionTerm(term_text, family, entry_context + " rhs");
      const std::string label = term.master.Label();
      if (master_labels.find(label) == master_labels.end()) {
        throw std::runtime_error("reduction rule references unknown master integral in " +
                                 entry_context + ": " + label);
      }
      rule.terms.push_back(std::move(term));
    }
    rule = CanonicalizeReductionRule(rule);
    if (!rule.terms.empty()) {
      explicit_rules.push_back(std::move(rule));
    }
  }

  result.explicit_rule_count = explicit_rules.size();
  if (explicit_rules.empty()) {
    result.status = ParsedReductionStatus::IdentityFallback;
    result.rules = MakeIdentityRules(result.master_list.masters);
    return result;
  }

  result.status = ParsedReductionStatus::ParsedRules;
  result.rules = explicit_rules;
  const std::vector<ParsedReductionRule> identity_rules =
      MakeIdentityRules(result.master_list.masters);
  result.rules.insert(result.rules.end(), identity_rules.begin(), identity_rules.end());
  return result;
}

CommandExecutionResult KiraBackend::ExecutePrepared(
    const BackendPreparation& preparation,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable) const {
  const PreparedCommand command = MakeExecutionCommand(layout, kira_executable, fermat_executable);
  CommandLogPaths log_paths = MakeCommandLogPaths(layout, command.label);

  if (!preparation.validation_messages.empty()) {
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                              JoinMessages(preparation.validation_messages));
  }

  if (!IsExecutableFile(command.executable)) {
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                              "kira executable is missing or not executable: " +
                                  command.executable.string());
  }

  const auto fermat_it = command.environment_overrides.find("FERMATPATH");
  if (fermat_it == command.environment_overrides.end() ||
      !IsExecutableFile(fermat_it->second)) {
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                              "FERMATPATH executable is missing or not executable: " +
                                  (fermat_it == command.environment_overrides.end()
                                       ? std::string("<unset>")
                                       : fermat_it->second));
  }

  const std::vector<std::string> write_messages = WritePreparationFiles(preparation, layout);
  if (!write_messages.empty()) {
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                              JoinMessages(write_messages));
  }

  const std::vector<std::string> prepared_file_messages =
      ValidatePreparedFiles(preparation, layout);
  if (!prepared_file_messages.empty()) {
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                              JoinMessages(prepared_file_messages));
  }

  int startup_pipe[2] = {-1, -1};
  if (::pipe(startup_pipe) != 0) {
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::FailedToStart,
                              "failed to create startup-status pipe: " +
                                  std::string(std::strerror(errno)));
  }
  std::string close_on_exec_error;
  if (!SetCloseOnExec(startup_pipe[1], &close_on_exec_error)) {
    ::close(startup_pipe[0]);
    ::close(startup_pipe[1]);
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::FailedToStart,
                              close_on_exec_error);
  }

  int stdout_fd = -1;
  int stderr_fd = -1;
  while (true) {
    log_paths = MakeCommandLogPaths(layout, command.label);
    stdout_fd = ::open(log_paths.stdout_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (stdout_fd < 0) {
      if (errno == EEXIST) {
        continue;
      }
      ::close(startup_pipe[0]);
      ::close(startup_pipe[1]);
      return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                                "failed to open stdout log: " +
                                    std::string(std::strerror(errno)));
    }

    stderr_fd = ::open(log_paths.stderr_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (stderr_fd >= 0) {
      break;
    }

    const int stderr_errno = errno;
    ::close(stdout_fd);
    stdout_fd = -1;
    std::error_code ignored_error;
    std::filesystem::remove(log_paths.stdout_path, ignored_error);
    if (stderr_errno == EEXIST) {
      continue;
    }

    ::close(startup_pipe[0]);
    ::close(startup_pipe[1]);
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::InvalidConfiguration,
                              "failed to open stderr log: " +
                                  std::string(std::strerror(stderr_errno)));
  }

  const std::map<std::string, std::string> merged_environment =
      MergeEnvironment(command.environment_overrides);

  std::vector<std::string> argv_storage;
  argv_storage.reserve(command.arguments.size() + 1);
  argv_storage.push_back(command.executable.string());
  for (const auto& argument : command.arguments) {
    argv_storage.push_back(argument);
  }
  std::vector<char*> argv_data;
  argv_data.reserve(argv_storage.size() + 1);
  for (auto& value : argv_storage) {
    argv_data.push_back(value.data());
  }
  argv_data.push_back(nullptr);

  std::vector<std::string> env_storage;
  env_storage.reserve(merged_environment.size());
  for (const auto& [key, value] : merged_environment) {
    env_storage.push_back(key + "=" + value);
  }
  std::vector<char*> env_data;
  env_data.reserve(env_storage.size() + 1);
  for (auto& value : env_storage) {
    env_data.push_back(value.data());
  }
  env_data.push_back(nullptr);

  CommandExecutionResult result;
  result.command = RenderCommand(command);
  result.working_directory = command.working_directory;
  result.stdout_log_path = log_paths.stdout_path;
  result.stderr_log_path = log_paths.stderr_path;
  result.attempt_number = log_paths.attempt_number;
  result.environment_overrides = command.environment_overrides;

  const pid_t child = ::fork();
  if (child < 0) {
    const std::string error_message = std::string("failed to launch Kira: ") +
                                      std::strerror(errno);
    ::close(startup_pipe[0]);
    ::close(startup_pipe[1]);
    ::close(stdout_fd);
    ::close(stderr_fd);
    return MakeRejectedResult(command, log_paths, CommandExecutionStatus::FailedToStart,
                              error_message);
  }

  if (child == 0) {
    ::close(startup_pipe[0]);
    if (::dup2(stdout_fd, STDOUT_FILENO) < 0 || ::dup2(stderr_fd, STDERR_FILENO) < 0) {
      const int duplicate_error = errno;
      WriteStartupFailureRecord(startup_pipe[1], StartupFailureStage::LogRedirection,
                                duplicate_error);
      const std::string message =
          "failed to redirect command logs: " + std::string(std::strerror(duplicate_error)) +
          "\n";
      ::write(STDERR_FILENO, message.data(), message.size());
      _exit(126);
    }
    ::close(stdout_fd);
    ::close(stderr_fd);
    if (!command.working_directory.empty() &&
        ::chdir(command.working_directory.c_str()) != 0) {
      const int chdir_error = errno;
      WriteStartupFailureRecord(startup_pipe[1], StartupFailureStage::WorkingDirectory,
                                chdir_error);
      const std::string message =
          "failed to enter working directory: " + std::string(std::strerror(chdir_error)) +
          "\n";
      ::write(STDERR_FILENO, message.data(), message.size());
      _exit(126);
    }
    ::execve(command.executable.c_str(), argv_data.data(), env_data.data());
    const int exec_error = errno;
    WriteStartupFailureRecord(startup_pipe[1], StartupFailureStage::Execve, exec_error);
    const std::string message =
        "failed to exec kira executable: " + std::string(std::strerror(exec_error)) + "\n";
    ::write(STDERR_FILENO, message.data(), message.size());
    _exit(127);
  }

  ::close(startup_pipe[1]);
  ::close(stdout_fd);
  ::close(stderr_fd);

  int wait_status = 0;
  if (::waitpid(child, &wait_status, 0) < 0) {
    ::close(startup_pipe[0]);
    result.status = CommandExecutionStatus::FailedToStart;
    result.error_message = std::string("failed to wait for Kira: ") + std::strerror(errno);
    return result;
  }

  StartupFailureRecord startup_failure;
  std::string startup_status_error;
  const bool has_startup_failure =
      ReadStartupFailureRecord(startup_pipe[0], &startup_failure, &startup_status_error);
  ::close(startup_pipe[0]);

  if (!startup_status_error.empty()) {
    result.status = CommandExecutionStatus::FailedToStart;
    result.error_message = startup_status_error;
    return result;
  }

  if (has_startup_failure) {
    result.status = CommandExecutionStatus::FailedToStart;
    result.error_message = DescribeStartupFailure(startup_failure);
    return result;
  }

  if (WIFEXITED(wait_status)) {
    result.status = CommandExecutionStatus::Completed;
    result.exit_code = WEXITSTATUS(wait_status);
    if (result.exit_code != 0) {
      result.error_message = "kira exited with code " + std::to_string(result.exit_code);
    }
    return result;
  }

  if (WIFSIGNALED(wait_status)) {
    result.status = CommandExecutionStatus::Signaled;
    result.exit_code = 128 + WTERMSIG(wait_status);
    result.error_message = "kira terminated by signal " + std::to_string(WTERMSIG(wait_status));
    return result;
  }

  result.status = CommandExecutionStatus::FailedToStart;
  result.error_message = "kira exited with an unrecognized wait status";
  return result;
}

std::vector<std::string> WritePreparationFiles(const BackendPreparation& preparation,
                                               const ArtifactLayout& layout) {
  std::vector<std::string> messages;
  for (const auto& [relative_path, content] : preparation.generated_files) {
    const std::filesystem::path destination = layout.generated_config_dir / relative_path;
    try {
      std::filesystem::create_directories(destination.parent_path());
    } catch (const std::exception& error) {
      messages.emplace_back("failed to create directory for " + destination.string() + ": " +
                            error.what());
      continue;
    }
    std::ofstream stream(destination, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
      messages.emplace_back("failed to open prepared file for writing: " +
                            destination.string());
      continue;
    }
    stream << content;
    if (!stream) {
      messages.emplace_back("failed to write prepared file: " + destination.string());
      continue;
    }
    stream.close();
    if (!stream) {
      messages.emplace_back("failed to finalize prepared file: " + destination.string());
    }
  }
  return messages;
}

}  // namespace amflow
