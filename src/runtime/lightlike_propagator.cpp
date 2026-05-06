#include "amflow/runtime/lightlike_propagator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

std::string RemoveAsciiSpaces(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](const unsigned char current) {
                               return std::isspace(current) != 0;
                             }),
              value.end());
  return value;
}

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

std::string MasterLabel(const MasterIntegral& master) {
  if (!master.label.empty()) {
    return master.label;
  }
  return IntegralLabel(master.family, master.indices);
}

std::string TargetLabel(const TargetIntegral& target) {
  return IntegralLabel(target.family, target.indices);
}

bool VectorEquals(const std::vector<std::string>& lhs,
                  const std::vector<std::string>& rhs) {
  return lhs == rhs;
}

bool HasBoundaryFile(const LightlikeGaugeLinkRuntimeState& state,
                     const std::string& name) {
  return std::find(state.boundary_file_names.begin(),
                   state.boundary_file_names.end(),
                   name) != state.boundary_file_names.end();
}

bool HasCanonicalSingularPoint(const LightlikeGaugeLinkRuntimeState& state,
                               const std::string& point) {
  const std::string canonical = RemoveAsciiSpaces(point);
  return std::any_of(state.singular_points.begin(),
                     state.singular_points.end(),
                     [&canonical](const std::string& candidate) {
                       return RemoveAsciiSpaces(candidate) == canonical;
                     });
}

std::vector<std::string> ExternalMomenta(const ProblemSpec& spec) {
  std::vector<std::string> momenta = spec.kinematics.incoming_momenta;
  momenta.insert(momenta.end(),
                 spec.kinematics.outgoing_momenta.begin(),
                 spec.kinematics.outgoing_momenta.end());
  return momenta;
}

bool HasGaugeLinkReplacement(const ProblemSpec& spec) {
  for (const ScalarProductRule& rule : spec.kinematics.scalar_product_rules) {
    const std::string lhs = RemoveAsciiSpaces(rule.left);
    const std::string rhs = RemoveAsciiSpaces(rule.right);
    if ((lhs == "n^2" || lhs == "n*n" || lhs == "n.n") && rhs == "-1") {
      return true;
    }
  }
  return false;
}

const std::vector<std::string>& ReviewedPropagators() {
  static const std::vector<std::string> propagators = {
      "l1^2",
      "l2^2",
      "l3^2",
      "1+l1*n",
      "1/2+l1*n+l2*n+l3*n",
      "l1^2+2*l1*l2+l2^2",
      "l1^2+2*l1*l3+l3^2",
      "l2^2+2*l2*l3+l3^2",
      "-1+l2^2+2*l2*n",
  };
  return propagators;
}

const std::vector<std::string>& GeneratedSquarePropagators() {
  static const std::vector<std::string> propagators = {
      "l1^2",
      "l2^2",
      "l3^2",
      "l1^2 + (1 + l1*n)/gaugex",
      "(l1 + l2 + l3)^2 + (1/2 + l1*n + l2*n + l3*n)/gaugex",
      "l1^2 + 2*l1*l2 + l2^2",
      "l1^2 + 2*l1*l3 + l3^2",
      "l2^2 + 2*l2*l3 + l3^2",
      "-1 + l2^2 + 2*l2*n",
  };
  return propagators;
}

const std::vector<std::size_t>& AffectedPropagatorIndices() {
  static const std::vector<std::size_t> indices = {3, 4};
  return indices;
}

const std::vector<std::string>& ReviewedTargetLabels() {
  static const std::vector<std::string> labels = {
      "gauge[1,1,1,1,1,0,-1,0,0]",
      "gauge[1,1,1,1,1,0,0,-1,0]",
      "gauge[1,1,1,1,1,0,0,0,-1]",
      "gauge[1,1,1,0,1,0,0,0,0]",
      "gauge[1,1,1,-1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,-1,0,0,0]",
      "gauge[1,1,1,1,1,0,0,0,0]",
      "gauge[1,1,1,1,1,-1,0,0,0]",
  };
  return labels;
}

const std::vector<std::string>& ReviewedSelectedEndpointTargetLabels() {
  static const std::vector<std::string> labels = {
      "gauge[1,1,1,0,1,0,0,0,0]",
      "gauge[1,1,1,-1,1,0,0,0,0]",
      "gauge[1,1,1,1,1,0,0,0,0]",
      "gauge[1,1,1,1,1,-1,0,0,0]",
  };
  return labels;
}

const std::vector<std::string>& ReviewedFirstEndpointTargetLabels() {
  static const std::vector<std::string> labels = {
      ReviewedSelectedEndpointTargetLabels().front(),
  };
  return labels;
}

const std::vector<std::string>& ReviewedReductionMasterLabels() {
  static const std::vector<std::string> labels = {
      "gauge[1,1,1,0,1,0,0,0,0]",
      "gauge[1,1,1,-1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,-1,0,0,0]",
      "gauge[1,1,1,1,1,0,0,0,0]",
      "gauge[1,1,1,1,1,-1,0,0,0]",
  };
  return labels;
}

bool LabelsExactlyMatch(const std::vector<TargetIntegral>& values,
                        const std::vector<std::string>& expected) {
  if (values.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (TargetLabel(values[index]) != expected[index]) {
      return false;
    }
  }
  return true;
}

bool LabelsExactlyMatch(const std::vector<MasterIntegral>& values,
                        const std::vector<std::string>& expected) {
  if (values.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (MasterLabel(values[index]) != expected[index]) {
      return false;
    }
  }
  return true;
}

bool LabelsMatchReviewedGaugeLinkTargets(
    const std::vector<TargetIntegral>& values) {
  if (LabelsExactlyMatch(values, ReviewedTargetLabels())) {
    return true;
  }
  if (values.empty() || values.size() > ReviewedSelectedEndpointTargetLabels().size()) {
    return false;
  }
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (TargetLabel(values[index]) !=
        ReviewedSelectedEndpointTargetLabels()[index]) {
      return false;
    }
  }
  return true;
}

bool LabelsContainAll(const std::vector<MasterIntegral>& haystack,
                      const std::vector<MasterIntegral>& needles) {
  std::set<std::string> labels;
  for (const MasterIntegral& master : haystack) {
    labels.insert(MasterLabel(master));
  }
  return std::all_of(needles.begin(),
                     needles.end(),
                     [&labels](const MasterIntegral& master) {
                       return labels.find(MasterLabel(master)) != labels.end();
                     });
}

void RequireReviewedGaugeLinkSourceSurface(const ProblemSpec& spec) {
  if (spec.family.name != "gauge") {
    throw std::runtime_error(
        "b64ag gauge-link scaffold is intentionally limited to family gauge");
  }
  if (!VectorEquals(spec.family.loop_momenta, {"l1", "l2", "l3"})) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires loop momenta {l1,l2,l3}");
  }
  if (!VectorEquals(ExternalMomenta(spec), {"n"})) {
    throw std::runtime_error("b64ag gauge-link scaffold requires external leg {n}");
  }
  if (!HasGaugeLinkReplacement(spec)) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires AMFlow replacement n^2 -> -1");
  }
  if (spec.family.propagators.size() != ReviewedPropagators().size()) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires the exact nine-propagator source surface");
  }
  for (std::size_t index = 0; index < ReviewedPropagators().size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (RemoveAsciiSpaces(propagator.expression) != ReviewedPropagators()[index]) {
      throw std::runtime_error(
          "b64ag gauge-link scaffold rejects non-reviewed denominator at propagator " +
          std::to_string(index));
    }
    if (RemoveAsciiSpaces(propagator.mass) != "0") {
      throw std::runtime_error(
          "b64ag gauge-link scaffold requires zero mass metadata on propagator " +
          std::to_string(index));
    }
    if (propagator.kind == PropagatorKind::Cut) {
      throw std::runtime_error(
          "b64ag gauge-link scaffold rejects cut propagators on the loop gauge-link surface");
    }
  }
  if (!spec.targets.empty() && !LabelsMatchReviewedGaugeLinkTargets(spec.targets)) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires the reviewed packet surface or the reviewed "
        "selected endpoint target prefix");
  }
}

std::string NormalizationFactor(const std::string& variable, const int affected_power_sum) {
  const int exponent = -affected_power_sum;
  if (exponent == 0) {
    return "1";
  }
  return variable + "^(" + std::to_string(exponent) + ")";
}

LightlikeGaugeLinkRuntimeState RequireRuntimeState(
    const LightlikeGaugeLinkRuntimeState& state) {
  if (!IsLightlikeGaugeLinkEtaZeroRuntimeState(state)) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold was invoked for a non-linear_propagator gaugex=0 state");
  }
  return state;
}

std::string TrimAsciiWhitespace(const std::string& value) {
  const std::size_t begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const std::size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitMathematicaListElements(const std::string& raw_list) {
  const std::string list = TrimAsciiWhitespace(raw_list);
  if (list.size() < 2 || list.front() != '{' || list.back() != '}') {
    throw std::runtime_error("b64ag gauge-link parser expected a Mathematica list");
  }
  const std::string body = TrimAsciiWhitespace(list.substr(1, list.size() - 2));
  if (body.empty()) {
    return {};
  }

  std::vector<std::string> elements;
  int brace_depth = 0;
  int paren_depth = 0;
  int bracket_depth = 0;
  std::size_t element_begin = 1;
  for (std::size_t index = 1; index + 1 < list.size(); ++index) {
    const char current = list[index];
    if (current == '{') {
      ++brace_depth;
    } else if (current == '}') {
      --brace_depth;
    } else if (current == '(') {
      ++paren_depth;
    } else if (current == ')') {
      --paren_depth;
    } else if (current == '[') {
      ++bracket_depth;
    } else if (current == ']') {
      --bracket_depth;
    } else if (current == ',' && brace_depth == 0 && paren_depth == 0 &&
               bracket_depth == 0) {
      elements.push_back(TrimAsciiWhitespace(
          list.substr(element_begin, index - element_begin)));
      element_begin = index + 1;
    }
    if (brace_depth < 0 || paren_depth < 0 || bracket_depth < 0) {
      throw std::runtime_error("b64ag gauge-link parser found unbalanced list syntax");
    }
  }
  elements.push_back(TrimAsciiWhitespace(
      list.substr(element_begin, list.size() - 1 - element_begin)));
  return elements;
}

std::string NormalizeRuntimeExpression(std::string expression) {
  expression = RemoveAsciiSpaces(std::move(expression));
  std::string normalized;
  normalized.reserve(expression.size());
  for (std::size_t index = 0; index < expression.size(); ++index) {
    if (expression[index] == '`') {
      ++index;
      while (index < expression.size() &&
             (std::isdigit(static_cast<unsigned char>(expression[index])) != 0 ||
              expression[index] == '.')) {
        ++index;
      }
      --index;
      continue;
    }
    if (expression[index] == '*' && index + 1 < expression.size() &&
        expression[index + 1] == '^') {
      normalized.push_back('e');
      ++index;
      continue;
    }
    normalized.push_back(expression[index]);
  }
  return normalized;
}

using RuntimeComplex = std::complex<long double>;

constexpr long double kRuntimeTiny = 1e-18L;

bool IsRuntimeTiny(const RuntimeComplex& value) {
  return std::abs(value) < kRuntimeTiny;
}

struct RuntimePolynomial {
  std::vector<RuntimeComplex> coefficients;
};

void PruneRuntimePolynomial(RuntimePolynomial& polynomial) {
  while (!polynomial.coefficients.empty() &&
         IsRuntimeTiny(polynomial.coefficients.back())) {
    polynomial.coefficients.pop_back();
  }
}

RuntimePolynomial MakeRuntimeConstantPolynomial(const RuntimeComplex& value) {
  RuntimePolynomial polynomial;
  if (!IsRuntimeTiny(value)) {
    polynomial.coefficients.push_back(value);
  }
  return polynomial;
}

RuntimePolynomial MakeRuntimeVariablePolynomial() {
  RuntimePolynomial polynomial;
  polynomial.coefficients.assign(2, RuntimeComplex{0.0L, 0.0L});
  polynomial.coefficients[1] = RuntimeComplex{1.0L, 0.0L};
  return polynomial;
}

RuntimePolynomial AddRuntimePolynomials(RuntimePolynomial lhs,
                                        const RuntimePolynomial& rhs,
                                        const long double rhs_sign = 1.0L) {
  if (lhs.coefficients.size() < rhs.coefficients.size()) {
    lhs.coefficients.resize(rhs.coefficients.size(), RuntimeComplex{0.0L, 0.0L});
  }
  for (std::size_t index = 0; index < rhs.coefficients.size(); ++index) {
    lhs.coefficients[index] += rhs.coefficients[index] * rhs_sign;
  }
  PruneRuntimePolynomial(lhs);
  return lhs;
}

RuntimePolynomial MultiplyRuntimePolynomials(const RuntimePolynomial& lhs,
                                             const RuntimePolynomial& rhs) {
  if (lhs.coefficients.empty() || rhs.coefficients.empty()) {
    return {};
  }
  RuntimePolynomial product;
  product.coefficients.assign(lhs.coefficients.size() + rhs.coefficients.size() - 1,
                              RuntimeComplex{0.0L, 0.0L});
  for (std::size_t lhs_index = 0; lhs_index < lhs.coefficients.size(); ++lhs_index) {
    for (std::size_t rhs_index = 0; rhs_index < rhs.coefficients.size(); ++rhs_index) {
      product.coefficients[lhs_index + rhs_index] +=
          lhs.coefficients[lhs_index] * rhs.coefficients[rhs_index];
    }
  }
  PruneRuntimePolynomial(product);
  return product;
}

struct RuntimeRationalPolynomial {
  RuntimePolynomial numerator;
  RuntimePolynomial denominator =
      MakeRuntimeConstantPolynomial(RuntimeComplex{1.0L, 0.0L});
};

RuntimeRationalPolynomial AddRuntimeRationals(const RuntimeRationalPolynomial& lhs,
                                              const RuntimeRationalPolynomial& rhs,
                                              const long double rhs_sign = 1.0L) {
  RuntimeRationalPolynomial result;
  result.numerator = AddRuntimePolynomials(
      MultiplyRuntimePolynomials(lhs.numerator, rhs.denominator),
      MultiplyRuntimePolynomials(rhs.numerator, lhs.denominator),
      rhs_sign);
  result.denominator = MultiplyRuntimePolynomials(lhs.denominator, rhs.denominator);
  return result;
}

RuntimeRationalPolynomial MultiplyRuntimeRationals(
    const RuntimeRationalPolynomial& lhs,
    const RuntimeRationalPolynomial& rhs) {
  RuntimeRationalPolynomial result;
  result.numerator = MultiplyRuntimePolynomials(lhs.numerator, rhs.numerator);
  result.denominator = MultiplyRuntimePolynomials(lhs.denominator, rhs.denominator);
  return result;
}

RuntimeRationalPolynomial DivideRuntimeRationals(const RuntimeRationalPolynomial& lhs,
                                                 const RuntimeRationalPolynomial& rhs,
                                                 const std::string& expression) {
  if (rhs.numerator.coefficients.empty()) {
    throw std::runtime_error("b64ag gauge-link rational parser divides by zero in \"" +
                             expression + "\"");
  }
  RuntimeRationalPolynomial result;
  result.numerator = MultiplyRuntimePolynomials(lhs.numerator, rhs.denominator);
  result.denominator = MultiplyRuntimePolynomials(lhs.denominator, rhs.numerator);
  return result;
}

RuntimeRationalPolynomial PowerRuntimeRational(RuntimeRationalPolynomial base,
                                               int exponent,
                                               const std::string& expression) {
  if (exponent < 0) {
    std::swap(base.numerator, base.denominator);
    exponent = -exponent;
  }
  RuntimeRationalPolynomial result;
  result.numerator = MakeRuntimeConstantPolynomial(RuntimeComplex{1.0L, 0.0L});
  result.denominator = MakeRuntimeConstantPolynomial(RuntimeComplex{1.0L, 0.0L});
  for (int index = 0; index < exponent; ++index) {
    result = MultiplyRuntimeRationals(result, base);
  }
  if (result.denominator.coefficients.empty()) {
    throw std::runtime_error(
        "b64ag gauge-link rational parser produced an empty denominator in \"" +
        expression + "\"");
  }
  return result;
}

long double ParseRuntimeRationalNumber(const std::string& raw_value) {
  const std::string value = RemoveAsciiSpaces(raw_value);
  const std::size_t slash = value.find('/');
  if (slash != std::string::npos) {
    const long double numerator = ParseRuntimeRationalNumber(value.substr(0, slash));
    const long double denominator = ParseRuntimeRationalNumber(value.substr(slash + 1));
    if (std::abs(denominator) < kRuntimeTiny) {
      throw std::runtime_error("b64ag gauge-link parser found a zero rational denominator");
    }
    return numerator / denominator;
  }
  return std::stold(value);
}

class GaugeLinkRationalParser {
 public:
  GaugeLinkRationalParser(std::string expression,
                          std::string variable,
                          const long double epsilon_sample)
      : expression_(NormalizeRuntimeExpression(std::move(expression))),
        variable_(std::move(variable)),
        epsilon_sample_(epsilon_sample) {}

  RuntimeRationalPolynomial Parse() {
    if (expression_.empty()) {
      throw std::runtime_error("b64ag gauge-link rational parser received an empty input");
    }
    RuntimeRationalPolynomial value = ParseExpression();
    if (position_ != expression_.size()) {
      throw std::runtime_error("b64ag gauge-link rational parser found trailing input in \"" +
                               expression_ + "\"");
    }
    return value;
  }

 private:
  bool Consume(const char expected) {
    if (position_ < expression_.size() && expression_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  bool StartsPrimary() const {
    if (position_ >= expression_.size()) {
      return false;
    }
    const char current = expression_[position_];
    return current == '(' ||
           std::isdigit(static_cast<unsigned char>(current)) != 0 ||
           current == '.' ||
           std::isalpha(static_cast<unsigned char>(current)) != 0 ||
           current == '_';
  }

  RuntimeRationalPolynomial ParseExpression() {
    RuntimeRationalPolynomial value = ParseTerm();
    while (position_ < expression_.size()) {
      if (Consume('+')) {
        value = AddRuntimeRationals(value, ParseTerm());
      } else if (Consume('-')) {
        value = AddRuntimeRationals(value, ParseTerm(), -1.0L);
      } else {
        break;
      }
    }
    return value;
  }

  RuntimeRationalPolynomial ParseTerm() {
    RuntimeRationalPolynomial value = ParsePower();
    while (position_ < expression_.size()) {
      if (Consume('*')) {
        value = MultiplyRuntimeRationals(value, ParsePower());
      } else if (Consume('/')) {
        value = DivideRuntimeRationals(value, ParsePower(), expression_);
      } else if (StartsPrimary()) {
        value = MultiplyRuntimeRationals(value, ParsePower());
      } else {
        break;
      }
    }
    return value;
  }

  RuntimeRationalPolynomial ParsePower() {
    RuntimeRationalPolynomial value = ParseUnary();
    if (Consume('^')) {
      value = PowerRuntimeRational(value, ParseSignedIntegerExponent(), expression_);
    }
    return value;
  }

  RuntimeRationalPolynomial ParseUnary() {
    if (Consume('+')) {
      return ParseUnary();
    }
    if (Consume('-')) {
      RuntimeRationalPolynomial value = ParseUnary();
      for (RuntimeComplex& coefficient : value.numerator.coefficients) {
        coefficient = -coefficient;
      }
      return value;
    }
    return ParsePrimary();
  }

  int ParseSignedIntegerExponent() {
    bool parenthesized = false;
    if (Consume('(')) {
      parenthesized = true;
    }
    bool negative = false;
    if (Consume('+')) {
      negative = false;
    } else if (Consume('-')) {
      negative = true;
    }
    if (position_ >= expression_.size() ||
        std::isdigit(static_cast<unsigned char>(expression_[position_])) == 0) {
      throw std::runtime_error(
          "b64ag gauge-link rational parser requires integer exponents in \"" +
          expression_ + "\"");
    }
    int exponent = 0;
    while (position_ < expression_.size() &&
           std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
      exponent = exponent * 10 + (expression_[position_] - '0');
      ++position_;
    }
    if (parenthesized && !Consume(')')) {
      throw std::runtime_error(
          "b64ag gauge-link rational parser expected ')' after exponent in \"" +
          expression_ + "\"");
    }
    return negative ? -exponent : exponent;
  }

  RuntimeRationalPolynomial Constant(const RuntimeComplex& value) const {
    RuntimeRationalPolynomial result;
    result.numerator = MakeRuntimeConstantPolynomial(value);
    result.denominator = MakeRuntimeConstantPolynomial(RuntimeComplex{1.0L, 0.0L});
    return result;
  }

  RuntimeRationalPolynomial ParsePrimary() {
    if (Consume('(')) {
      RuntimeRationalPolynomial value = ParseExpression();
      if (!Consume(')')) {
        throw std::runtime_error("b64ag gauge-link rational parser expected ')' in \"" +
                                 expression_ + "\"");
      }
      return value;
    }

    if (position_ < expression_.size() &&
        (std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0 ||
         expression_[position_] == '.')) {
      const std::size_t begin = position_;
      bool saw_digit = false;
      while (position_ < expression_.size() &&
             std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
        saw_digit = true;
        ++position_;
      }
      if (Consume('.')) {
        while (position_ < expression_.size() &&
               std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
          saw_digit = true;
          ++position_;
        }
      }
      if (!saw_digit) {
        throw std::runtime_error(
            "b64ag gauge-link rational parser found malformed number in \"" +
            expression_ + "\"");
      }
      if (position_ < expression_.size() &&
          (expression_[position_] == 'e' || expression_[position_] == 'E')) {
        ++position_;
        if (position_ < expression_.size() &&
            (expression_[position_] == '+' || expression_[position_] == '-')) {
          ++position_;
        }
        if (position_ >= expression_.size() ||
            std::isdigit(static_cast<unsigned char>(expression_[position_])) == 0) {
          throw std::runtime_error(
              "b64ag gauge-link rational parser found malformed exponent in \"" +
              expression_ + "\"");
        }
        while (position_ < expression_.size() &&
               std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
          ++position_;
        }
      }
      return Constant(RuntimeComplex{
          std::stold(expression_.substr(begin, position_ - begin)), 0.0L});
    }

    if (position_ < expression_.size() &&
        (std::isalpha(static_cast<unsigned char>(expression_[position_])) != 0 ||
         expression_[position_] == '_')) {
      const std::size_t begin = position_;
      ++position_;
      while (position_ < expression_.size() &&
             (std::isalnum(static_cast<unsigned char>(expression_[position_])) != 0 ||
              expression_[position_] == '_')) {
        ++position_;
      }
      const std::string identifier = expression_.substr(begin, position_ - begin);
      if (identifier == "I") {
        return Constant(RuntimeComplex{0.0L, 1.0L});
      }
      if (identifier == variable_) {
        RuntimeRationalPolynomial result;
        result.numerator = MakeRuntimeVariablePolynomial();
        result.denominator =
            MakeRuntimeConstantPolynomial(RuntimeComplex{1.0L, 0.0L});
        return result;
      }
      if (identifier == "eps") {
        return Constant(RuntimeComplex{epsilon_sample_, 0.0L});
      }
      throw std::runtime_error("b64ag gauge-link rational parser requires a binding for " +
                               identifier + " in \"" + expression_ + "\"");
    }

    throw std::runtime_error("b64ag gauge-link rational parser found malformed expression \"" +
                             expression_ + "\"");
  }

  std::string expression_;
  std::string variable_;
  long double epsilon_sample_ = 0.0L;
  std::size_t position_ = 0;
};

RuntimeRationalPolynomial ParseGaugeLinkRationalExpression(
    const std::string& expression,
    const std::string& variable,
    const long double epsilon_sample) {
  return GaugeLinkRationalParser(expression, variable, epsilon_sample).Parse();
}

int RuntimePolynomialDegree(const RuntimePolynomial& polynomial) {
  for (std::size_t reverse_index = polynomial.coefficients.size();
       reverse_index > 0;
       --reverse_index) {
    const std::size_t index = reverse_index - 1;
    if (!IsRuntimeTiny(polynomial.coefficients[index])) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

std::vector<RuntimeComplex> RuntimePolynomialRootsDurandKerner(
    const RuntimePolynomial& polynomial) {
  const int degree = RuntimePolynomialDegree(polynomial);
  if (degree <= 0) {
    return {};
  }
  const RuntimeComplex leading = polynomial.coefficients[static_cast<std::size_t>(degree)];
  if (IsRuntimeTiny(leading)) {
    return {};
  }
  if (degree == 1) {
    return {-polynomial.coefficients[0] / leading};
  }

  std::vector<RuntimeComplex> normalized_coefficients(static_cast<std::size_t>(degree));
  long double radius = 1.0L;
  for (int index = 0; index < degree; ++index) {
    normalized_coefficients[static_cast<std::size_t>(index)] =
        polynomial.coefficients[static_cast<std::size_t>(index)] / leading;
    radius = std::max(
        radius,
        1.0L + std::abs(normalized_coefficients[static_cast<std::size_t>(index)]));
  }

  constexpr long double kPi = 3.141592653589793238462643383279502884L;
  std::vector<RuntimeComplex> roots(static_cast<std::size_t>(degree));
  for (int index = 0; index < degree; ++index) {
    const long double angle =
        2.0L * kPi * static_cast<long double>(index) /
            static_cast<long double>(degree) +
        0.37L;
    roots[static_cast<std::size_t>(index)] = std::polar(radius, angle);
  }

  auto evaluate = [&](const RuntimeComplex& variable_value) {
    RuntimeComplex result = RuntimeComplex{1.0L, 0.0L};
    for (int index = degree - 1; index >= 0; --index) {
      result = result * variable_value +
               normalized_coefficients[static_cast<std::size_t>(index)];
    }
    return result;
  };

  for (int iteration = 0; iteration < 400; ++iteration) {
    long double max_delta = 0.0L;
    for (int root_index = 0; root_index < degree; ++root_index) {
      RuntimeComplex denominator = RuntimeComplex{1.0L, 0.0L};
      for (int other_index = 0; other_index < degree; ++other_index) {
        if (other_index == root_index) {
          continue;
        }
        denominator *= roots[static_cast<std::size_t>(root_index)] -
                       roots[static_cast<std::size_t>(other_index)];
      }
      if (std::abs(denominator) < 1e-30L) {
        denominator += RuntimeComplex{1e-24L, 1e-24L};
      }
      const RuntimeComplex delta =
          evaluate(roots[static_cast<std::size_t>(root_index)]) / denominator;
      roots[static_cast<std::size_t>(root_index)] -= delta;
      max_delta = std::max(max_delta, std::abs(delta));
    }
    if (max_delta < 1e-18L) {
      break;
    }
  }
  return roots;
}

std::string FormatLongDouble(const long double raw_value,
                             const int precision_digits = 24) {
  long double value = raw_value;
  if (std::abs(value) < 1e-15L) {
    value = 0.0L;
  }
  std::ostringstream stream;
  stream << std::setprecision(precision_digits) << value;
  std::string text = stream.str();
  const std::size_t exponent = text.find_first_of("eE");
  const std::size_t dot = text.find('.');
  if (dot != std::string::npos && exponent == std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  return text.empty() ? "0" : text;
}

std::string FormatRuntimeComplex(const RuntimeComplex& value,
                                 const int precision_digits = 24) {
  const long double real = std::abs(value.real()) < 1e-15L ? 0.0L : value.real();
  const long double imaginary =
      std::abs(value.imag()) < 1e-15L ? 0.0L : value.imag();
  const std::string real_text = FormatLongDouble(real, precision_digits);
  const std::string imaginary_text =
      FormatLongDouble(std::abs(imaginary), precision_digits);
  if (std::abs(imaginary) < 1e-15L) {
    return real_text;
  }
  if (std::abs(real) < 1e-15L) {
    return (imaginary < 0 ? "-" : "") + imaginary_text + "*I";
  }
  return real_text + (imaginary < 0 ? " - " : " + ") + imaginary_text + "*I";
}

struct RuntimePoleAudit {
  RuntimeComplex value;
  int multiplicity = 0;
  std::vector<std::string> sources;
};

bool SameRuntimePole(const RuntimeComplex& lhs, const RuntimeComplex& rhs) {
  return std::abs(lhs - rhs) < 1e-7L;
}

void AddRuntimePole(std::vector<RuntimePoleAudit>& poles,
                    const RuntimeComplex& value,
                    const std::string& source) {
  for (RuntimePoleAudit& pole : poles) {
    if (SameRuntimePole(pole.value, value)) {
      ++pole.multiplicity;
      if (pole.sources.size() < 4) {
        pole.sources.push_back(source);
      }
      return;
    }
  }
  RuntimePoleAudit pole;
  pole.value = value;
  pole.multiplicity = 1;
  pole.sources.push_back(source);
  poles.push_back(std::move(pole));
}

std::vector<LightlikeGaugeLinkPoleAudit> PublicPoleAudits(
    const std::vector<RuntimePoleAudit>& poles) {
  std::vector<LightlikeGaugeLinkPoleAudit> public_poles;
  public_poles.reserve(poles.size());
  for (const RuntimePoleAudit& pole : poles) {
    public_poles.push_back(
        {FormatRuntimeComplex(pole.value, 24), pole.multiplicity, pole.sources});
  }
  return public_poles;
}

std::vector<RuntimePoleAudit> ExtractRuntimePolesFromMatrix(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable,
    const long double epsilon_sample,
    std::size_t* nonzero_cell_count) {
  std::vector<RuntimePoleAudit> poles;
  if (nonzero_cell_count != nullptr) {
    *nonzero_cell_count = 0;
  }
  for (std::size_t row_index = 0; row_index < matrix.size(); ++row_index) {
    for (std::size_t column_index = 0; column_index < matrix[row_index].size();
         ++column_index) {
      const RuntimeRationalPolynomial rational =
          ParseGaugeLinkRationalExpression(matrix[row_index][column_index],
                                           variable,
                                           epsilon_sample);
      if (nonzero_cell_count != nullptr && !rational.numerator.coefficients.empty()) {
        ++(*nonzero_cell_count);
      }
      const int degree = RuntimePolynomialDegree(rational.denominator);
      if (degree <= 0) {
        continue;
      }
      for (const RuntimeComplex& root :
           RuntimePolynomialRootsDurandKerner(rational.denominator)) {
        AddRuntimePole(poles,
                       root,
                       "gaugex_matrix[" + std::to_string(row_index) + "," +
                           std::to_string(column_index) + "]");
      }
    }
  }
  std::sort(poles.begin(),
            poles.end(),
            [](const RuntimePoleAudit& lhs, const RuntimePoleAudit& rhs) {
              if (std::abs(lhs.value.real() - rhs.value.real()) > 1e-12L) {
                return lhs.value.real() < rhs.value.real();
              }
              return lhs.value.imag() < rhs.value.imag();
            });
  return poles;
}

RuntimeComplex ParseRuntimePointValue(const std::string& raw_point,
                                      const std::string& variable) {
  std::string value = raw_point;
  const std::size_t arrow = value.find("->");
  const std::size_t equals = value.find('=');
  if (arrow != std::string::npos) {
    value = value.substr(arrow + 2);
  } else if (equals != std::string::npos) {
    value = value.substr(equals + 1);
  }
  const RuntimeRationalPolynomial rational =
      ParseGaugeLinkRationalExpression(value, variable, 0.0L);
  if (RuntimePolynomialDegree(rational.numerator) > 0 ||
      RuntimePolynomialDegree(rational.denominator) > 0 ||
      rational.denominator.coefficients.empty()) {
    throw std::runtime_error("b64ag gauge-link contour point is not numeric: " +
                             raw_point);
  }
  if (rational.numerator.coefficients.empty()) {
    return RuntimeComplex{0.0L, 0.0L};
  }
  return rational.numerator.coefficients.front() /
         rational.denominator.coefficients.front();
}

long double DistancePointToSegment(const RuntimeComplex& point,
                                   const RuntimeComplex& start,
                                   const RuntimeComplex& end) {
  const RuntimeComplex segment = end - start;
  const RuntimeComplex offset = point - start;
  const long double length_squared = std::norm(segment);
  if (length_squared < kRuntimeTiny) {
    return std::abs(offset);
  }
  long double projection =
      (offset.real() * segment.real() + offset.imag() * segment.imag()) /
      length_squared;
  if (projection < 0.0L) {
    projection = 0.0L;
  } else if (projection > 1.0L) {
    projection = 1.0L;
  }
  const RuntimeComplex closest = start + segment * projection;
  return std::abs(point - closest);
}

std::string SerializeGaugeLinkContourPlanForFingerprint(
    const LightlikeGaugeLinkContourPlanAudit& plan) {
  std::ostringstream out;
  out << "kind=b64ag-gauge-link-gaugex-contour-plan\n";
  out << "variable=" << plan.variable << "\n";
  out << "desolver_local_variable=" << plan.desolver_local_variable << "\n";
  out << "boundary_point=" << plan.boundary_point << "\n";
  out << "target_point=" << plan.target_point << "\n";
  out << "half_plane=" << plan.half_plane << "\n";
  out << "matrix_rows=" << plan.matrix_row_count << "\n";
  out << "matrix_columns=" << plan.matrix_column_count << "\n";
  out << "waypoints=" << plan.waypoints.size() << "\n";
  for (std::size_t index = 0; index < plan.waypoints.size(); ++index) {
    out << "waypoint[" << index << "]=" << plan.waypoints[index] << "\n";
  }
  out << "poles=" << plan.poles.size() << "\n";
  for (std::size_t index = 0; index < plan.poles.size(); ++index) {
    out << "pole[" << index << "]=" << plan.poles[index].value
        << ";multiplicity=" << plan.poles[index].multiplicity << "\n";
  }
  out << "endpoint_local_model_kind=" << plan.endpoint_local_model_kind << "\n";
  out << "dropped_term_audit=" << plan.dropped_term_audit << "\n";
  return out.str();
}

std::string SerializeGaugeLinkSelectedCoefficientAuditForFingerprint(
    const LightlikeGaugeLinkSelectedCoefficientAudit& audit) {
  std::ostringstream out;
  out << "kind=b64ag-gauge-link-selected-endpoint-coefficient\n";
  out << "master=" << audit.master_label << "\n";
  out << "runtime_application=" << audit.runtime_application << "\n";
  out << "transport_scope=" << audit.transport_scope << "\n";
  out << "endpoint_local_model_kind=" << audit.endpoint_local_model_kind << "\n";
  out << "contour_fingerprint=" << audit.contour_fingerprint << "\n";
  out << "eta_zero_selection_audit=" << audit.eta_zero_selection_audit << "\n";
  out << "final_solution_samples_used_as_input=false\n";
  return out.str();
}

std::string SerializeGaugeLinkEndpointTransportForFingerprint(
    const LightlikeGaugeLinkEndpointTransportResult& result) {
  std::ostringstream out;
  out << "kind=b64ag-gauge-link-finite-boundary-endpoint-transport\n";
  out << "runtime_application=" << result.runtime_application << "\n";
  out << "transport_scope=" << result.transport_scope << "\n";
  out << "epsilon_samples=" << result.epsilon_sample_count << "\n";
  out << "requested_masters=" << result.requested_master_count << "\n";
  out << "transported_masters=" << result.transported_master_count << "\n";
  out << "contour_fingerprint=" << result.contour_fingerprint << "\n";
  out << "endpoint_local_model_kind=" << result.endpoint_local_model_kind << "\n";
  out << "final_solution_samples_used_as_input=false\n";
  for (const std::string& label : result.transported_master_labels) {
    out << "transported_master=" << label << "\n";
  }
  for (const LightlikeGaugeLinkSixMasterEndpointTerms& master_terms :
       result.endpoint_terms) {
    out << "endpoint_master=" << master_terms.master_label << "\n";
    for (const LightlikeGaugeLinkFinitePartTerm& term :
         master_terms.endpoint_terms) {
      out << "term=" << term.region_key << "," << term.power << ","
          << term.log_power << "," << term.coefficient << "\n";
    }
  }
  for (const std::string& gap : result.remaining_master_gaps) {
    out << "remaining_gap=" << gap << "\n";
  }
  return out.str();
}

struct GaugeLinkEndpointTermKey {
  std::string region_key;
  int power;
  int log_power;

  bool operator<(const GaugeLinkEndpointTermKey& other) const {
    if (region_key != other.region_key) {
      return region_key < other.region_key;
    }
    if (power != other.power) {
      return power < other.power;
    }
    return log_power < other.log_power;
  }
};

std::string CanonicalGaugeLinkRegionKey(const std::string& region_key) {
  return region_key.empty() ? "integer" : region_key;
}

std::string MultiplyGaugeLinkCoefficients(const std::string& reduction_coefficient,
                                          const std::string& endpoint_coefficient) {
  if (reduction_coefficient == "1") {
    return endpoint_coefficient;
  }
  if (endpoint_coefficient == "1") {
    return reduction_coefficient;
  }
  return "(" + reduction_coefficient + ")*(" + endpoint_coefficient + ")";
}

std::string AddGaugeLinkCoefficients(const std::string& lhs,
                                     const std::string& rhs) {
  if (lhs.empty()) {
    return rhs;
  }
  return "(" + lhs + ")+(" + rhs + ")";
}

void RecordReducedFinitePartFailure(
    LightlikeGaugeLinkReducedFinitePartResult& result,
    const std::string& target_label,
    const std::string& failure_code,
    const std::string& summary) {
  result.failures.push_back({target_label, failure_code, summary});
}

RuntimeComplex RuntimePolynomialValue(const RuntimePolynomial& polynomial,
                                      const RuntimeComplex& variable_value) {
  RuntimeComplex value = RuntimeComplex{0.0L, 0.0L};
  for (std::size_t reverse_index = polynomial.coefficients.size();
       reverse_index > 0;
       --reverse_index) {
    value = value * variable_value +
            polynomial.coefficients[reverse_index - 1];
  }
  return value;
}

RuntimeComplex RuntimeRationalValue(const RuntimeRationalPolynomial& rational,
                                    const RuntimeComplex& variable_value,
                                    const std::string& expression) {
  const RuntimeComplex denominator =
      RuntimePolynomialValue(rational.denominator, variable_value);
  if (std::abs(denominator) < kRuntimeTiny) {
    throw std::runtime_error("b64ag gauge-link endpoint transport divides by zero in \"" +
                             expression + "\"");
  }
  return RuntimePolynomialValue(rational.numerator, variable_value) / denominator;
}

int RuntimePolynomialLowestDegree(const RuntimePolynomial& polynomial) {
  for (std::size_t index = 0; index < polynomial.coefficients.size(); ++index) {
    if (!IsRuntimeTiny(polynomial.coefficients[index])) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

RuntimeComplex RuntimeRationalLaurentCoefficient(
    const RuntimeRationalPolynomial& rational,
    const int target_power) {
  const int numerator_lowest = RuntimePolynomialLowestDegree(rational.numerator);
  if (numerator_lowest < 0) {
    return RuntimeComplex{0.0L, 0.0L};
  }
  const int denominator_lowest = RuntimePolynomialLowestDegree(rational.denominator);
  if (denominator_lowest < 0) {
    throw std::runtime_error(
        "b64ag gauge-link Laurent coefficient extraction found an empty denominator");
  }
  const RuntimeComplex denominator_leading =
      rational.denominator.coefficients[static_cast<std::size_t>(denominator_lowest)];
  if (IsRuntimeTiny(denominator_leading)) {
    throw std::runtime_error(
        "b64ag gauge-link Laurent coefficient extraction found a zero leading denominator");
  }

  const int first_power = numerator_lowest - denominator_lowest;
  if (target_power < first_power) {
    return RuntimeComplex{0.0L, 0.0L};
  }

  std::map<int, RuntimeComplex> quotient;
  for (int power = first_power; power <= target_power; ++power) {
    RuntimeComplex numerator_coefficient{0.0L, 0.0L};
    const int numerator_index = power + denominator_lowest;
    if (numerator_index >= 0 &&
        static_cast<std::size_t>(numerator_index) <
            rational.numerator.coefficients.size()) {
      numerator_coefficient =
          rational.numerator.coefficients[static_cast<std::size_t>(numerator_index)];
    }

    RuntimeComplex known_product{0.0L, 0.0L};
    for (std::size_t denominator_index =
             static_cast<std::size_t>(denominator_lowest + 1);
         denominator_index < rational.denominator.coefficients.size();
         ++denominator_index) {
      const int prior_power =
          power - (static_cast<int>(denominator_index) - denominator_lowest);
      const auto prior_it = quotient.find(prior_power);
      if (prior_it != quotient.end()) {
        known_product += rational.denominator.coefficients[denominator_index] *
                         prior_it->second;
      }
    }
    quotient[power] =
        (numerator_coefficient - known_product) / denominator_leading;
  }
  return quotient[target_power];
}

RuntimeComplex GaugeLinkMatrixLaurentCoefficient(
    const std::vector<std::vector<std::string>>& diffeq_matrix,
    const std::size_t row,
    const std::size_t column,
    const int power,
    const std::string& variable,
    const long double epsilon_sample) {
  if (row >= diffeq_matrix.size() || column >= diffeq_matrix[row].size()) {
    throw std::runtime_error(
        "b64ag gauge-link endpoint transport inspected a matrix cell outside the "
        "reviewed six-master basis");
  }
  return RuntimeRationalLaurentCoefficient(
      ParseGaugeLinkRationalExpression(diffeq_matrix[row][column],
                                       variable,
                                       epsilon_sample),
      power);
}

void RequireGaugeLinkLaurentCoefficient(
    const std::vector<std::vector<std::string>>& diffeq_matrix,
    const std::size_t row,
    const std::size_t column,
    const int power,
    const RuntimeComplex& expected,
    const std::string& variable,
    const long double epsilon_sample,
    const std::string& description) {
  const RuntimeComplex actual = GaugeLinkMatrixLaurentCoefficient(
      diffeq_matrix, row, column, power, variable, epsilon_sample);
  const long double reference = std::max(1.0L, std::abs(expected));
  if (std::abs(actual - expected) > reference * 1e-12L) {
    throw std::runtime_error(
        "b64ag finite-boundary endpoint transport rejected the reviewed " +
        description + " Laurent coefficient");
  }
}

using RuntimeSeries = std::map<GaugeLinkEndpointTermKey, RuntimeComplex>;

RuntimeComplex RuntimeSeriesCoefficient(const RuntimeSeries& series,
                                        const int power,
                                        const int log_power) {
  const auto it = series.find({"integer", power, log_power});
  return it == series.end() ? RuntimeComplex{0.0L, 0.0L} : it->second;
}

void AddRuntimeSeriesTerm(RuntimeSeries& series,
                          const int power,
                          const int log_power,
                          const RuntimeComplex& coefficient) {
  if (IsRuntimeTiny(coefficient)) {
    return;
  }
  series[{"integer", power, log_power}] += coefficient;
  if (IsRuntimeTiny(series[{"integer", power, log_power}])) {
    series.erase({"integer", power, log_power});
  }
}

RuntimeSeries AddRuntimeSeries(RuntimeSeries lhs,
                               const RuntimeSeries& rhs,
                               const RuntimeComplex& rhs_scale =
                                   RuntimeComplex{1.0L, 0.0L}) {
  for (const auto& entry : rhs) {
    AddRuntimeSeriesTerm(lhs,
                         entry.first.power,
                         entry.first.log_power,
                         rhs_scale * entry.second);
  }
  return lhs;
}

RuntimeComplex RuntimeSeriesValue(const RuntimeSeries& series,
                                  const long double x) {
  const long double log_x = std::log(x);
  RuntimeComplex value{0.0L, 0.0L};
  for (const auto& entry : series) {
    value += entry.second *
             std::pow(x, static_cast<long double>(entry.first.power)) *
             std::pow(log_x, static_cast<long double>(entry.first.log_power));
  }
  return value;
}

int RuntimeRationalLowestPower(const RuntimeRationalPolynomial& rational) {
  const int numerator_lowest = RuntimePolynomialLowestDegree(rational.numerator);
  if (numerator_lowest < 0) {
    return 0;
  }
  const int denominator_lowest = RuntimePolynomialLowestDegree(rational.denominator);
  if (denominator_lowest < 0) {
    throw std::runtime_error(
        "b64ag gauge-link Laurent series found an empty denominator");
  }
  return numerator_lowest - denominator_lowest;
}

int RequireIntegerResidue(const RuntimeComplex& residue,
                          const std::string& description) {
  if (std::abs(residue.imag()) > 1e-12L) {
    throw std::runtime_error(
        "b64ag finite-boundary endpoint transport found a complex " +
        description + " residue");
  }
  const long double rounded = std::round(residue.real());
  if (std::abs(residue.real() - rounded) > 1e-12L) {
    throw std::runtime_error(
        "b64ag finite-boundary endpoint transport requires an integer " +
        description + " residue");
  }
  return static_cast<int>(rounded);
}

RuntimeSeries BuildGaugeLinkScalarEndpointSeries(
    const std::vector<std::vector<std::string>>& diffeq_matrix,
    const std::size_t row,
    const std::vector<RuntimeSeries>& known_series,
    const RuntimeComplex& boundary_value,
    const std::string& variable,
    const long double epsilon_value) {
  constexpr int kMinPower = -4;
  constexpr int kMaxPower = 80;
  constexpr long double kBoundaryPoint = 1.0L / 40.0L;

  const int residue = RequireIntegerResidue(
      GaugeLinkMatrixLaurentCoefficient(
          diffeq_matrix, row, row, -1, variable, epsilon_value),
      "diagonal");
  std::map<int, RuntimeComplex> diagonal_regular_coefficients;
  for (int power = 0; power <= kMaxPower - kMinPower + 1; ++power) {
    const RuntimeComplex coefficient = GaugeLinkMatrixLaurentCoefficient(
        diffeq_matrix, row, row, power, variable, epsilon_value);
    if (!IsRuntimeTiny(coefficient)) {
      diagonal_regular_coefficients[power] = coefficient;
    }
  }

  RuntimeSeries source;
  for (std::size_t column = 0; column < diffeq_matrix[row].size(); ++column) {
    if (column == row || column >= known_series.size() ||
        known_series[column].empty()) {
      continue;
    }
    const RuntimeRationalPolynomial cell = ParseGaugeLinkRationalExpression(
        diffeq_matrix[row][column], variable, epsilon_value);
    if (cell.numerator.coefficients.empty()) {
      continue;
    }
    int min_known_power = kMaxPower;
    for (const auto& term : known_series[column]) {
      min_known_power = std::min(min_known_power, term.first.power);
    }
    const int lowest_matrix_power = RuntimeRationalLowestPower(cell);
    const int highest_matrix_power = kMaxPower - min_known_power;
    for (int matrix_power = lowest_matrix_power;
         matrix_power <= highest_matrix_power;
         ++matrix_power) {
      const RuntimeComplex matrix_coefficient =
          RuntimeRationalLaurentCoefficient(cell, matrix_power);
      if (IsRuntimeTiny(matrix_coefficient)) {
        continue;
      }
      for (const auto& term : known_series[column]) {
        AddRuntimeSeriesTerm(source,
                             term.first.power + matrix_power,
                             term.first.log_power,
                             matrix_coefficient * term.second);
      }
    }
  }

  const auto solve_with_seed = [&](const RuntimeSeries& forcing,
                                   const RuntimeComplex& resonance_seed) {
    RuntimeSeries series;
    for (int power = kMinPower; power <= kMaxPower; ++power) {
      const long double denominator =
          static_cast<long double>(power - residue);
      RuntimeComplex log_rhs =
          RuntimeSeriesCoefficient(forcing, power - 1, 1);
      for (const auto& diagonal : diagonal_regular_coefficients) {
        log_rhs += diagonal.second *
                   RuntimeSeriesCoefficient(
                       series, power - 1 - diagonal.first, 1);
      }

      RuntimeComplex log_coefficient{0.0L, 0.0L};
      if (std::abs(denominator) < kRuntimeTiny) {
        if (std::abs(log_rhs) > 1e-12L) {
          throw std::runtime_error(
              "b64ag finite-boundary endpoint transport encountered an "
              "unresolved log-squared resonance");
        }
      } else {
        log_coefficient = log_rhs / denominator;
      }

      RuntimeComplex regular_rhs =
          RuntimeSeriesCoefficient(forcing, power - 1, 0);
      for (const auto& diagonal : diagonal_regular_coefficients) {
        regular_rhs += diagonal.second *
                       RuntimeSeriesCoefficient(
                           series, power - 1 - diagonal.first, 0);
      }

      RuntimeComplex regular_coefficient{0.0L, 0.0L};
      if (std::abs(denominator) < kRuntimeTiny) {
        regular_coefficient = resonance_seed;
        log_coefficient = regular_rhs;
      } else {
        regular_coefficient =
            (regular_rhs - log_coefficient) / denominator;
      }
      AddRuntimeSeriesTerm(series, power, 1, log_coefficient);
      AddRuntimeSeriesTerm(series, power, 0, regular_coefficient);
    }
    return series;
  };

  const RuntimeSeries particular =
      solve_with_seed(source, RuntimeComplex{0.0L, 0.0L});
  const RuntimeSeries homogeneous =
      solve_with_seed(RuntimeSeries{}, RuntimeComplex{1.0L, 0.0L});
  const RuntimeComplex homogeneous_boundary =
      RuntimeSeriesValue(homogeneous, kBoundaryPoint);
  if (IsRuntimeTiny(homogeneous_boundary)) {
    throw std::runtime_error(
        "b64ag finite-boundary endpoint transport produced a zero scalar "
        "connection basis");
  }
  const RuntimeComplex connection =
      (boundary_value - RuntimeSeriesValue(particular, kBoundaryPoint)) /
      homogeneous_boundary;
  return AddRuntimeSeries(particular, homogeneous, connection);
}

std::vector<LightlikeGaugeLinkFinitePartTerm> EndpointTermsFromRuntimeSeries(
    const RuntimeSeries& series,
    const int max_power) {
  std::vector<LightlikeGaugeLinkFinitePartTerm> terms;
  for (const auto& entry : series) {
    if (entry.first.power > max_power ||
        (entry.first.log_power != 0 && entry.first.power > 0) ||
        IsRuntimeTiny(entry.second)) {
      continue;
    }
    terms.push_back({"integer",
                     entry.first.power,
                     entry.first.log_power,
                     FormatRuntimeComplex(entry.second, 24)});
  }
  return terms;
}

RuntimeComplex ParseRuntimeConstantComplexExpression(
    const std::string& expression,
    const std::string& variable,
    const long double epsilon_sample) {
  const RuntimeRationalPolynomial rational =
      ParseGaugeLinkRationalExpression(expression, variable, epsilon_sample);
  if (RuntimePolynomialDegree(rational.numerator) > 0 ||
      RuntimePolynomialDegree(rational.denominator) > 0) {
    throw std::runtime_error(
        "b64ag gauge-link endpoint transport boundary value is not finite-constant: " +
        expression);
  }
  return RuntimeRationalValue(rational, RuntimeComplex{0.0L, 0.0L}, expression);
}

struct GaugeLinkFirstBlockEndpointBasisValue {
  long double y = 0.0L;
  long double w = 0.0L;
};

GaugeLinkFirstBlockEndpointBasisValue EvaluateGaugeLinkFirstBlockEndpointBasis(
    const long double epsilon_value,
    const long double rho,
    const GaugeLinkFirstBlockEndpointBasisValue& leading,
    const long double x) {
  constexpr int kSeriesOrder = 180;
  const long double lambda = 6.0L * (epsilon_value - 1.0L);
  std::vector<GaugeLinkFirstBlockEndpointBasisValue> coefficients(
      static_cast<std::size_t>(kSeriesOrder) + 1);
  coefficients[0] = leading;

  long double minus_two_power = 1.0L;
  std::vector<long double> regular_scales(static_cast<std::size_t>(kSeriesOrder));
  for (int index = 0; index < kSeriesOrder; ++index) {
    regular_scales[static_cast<std::size_t>(index)] =
        (epsilon_value - 1.0L) * minus_two_power;
    minus_two_power *= -2.0L;
  }

  for (int order = 1; order <= kSeriesOrder; ++order) {
    long double rhs_y = 0.0L;
    long double rhs_w = 0.0L;
    for (int matrix_order = 0; matrix_order < order; ++matrix_order) {
      const GaugeLinkFirstBlockEndpointBasisValue& previous =
          coefficients[static_cast<std::size_t>(order - 1 - matrix_order)];
      const long double scale =
          regular_scales[static_cast<std::size_t>(matrix_order)];
      rhs_y += scale * (2.0L * previous.y + 24.0L * previous.w);
      rhs_w += scale * (-previous.y / 3.0L - 4.0L * previous.w);
    }
    const long double y_denominator = rho + static_cast<long double>(order);
    const long double w_denominator =
        rho + static_cast<long double>(order) - lambda;
    if (std::abs(y_denominator) < kRuntimeTiny ||
        std::abs(w_denominator) < kRuntimeTiny) {
      throw std::runtime_error(
          "b64ag first-block endpoint transport hit a resonant Frobenius denominator");
    }
    coefficients[static_cast<std::size_t>(order)].y = rhs_y / y_denominator;
    coefficients[static_cast<std::size_t>(order)].w = rhs_w / w_denominator;
  }

  GaugeLinkFirstBlockEndpointBasisValue value;
  long double x_power = 1.0L;
  for (const GaugeLinkFirstBlockEndpointBasisValue& coefficient : coefficients) {
    value.y += coefficient.y * x_power;
    value.w += coefficient.w * x_power;
    x_power *= x;
  }
  const long double frobenius_power = std::exp(rho * std::log(x));
  value.y *= frobenius_power;
  value.w *= frobenius_power;
  return value;
}

struct GaugeLinkFirstBlockEndpointCoefficients {
  RuntimeComplex first_master_regular_coefficient;
  RuntimeComplex companion_regular_coefficient;
  RuntimeComplex unresolved_frobenius_coefficient;
};

GaugeLinkFirstBlockEndpointCoefficients
EvaluateGaugeLinkFirstBlockEndpointCoefficients(
    const long double epsilon_value,
    const RuntimeComplex& first_master_boundary,
    const RuntimeComplex& companion_boundary) {
  const long double x = 1.0L / 40.0L;
  const long double lambda = 6.0L * (epsilon_value - 1.0L);
  const GaugeLinkFirstBlockEndpointBasisValue regular_basis =
      EvaluateGaugeLinkFirstBlockEndpointBasis(epsilon_value,
                                               0.0L,
                                               {1.0L, 0.0L},
                                               x);
  const GaugeLinkFirstBlockEndpointBasisValue singular_basis =
      EvaluateGaugeLinkFirstBlockEndpointBasis(epsilon_value,
                                               lambda,
                                               {0.0L, 1.0L},
                                               x);
  const RuntimeComplex boundary_y = first_master_boundary / x;
  const RuntimeComplex boundary_w =
      companion_boundary - boundary_y * (5.0L / 6.0L);
  const long double determinant =
      regular_basis.y * singular_basis.w -
      singular_basis.y * regular_basis.w;
  if (std::abs(determinant) < kRuntimeTiny) {
    throw std::runtime_error(
        "b64ag first-block endpoint transport produced a singular connection matrix");
  }

  GaugeLinkFirstBlockEndpointCoefficients coefficients;
  coefficients.first_master_regular_coefficient =
      (boundary_y * singular_basis.w - boundary_w * singular_basis.y) /
      determinant;
  coefficients.unresolved_frobenius_coefficient =
      (regular_basis.y * boundary_w - regular_basis.w * boundary_y) /
      determinant;
  coefficients.companion_regular_coefficient =
      coefficients.first_master_regular_coefficient * (5.0L / 6.0L);
  return coefficients;
}

bool IsSmallRelativeToTransportScale(const RuntimeComplex& value,
                                     const RuntimeComplex& scale) {
  const long double reference = std::max(1.0L, std::abs(scale));
  return std::abs(value) < reference * 1e-12L;
}

}  // namespace

bool IsLightlikeGaugeLinkEtaZeroRuntimeState(
    const LightlikeGaugeLinkRuntimeState& state) {
  if (!state.amflow_state_input || state.benchmark_id != "linear_propagator" ||
      state.family != "gauge" || state.integral_kind != "loop" ||
      state.variable != "gaugex" ||
      RemoveAsciiSpaces(state.start_location) != "gaugex->1/40" ||
      RemoveAsciiSpaces(state.target_location) != "gaugex=0" ||
      state.boundary_state_kind != "amflow_finite_solution_samples" ||
      !HasCanonicalSingularPoint(state, "gaugex=0")) {
    return false;
  }
  for (const std::string& required_file : {"boundary", "diffeq", "reduction", "solve.wl"}) {
    if (!HasBoundaryFile(state, required_file)) {
      return false;
    }
  }
  if (!state.boundary_point.empty() &&
      RemoveAsciiSpaces(state.boundary_point) != "gaugex->1/40") {
    return false;
  }
  if (!state.diffeq_variables.empty() &&
      !VectorEquals(state.diffeq_variables, {"gaugex"})) {
    return false;
  }
  return true;
}

ProblemSpec MakeReviewedLightlikeGaugeLinkProblemSpec() {
  ProblemSpec spec;
  spec.family.name = "gauge";
  spec.family.loop_momenta = {"l1", "l2", "l3"};
  for (const std::string& expression : ReviewedPropagators()) {
    spec.family.propagators.push_back(Propagator(expression));
  }
  spec.kinematics.incoming_momenta = {"n"};
  spec.kinematics.scalar_product_rules = {{"n^2", "-1"}};
  spec.kinematics.invariants = {"n2"};
  for (const std::string& label : ReviewedTargetLabels()) {
    const std::size_t open = label.find('[');
    const std::size_t close = label.find(']');
    TargetIntegral target;
    target.family = "gauge";
    std::string number;
    for (std::size_t index = open + 1; index < close; ++index) {
      const char current = label[index];
      if (current == ',') {
        target.indices.push_back(std::stoi(number));
        number.clear();
      } else {
        number.push_back(current);
      }
    }
    if (!number.empty()) {
      target.indices.push_back(std::stoi(number));
    }
    spec.targets.push_back(std::move(target));
  }
  return spec;
}

LightlikeGaugeLinkSquareFamilyResult BuildLightlikeGaugeLinkSquareFamily(
    const ProblemSpec& spec,
    const std::string& variable) {
  if (RemoveAsciiSpaces(variable) != "gaugex") {
    throw std::runtime_error(
        "b64ag gauge-link scaffold is reviewed only for generated variable gaugex");
  }
  RequireReviewedGaugeLinkSourceSurface(spec);

  LightlikeGaugeLinkSquareFamilyResult result;
  result.transformed_spec = spec;
  result.variable = "gaugex";
  result.affected_propagator_indices = AffectedPropagatorIndices();
  result.generated_square_propagators = GeneratedSquarePropagators();
  for (std::size_t index = 0; index < result.generated_square_propagators.size(); ++index) {
    result.transformed_spec.family.propagators[index].expression =
        result.generated_square_propagators[index];
    result.transformed_spec.family.propagators[index].kind = PropagatorKind::Standard;
    result.transformed_spec.family.propagators[index].variant = PropagatorVariant::Quadratic;
  }
  return result;
}

std::vector<LightlikeGaugeLinkTargetNormalization>
ApplyLightlikeGaugeLinkPowerNormalization(
    const std::vector<TargetIntegral>& targets,
    const std::vector<std::size_t>& affected_propagator_indices,
    const std::string& variable) {
  std::vector<LightlikeGaugeLinkTargetNormalization> normalizations;
  normalizations.reserve(targets.size());
  for (const TargetIntegral& target : targets) {
    int affected_power_sum = 0;
    for (const std::size_t propagator_index : affected_propagator_indices) {
      if (propagator_index >= target.indices.size()) {
        throw std::runtime_error(
            "b64ag gauge-link power normalization found a target with too few indices");
      }
      affected_power_sum += target.indices[propagator_index];
    }
    normalizations.push_back({TargetLabel(target),
                              affected_power_sum,
                              NormalizationFactor(variable, affected_power_sum)});
  }
  return normalizations;
}

LightlikeGaugeLinkFinitePartResult ExtractLightlikeGaugeLinkEndpointFinitePart(
    const std::vector<LightlikeGaugeLinkFinitePartTerm>& terms) {
  LightlikeGaugeLinkFinitePartResult result;
  if (terms.empty()) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction has no endpoint terms; this partial scaffold does not "
        "publish an implicit zero coefficient";
    return result;
  }
  std::set<std::string> region_keys;
  for (const LightlikeGaugeLinkFinitePartTerm& term : terms) {
    region_keys.insert(term.region_key.empty() ? "integer" : term.region_key);
    if (term.log_power != 0 && term.power <= 0) {
      result.failure_code = "continuation_budget_exhausted";
      result.summary =
          "b64ag finite-part extraction rejects unresolved non-vanishing "
          "logarithmic endpoint structure at power " +
          std::to_string(term.power) + " with coefficient " + term.coefficient;
      return result;
    }
  }
  if (region_keys.size() > 1) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction rejects multiple integer endpoint regions";
    return result;
  }
  const auto min_power_it =
      std::min_element(terms.begin(),
                       terms.end(),
                       [](const LightlikeGaugeLinkFinitePartTerm& lhs,
                          const LightlikeGaugeLinkFinitePartTerm& rhs) {
                         return lhs.power < rhs.power;
                       });
  if (min_power_it != terms.end() && min_power_it->power > 0) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction found a positive starting power; this partial scaffold "
        "does not publish an implicit zero coefficient";
    return result;
  }

  const auto zero_it =
      std::find_if(terms.begin(),
                   terms.end(),
                   [](const LightlikeGaugeLinkFinitePartTerm& term) {
                     return term.power == 0;
                   });
  for (const LightlikeGaugeLinkFinitePartTerm& term : terms) {
    if (term.power < 0) {
      result.dropped_singular_terms.push_back(term.coefficient);
    }
  }
  if (zero_it == terms.end()) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction did not find a power-zero coefficient; this partial "
        "scaffold does not publish an implicit zero coefficient";
    return result;
  }
  result.success = true;
  result.ir_subtraction_applied = true;
  result.finite_part_coefficient = zero_it->coefficient;
  result.summary =
      result.dropped_singular_terms.empty()
          ? "b64ag finite-part extraction selected the endpoint power-zero coefficient"
          : "b64ag finite-part extraction dropped singular endpoint powers and selected the "
            "power-zero coefficient";
  return result;
}

LightlikeGaugeLinkReducedFinitePartResult
EvaluateLightlikeGaugeLinkReducedFiniteParts(
    const std::vector<TargetIntegral>& targets,
    const std::vector<LightlikeGaugeLinkSixMasterEndpointTerms>& endpoint_terms,
    const std::vector<LightlikeGaugeLinkTargetReductionTerm>& target_reduction_terms,
    const std::string& variable) {
  LightlikeGaugeLinkReducedFinitePartResult result;
  result.runtime_application =
      "b64ag-gauge-link-reduced-finite-part-functional";

  const auto fail_global = [&result](const std::string& failure_code,
                                     const std::string& summary) {
    RecordReducedFinitePartFailure(result, "<target-set>", failure_code, summary);
    result.summary =
        summary +
        "; retained_solution_samples_used=false; full_eta_zero_contour_applied=false.";
    return result;
  };

  if (RemoveAsciiSpaces(variable) != "gaugex") {
    return fail_global(
        "master_set_instability",
        "b64ag reduced finite-part functional is reviewed only for variable gaugex");
  }
  if (targets.empty()) {
    return fail_global(
        "boundary_unsolved",
        "b64ag reduced finite-part functional requires at least one retained target");
  }
  if (!LabelsMatchReviewedGaugeLinkTargets(targets)) {
    return fail_global(
        "master_set_instability",
        "b64ag reduced finite-part functional requires the reviewed target packet or selected "
        "endpoint prefix");
  }

  std::vector<LightlikeGaugeLinkTargetNormalization> normalizations;
  try {
    normalizations = ApplyLightlikeGaugeLinkPowerNormalization(
        targets, AffectedPropagatorIndices(), variable);
  } catch (const std::runtime_error& error) {
    return fail_global("master_set_instability", error.what());
  }

  std::map<std::string, std::vector<LightlikeGaugeLinkFinitePartTerm>>
      endpoint_terms_by_master;
  for (const LightlikeGaugeLinkSixMasterEndpointTerms& master_terms :
       endpoint_terms) {
    if (master_terms.master_label.empty()) {
      return fail_global(
          "master_set_instability",
          "b64ag reduced finite-part functional received an unlabeled six-master endpoint row");
    }
    const auto inserted =
        endpoint_terms_by_master.emplace(master_terms.master_label,
                                         master_terms.endpoint_terms);
    if (!inserted.second) {
      return fail_global(
          "master_set_instability",
          "b64ag reduced finite-part functional received duplicate endpoint terms for " +
              master_terms.master_label);
    }
  }

  const std::set<std::string> reviewed_master_labels(
      ReviewedReductionMasterLabels().begin(), ReviewedReductionMasterLabels().end());
  for (const auto& entry : endpoint_terms_by_master) {
    if (reviewed_master_labels.find(entry.first) == reviewed_master_labels.end()) {
      return fail_global(
          "master_set_instability",
          "b64ag reduced finite-part functional received non-reviewed six-master label " +
              entry.first);
    }
  }
  for (const std::string& master_label : ReviewedReductionMasterLabels()) {
    const auto terms_it = endpoint_terms_by_master.find(master_label);
    if (terms_it == endpoint_terms_by_master.end() || terms_it->second.empty()) {
      return fail_global(
          "boundary_unsolved",
          "b64ag reduced finite-part functional is missing supplied endpoint terms for " +
              master_label);
    }
  }

  std::set<std::string> requested_target_labels;
  for (const LightlikeGaugeLinkTargetNormalization& normalization :
       normalizations) {
    requested_target_labels.insert(normalization.target_label);
  }
  std::map<std::string, std::vector<LightlikeGaugeLinkTargetReductionTerm>>
      reduction_terms_by_target;
  for (const LightlikeGaugeLinkTargetReductionTerm& term :
       target_reduction_terms) {
    if (term.target_label.empty() || term.master_label.empty() ||
        term.coefficient.empty()) {
      return fail_global(
          "boundary_unsolved",
          "b64ag reduced finite-part functional received an incomplete target-reduction term");
    }
    if (requested_target_labels.find(term.target_label) ==
        requested_target_labels.end()) {
      return fail_global(
          "master_set_instability",
          "b64ag reduced finite-part functional received a target-reduction row outside the "
          "requested retained target set: " +
              term.target_label);
    }
    reduction_terms_by_target[term.target_label].push_back(term);
  }

  for (const LightlikeGaugeLinkTargetNormalization& normalization :
       normalizations) {
    LightlikeGaugeLinkReducedFinitePartTarget target_result;
    target_result.target_label = normalization.target_label;
    target_result.affected_power_sum = normalization.affected_power_sum;
    target_result.normalization_factor = normalization.normalization_factor;

    const auto reduction_it =
        reduction_terms_by_target.find(normalization.target_label);
    if (reduction_it == reduction_terms_by_target.end() ||
        reduction_it->second.empty()) {
      target_result.failure_code = "boundary_unsolved";
      target_result.summary =
          "b64ag reduced finite-part functional is missing the retained target-reduction row "
          "for " +
          normalization.target_label;
      RecordReducedFinitePartFailure(result,
                                     target_result.target_label,
                                     target_result.failure_code,
                                     target_result.summary);
      result.targets.push_back(std::move(target_result));
      continue;
    }

    std::map<GaugeLinkEndpointTermKey, LightlikeGaugeLinkFinitePartTerm>
        reduced_terms_by_key;
    bool row_failed = false;
    for (const LightlikeGaugeLinkTargetReductionTerm& reduction_term :
         reduction_it->second) {
      const auto endpoint_it =
          endpoint_terms_by_master.find(reduction_term.master_label);
      if (endpoint_it == endpoint_terms_by_master.end()) {
        target_result.failure_code = "boundary_unsolved";
        target_result.summary =
            "b64ag reduced finite-part functional is missing endpoint terms for reduction "
            "master " +
            reduction_term.master_label + " while evaluating " +
            normalization.target_label;
        row_failed = true;
        break;
      }
      for (const LightlikeGaugeLinkFinitePartTerm& endpoint_term :
           endpoint_it->second) {
        if (endpoint_term.coefficient.empty()) {
          target_result.failure_code = "boundary_unsolved";
          target_result.summary =
              "b64ag reduced finite-part functional received an empty endpoint coefficient for " +
              reduction_term.master_label;
          row_failed = true;
          break;
        }
        const int reduced_power = endpoint_term.power +
                                  reduction_term.gaugex_power_shift -
                                  normalization.affected_power_sum;
        const int reduced_log_power =
            endpoint_term.log_power + reduction_term.log_power;
        const GaugeLinkEndpointTermKey key{
            CanonicalGaugeLinkRegionKey(endpoint_term.region_key),
            reduced_power,
            reduced_log_power};
        LightlikeGaugeLinkFinitePartTerm& reduced_term =
            reduced_terms_by_key[key];
        reduced_term.region_key = key.region_key;
        reduced_term.power = key.power;
        reduced_term.log_power = key.log_power;
        reduced_term.coefficient =
            AddGaugeLinkCoefficients(
                reduced_term.coefficient,
                MultiplyGaugeLinkCoefficients(reduction_term.coefficient,
                                               endpoint_term.coefficient));
      }
      if (row_failed) {
        break;
      }
    }
    if (row_failed) {
      RecordReducedFinitePartFailure(result,
                                     target_result.target_label,
                                     target_result.failure_code,
                                     target_result.summary);
      result.targets.push_back(std::move(target_result));
      continue;
    }

    for (const auto& entry : reduced_terms_by_key) {
      target_result.reduced_endpoint_terms.push_back(entry.second);
    }
    const LightlikeGaugeLinkFinitePartResult finite_part =
        ExtractLightlikeGaugeLinkEndpointFinitePart(
            target_result.reduced_endpoint_terms);
    if (!finite_part.success) {
      target_result.failure_code = finite_part.failure_code;
      target_result.summary =
          "b64ag reduced finite-part functional failed closed for " +
          normalization.target_label + ": " + finite_part.summary;
      RecordReducedFinitePartFailure(result,
                                     target_result.target_label,
                                     target_result.failure_code,
                                     target_result.summary);
      result.targets.push_back(std::move(target_result));
      continue;
    }

    target_result.success = true;
    target_result.ir_subtraction_applied = finite_part.ir_subtraction_applied;
    target_result.finite_part_coefficient =
        finite_part.finite_part_coefficient;
    target_result.dropped_singular_terms =
        finite_part.dropped_singular_terms;
    target_result.summary =
        "Applied retained target reduction and D4,D5 normalization before "
        "PickZeroRuleS-compatible finite-part extraction for " +
        normalization.target_label + "; normalization_factor=" +
        normalization.normalization_factor + ".";
    result.ir_subtraction_applied =
        result.ir_subtraction_applied || target_result.ir_subtraction_applied;
    result.targets.push_back(std::move(target_result));
  }

  result.success = !result.targets.empty() && result.failures.empty();
  if (result.success) {
    result.summary =
        "b64ag reduced gauge-link finite-part functional evaluated " +
        std::to_string(result.targets.size()) +
        " retained target(s) from supplied six-master endpoint terms; retained_solution_samples_"
        "used=false; full_eta_zero_contour_applied=false; full endpoint transport from "
        "gaugex=1/40 remains deferred.";
  } else {
    result.summary =
        "b64ag reduced gauge-link finite-part functional failed closed for " +
        std::to_string(result.failures.size()) +
        " target diagnostic(s); retained_solution_samples_used=false; "
        "full_eta_zero_contour_applied=false.";
  }
  return result;
}

LightlikeGaugeLinkEndpointTransportResult
TransportLightlikeGaugeLinkFiniteBoundaryEndpointTerms(
    const LightlikeGaugeLinkRuntimeState& state,
    const std::vector<LightlikeGaugeLinkFiniteBoundarySample>& boundary_samples) {
  LightlikeGaugeLinkRuntimeState checked_state = RequireRuntimeState(state);
  LightlikeGaugeLinkTransportAudit scaffold =
      BuildLightlikeGaugeLinkRetainedStateScaffold(checked_state);

  LightlikeGaugeLinkEndpointTransportResult result;
  result.retained_solution_samples_available =
      scaffold.retained_solution_samples_available;
  result.epsilon_sample_count = boundary_samples.size();
  result.requested_master_count = checked_state.diffeq_masters.size();
  result.runtime_application =
      "b64ag-gauge-link-finite-boundary-endpoint-transport";
  result.transport_scope =
      "eta-zero-six-master-endpoint-terms-finite-gaugex";
  result.contour_fingerprint = scaffold.contour_fingerprint;
  result.endpoint_local_model_kind = scaffold.endpoint_local_model_kind;

  const auto fail = [&result](const std::string& failure_code,
                              const std::string& summary) {
    result.failure_code = failure_code;
    result.summary =
        summary +
        "; retained_solution_samples_used=false; full_eta_zero_contour_applied=false.";
    return result;
  };

  if (checked_state.diffeq_masters.size() != ReviewedReductionMasterLabels().size() ||
      !LabelsExactlyMatch(checked_state.diffeq_masters,
                          ReviewedReductionMasterLabels())) {
    return fail("master_set_instability",
                "b64ag finite-boundary endpoint transport requires the reviewed "
                "six-master DE basis");
  }
  if (boundary_samples.empty()) {
    return fail("boundary_unsolved",
                "b64ag finite-boundary endpoint transport requires finite "
                "gaugex=1/40 boundary samples");
  }
  if (boundary_samples.size() != 1) {
    return fail("insufficient_precision",
                "b64ag finite-boundary endpoint transport currently publishes "
                "six-master endpoint terms for one supplied epsilon sample; "
                "multi-sample Laurent fitting remains a downstream gap");
  }

  const LightlikeGaugeLinkFiniteBoundarySample& sample = boundary_samples.front();
  const std::string epsilon_sample =
      sample.epsilon_sample.empty() && !checked_state.epsilon_samples.empty()
          ? checked_state.epsilon_samples.front()
          : sample.epsilon_sample;
  if (epsilon_sample.empty()) {
    return fail("boundary_unsolved",
                "b64ag finite-boundary endpoint transport requires an epsilon sample label");
  }
  if (sample.master_values.size() != checked_state.diffeq_masters.size()) {
    return fail("master_set_instability",
                "b64ag finite-boundary endpoint transport boundary vector width does "
                "not match the reviewed six-master DE basis");
  }

  try {
    const long double epsilon_value = ParseRuntimeRationalNumber(epsilon_sample);
    const auto diffeq_raw_it = checked_state.boundary_file_raws.find("diffeq");
    if (diffeq_raw_it == checked_state.boundary_file_raws.end()) {
      return fail("boundary_unsolved",
                  "b64ag finite-boundary endpoint transport requires the retained "
                  "diffeq matrix raw input");
    }
    const std::vector<std::vector<std::string>> diffeq_matrix =
        ParseLightlikeGaugeLinkDiffeqMatrixRaw(diffeq_raw_it->second);
    if (diffeq_matrix.empty() ||
        diffeq_matrix.size() != checked_state.diffeq_masters.size() ||
        diffeq_matrix.front().size() != checked_state.diffeq_masters.size()) {
      return fail("master_set_instability",
                  "b64ag finite-boundary endpoint transport parsed a DE matrix "
                  "whose shape does not match the reviewed six-master basis");
    }
    RequireGaugeLinkLaurentCoefficient(diffeq_matrix,
                                       2,
                                       2,
                                       -1,
                                       RuntimeComplex{1.0L, 0.0L},
                                       checked_state.variable,
                                       epsilon_value,
                                       "second-block self");
    RequireGaugeLinkLaurentCoefficient(diffeq_matrix,
                                       3,
                                       2,
                                       -3,
                                       RuntimeComplex{0.5L, 0.0L},
                                       checked_state.variable,
                                       epsilon_value,
                                       "second-block companion source");
    RequireGaugeLinkLaurentCoefficient(diffeq_matrix,
                                       4,
                                       4,
                                       -1,
                                       RuntimeComplex{2.0L, 0.0L},
                                       checked_state.variable,
                                       epsilon_value,
                                       "downstream first-row self");
    RequireGaugeLinkLaurentCoefficient(diffeq_matrix,
                                       5,
                                       2,
                                       -1,
                                       RuntimeComplex{-1.0L, 0.0L},
                                       checked_state.variable,
                                       epsilon_value,
                                       "downstream second-block source");
    RequireGaugeLinkLaurentCoefficient(diffeq_matrix,
                                       5,
                                       5,
                                       -1,
                                       RuntimeComplex{2.0L, 0.0L},
                                       checked_state.variable,
                                       epsilon_value,
                                       "downstream companion self");

    std::vector<RuntimeComplex> boundary_values;
    boundary_values.reserve(sample.master_values.size());
    for (const std::string& master_value : sample.master_values) {
      boundary_values.push_back(ParseRuntimeConstantComplexExpression(
          master_value, checked_state.variable, epsilon_value));
    }
    const RuntimeComplex first_boundary = boundary_values[0];
    const RuntimeComplex companion_boundary = boundary_values[1];
    const GaugeLinkFirstBlockEndpointCoefficients coefficients =
        EvaluateGaugeLinkFirstBlockEndpointCoefficients(epsilon_value,
                                                        first_boundary,
                                                        companion_boundary);
    if (!IsSmallRelativeToTransportScale(
            coefficients.unresolved_frobenius_coefficient,
            coefficients.first_master_regular_coefficient)) {
      return fail(
          "continuation_budget_exhausted",
          "b64ag first-block endpoint transport found an unresolved non-integer "
          "Frobenius branch; publishing PickZeroRuleS terms would overclaim the "
          "finite-gaugex transport");
    }

    const std::string first_label = ReviewedReductionMasterLabels()[0];
    const std::string companion_label = ReviewedReductionMasterLabels()[1];
    const std::string second_label = ReviewedReductionMasterLabels()[2];
    const std::string second_companion_label = ReviewedReductionMasterLabels()[3];
    const std::string downstream_label = ReviewedReductionMasterLabels()[4];
    const std::string downstream_companion_label =
        ReviewedReductionMasterLabels()[5];
    std::vector<RuntimeSeries> endpoint_series(
        ReviewedReductionMasterLabels().size());
    AddRuntimeSeriesTerm(endpoint_series[0],
                         1,
                         0,
                         coefficients.first_master_regular_coefficient);
    AddRuntimeSeriesTerm(endpoint_series[1],
                         0,
                         0,
                         coefficients.companion_regular_coefficient);
    endpoint_series[2] = BuildGaugeLinkScalarEndpointSeries(
        diffeq_matrix,
        2,
        endpoint_series,
        boundary_values[2],
        checked_state.variable,
        epsilon_value);
    endpoint_series[3] = BuildGaugeLinkScalarEndpointSeries(
        diffeq_matrix,
        3,
        endpoint_series,
        boundary_values[3],
        checked_state.variable,
        epsilon_value);
    endpoint_series[4] = BuildGaugeLinkScalarEndpointSeries(
        diffeq_matrix,
        4,
        endpoint_series,
        boundary_values[4],
        checked_state.variable,
        epsilon_value);
    endpoint_series[5] = BuildGaugeLinkScalarEndpointSeries(
        diffeq_matrix,
        5,
        endpoint_series,
        boundary_values[5],
        checked_state.variable,
        epsilon_value);
    const auto endpoint_term = [](const int power,
                                  const RuntimeComplex& coefficient) {
      return LightlikeGaugeLinkFinitePartTerm{
          "integer", power, 0, FormatRuntimeComplex(coefficient, 24)};
    };
    result.endpoint_terms.push_back(
        {first_label,
         {endpoint_term(
             1, coefficients.first_master_regular_coefficient)}});
    result.endpoint_terms.push_back(
        {companion_label,
         {endpoint_term(
             0, coefficients.companion_regular_coefficient)}});
    result.endpoint_terms.push_back(
        {second_label,
         EndpointTermsFromRuntimeSeries(endpoint_series[2], 2)});
    result.endpoint_terms.push_back(
        {second_companion_label,
         EndpointTermsFromRuntimeSeries(endpoint_series[3], 2)});
    result.endpoint_terms.push_back(
        {downstream_label,
         EndpointTermsFromRuntimeSeries(endpoint_series[4], 2)});
    result.endpoint_terms.push_back(
        {downstream_companion_label,
         EndpointTermsFromRuntimeSeries(endpoint_series[5], 2)});
    for (const LightlikeGaugeLinkSixMasterEndpointTerms& master_terms :
         result.endpoint_terms) {
      if (master_terms.endpoint_terms.empty()) {
        return fail("boundary_unsolved",
                    "b64ag finite-boundary endpoint transport produced no "
                    "publishable endpoint terms for " +
                        master_terms.master_label);
      }
    }
    result.transported_master_labels = ReviewedReductionMasterLabels();
    result.transported_master_count = result.endpoint_terms.size();
    result.success = true;
    result.extraction_fingerprint = ComputeArtifactFingerprint(
        SerializeGaugeLinkEndpointTransportForFingerprint(result));
    result.summary =
        "Applied b64ag finite-gaugex endpoint transport from the retained "
        "gaugex=1/40 boundary vector to live endpoint terms for all six "
        "reviewed gauge-link DE masters using the parsed first block, "
        "second-block, and downstream Laurent/Frobenius recurrence without "
        "reading retained final solution samples; "
        "retained_solution_samples_used=false; full_eta_zero_contour_applied=false.";
    return result;
  } catch (const std::exception& error) {
    return fail("boundary_unsolved", error.what());
  }
}

std::vector<std::vector<std::string>> ParseLightlikeGaugeLinkDiffeqMatrixRaw(
    const std::string& diffeq_raw) {
  const std::vector<std::string> root = SplitMathematicaListElements(diffeq_raw);
  if (root.size() != 3) {
    throw std::runtime_error(
        "b64ag gauge-link diffeq raw file must contain masters, variables, and matrices");
  }
  const std::vector<std::string> variables = SplitMathematicaListElements(root[1]);
  if (variables.size() != 1 || RemoveAsciiSpaces(variables.front()) != "gaugex") {
    throw std::runtime_error(
        "b64ag gauge-link diffeq raw file must carry the single variable gaugex");
  }
  const std::vector<std::string> variable_matrices = SplitMathematicaListElements(root[2]);
  if (variable_matrices.size() != 1) {
    throw std::runtime_error(
        "b64ag gauge-link diffeq raw file must carry exactly one gaugex matrix");
  }
  const std::vector<std::string> row_lists =
      SplitMathematicaListElements(variable_matrices.front());
  std::vector<std::vector<std::string>> matrix;
  matrix.reserve(row_lists.size());
  std::optional<std::size_t> column_count;
  for (const std::string& row_list : row_lists) {
    std::vector<std::string> row = SplitMathematicaListElements(row_list);
    if (!column_count.has_value()) {
      column_count = row.size();
    } else if (*column_count != row.size()) {
      throw std::runtime_error(
          "b64ag gauge-link diffeq raw matrix must be rectangular");
    }
    matrix.push_back(std::move(row));
  }
  if (matrix.empty() || !column_count.has_value() || *column_count == 0) {
    throw std::runtime_error("b64ag gauge-link diffeq raw matrix must not be empty");
  }
  return matrix;
}

LightlikeGaugeLinkContourPlanAudit BuildLightlikeGaugeLinkContourPlanAudit(
    const std::vector<std::vector<std::string>>& diffeq_matrix,
    const std::vector<std::string>& epsilon_samples,
    const std::string& variable,
    const std::string& boundary_point,
    const std::string& target_point) {
  if (RemoveAsciiSpaces(variable) != "gaugex") {
    throw std::runtime_error(
        "b64ag gauge-link contour planning is reviewed only for variable gaugex");
  }
  if (RemoveAsciiSpaces(target_point) != "gaugex=0") {
    throw std::runtime_error(
        "b64ag gauge-link contour planning is reviewed only for target gaugex=0");
  }
  if (diffeq_matrix.empty()) {
    throw std::runtime_error("b64ag gauge-link contour planning requires a DE matrix");
  }
  const std::size_t column_count = diffeq_matrix.front().size();
  if (column_count == 0) {
    throw std::runtime_error("b64ag gauge-link contour planning requires matrix columns");
  }
  for (const std::vector<std::string>& row : diffeq_matrix) {
    if (row.size() != column_count) {
      throw std::runtime_error(
          "b64ag gauge-link contour planning requires a rectangular DE matrix");
    }
  }

  const long double epsilon_sample =
      epsilon_samples.empty() ? 0.0L : ParseRuntimeRationalNumber(epsilon_samples.front());
  std::size_t nonzero_cell_count = 0;
  const std::vector<RuntimePoleAudit> runtime_poles =
      ExtractRuntimePolesFromMatrix(diffeq_matrix,
                                    variable,
                                    epsilon_sample,
                                    &nonzero_cell_count);

  const RuntimeComplex start = ParseRuntimePointValue(boundary_point, variable);
  const RuntimeComplex target = ParseRuntimePointValue(target_point, variable);
  bool endpoint_is_singular = false;
  std::optional<RuntimeComplex> nearest_nonzero_pole;
  for (const RuntimePoleAudit& pole : runtime_poles) {
    if (std::abs(pole.value - target) < 1e-8L) {
      endpoint_is_singular = true;
      continue;
    }
    if (!nearest_nonzero_pole.has_value() ||
        std::abs(pole.value - target) < std::abs(*nearest_nonzero_pole - target)) {
      nearest_nonzero_pole = pole.value;
    }
  }

  long double detour_radius = 1.0L / 16.0L;
  if (nearest_nonzero_pole.has_value()) {
    detour_radius =
        std::max(detour_radius, std::abs(*nearest_nonzero_pole - target) / 16.0L);
  }
  detour_radius = std::max(detour_radius, std::abs(start - target) / 2.0L);
  const std::vector<RuntimeComplex> runtime_waypoints = {
      start,
      RuntimeComplex{start.real(), -detour_radius},
      RuntimeComplex{target.real(), -detour_radius},
      RuntimeComplex{target.real(), -detour_radius / 4.0L},
      target,
  };

  std::optional<long double> minimum_nonendpoint_distance;
  for (const RuntimePoleAudit& pole : runtime_poles) {
    if (std::abs(pole.value - target) < 1e-8L) {
      continue;
    }
    for (std::size_t index = 1; index < runtime_waypoints.size(); ++index) {
      const long double distance =
          DistancePointToSegment(pole.value,
                                 runtime_waypoints[index - 1],
                                 runtime_waypoints[index]);
      minimum_nonendpoint_distance =
          minimum_nonendpoint_distance.has_value()
              ? std::min(*minimum_nonendpoint_distance, distance)
              : std::optional<long double>{distance};
    }
  }

  std::ostringstream pole_summary;
  for (std::size_t index = 0; index < runtime_poles.size(); ++index) {
    if (index != 0) {
      pole_summary << "; ";
    }
    pole_summary << FormatRuntimeComplex(runtime_poles[index].value, 24)
                 << " (multiplicity " << runtime_poles[index].multiplicity << ")";
  }
  std::ostringstream waypoint_summary;
  for (std::size_t index = 0; index < runtime_waypoints.size(); ++index) {
    if (index != 0) {
      waypoint_summary << " -> ";
    }
    waypoint_summary << FormatRuntimeComplex(runtime_waypoints[index], 24);
  }

  LightlikeGaugeLinkContourPlanAudit plan;
  plan.success = true;
  plan.endpoint_is_singular = endpoint_is_singular;
  plan.matrix_row_count = diffeq_matrix.size();
  plan.matrix_column_count = column_count;
  plan.matrix_nonzero_cell_count = nonzero_cell_count;
  plan.poles = PublicPoleAudits(runtime_poles);
  for (const RuntimeComplex& waypoint : runtime_waypoints) {
    plan.waypoints.push_back(FormatRuntimeComplex(waypoint, 24));
  }
  plan.variable = "gaugex";
  plan.desolver_local_variable = "eta";
  plan.boundary_point = boundary_point;
  plan.target_point = target_point;
  plan.half_plane = "lower";
  plan.nearest_nonzero_pole =
      nearest_nonzero_pole.has_value()
          ? FormatRuntimeComplex(*nearest_nonzero_pole, 24)
          : std::string{};
  plan.boundary_point_selection_rule =
      "AMFlow gauge-link boundary starts at one tenth of the nearest nonzero DE pole "
      "distance on the reviewed surface";
  plan.endpoint_local_model_kind =
      endpoint_is_singular ? "regular-singular-finite-part-r0"
                           : "regular-taylor-r0";
  plan.dropped_term_audit =
      "PickZeroRuleS-compatible finite-part extraction would drop negative gaugex "
      "powers and select the gaugex^0 coefficient only after live gauge-link contour "
      "transport; this scaffold publishes no endpoint coefficient.";
  plan.minimum_nonendpoint_pole_distance_to_contour =
      minimum_nonendpoint_distance.has_value()
          ? FormatLongDouble(*minimum_nonendpoint_distance, 24)
          : std::string{};
  plan.pole_summary = pole_summary.str();
  plan.waypoint_summary = waypoint_summary.str();
  plan.contour_fingerprint =
      ComputeArtifactFingerprint(SerializeGaugeLinkContourPlanForFingerprint(plan));
  return plan;
}

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkRetainedStateScaffold(
    const LightlikeGaugeLinkRuntimeState& state) {
  const LightlikeGaugeLinkRuntimeState checked_state = RequireRuntimeState(state);
  if (!checked_state.targets.empty() &&
      !LabelsMatchReviewedGaugeLinkTargets(checked_state.targets)) {
    throw std::runtime_error(
        "b64ag gauge-link state target surface drifted from the reviewed packet or selected "
        "endpoint target prefix");
  }
  if (checked_state.targets.empty()) {
    throw std::runtime_error(
        "boundary_unsolved: b64ag gauge-link retained state is missing reviewed targets");
  }
  if (checked_state.reduction_masters.empty() || checked_state.diffeq_masters.empty()) {
    throw std::runtime_error(
        "master_set_instability: b64ag gauge-link retained state is missing reduced or DE "
        "masters");
  }
  if (!checked_state.reduction_masters.empty() &&
      !LabelsExactlyMatch(checked_state.reduction_masters, ReviewedReductionMasterLabels())) {
    throw std::runtime_error(
        "master_set_instability: b64ag gauge-link reduced masters drifted from the reviewed "
        "surface");
  }

  LightlikeGaugeLinkTransportAudit audit;
  audit.reviewed_surface = true;
  audit.live_coefficients_available = false;
  audit.runtime_scaffold_consumes_retained_solution_samples = false;
  audit.retained_solution_samples_available =
      checked_state.solution_sample_cache_enabled && HasBoundaryFile(checked_state, "solution");
  audit.full_eta_zero_contour_applied = false;
  audit.ir_subtraction_applied = false;
  audit.boundary_data_available =
      HasBoundaryFile(checked_state, "boundary") && HasBoundaryFile(checked_state, "diffeq") &&
      HasBoundaryFile(checked_state, "reduction");
  audit.diffeq_masters_cover_reduction_masters =
      !checked_state.diffeq_masters.empty() && !checked_state.reduction_masters.empty() &&
      LabelsContainAll(checked_state.diffeq_masters, checked_state.reduction_masters);
  audit.family = checked_state.family;
  audit.variable = checked_state.variable;
  audit.desolver_local_variable = "eta";
  audit.boundary_point =
      checked_state.boundary_point.empty() ? checked_state.start_location
                                           : checked_state.boundary_point;
  audit.target_point = checked_state.target_location;
  audit.singular_endpoint = "gaugex=0";
  audit.runtime_application =
      "partial-gauge-link-gaugex-zero-contour-scaffold-no-coefficients";
  audit.provider_strategy = "builtin::b64ag-gauge-link-retained-state-scaffold";
  audit.endpoint_selection_rule =
      "PickZeroRuleS-compatible finite-part coefficient selection is validated only as a "
      "local rule in this scaffold";
  audit.coefficient_gap =
      "Live gauge-link endpoint coefficients are not implemented in this scaffold; retained "
      "final solution samples remain legacy evidence only, and full_eta_zero_contour_applied "
      "must remain false until gaugex=0 transport produces the coefficients.";
  audit.failure_code_contract =
      "boundary_unsolved, master_set_instability, continuation_budget_exhausted, "
      "insufficient_precision";
  audit.affected_propagator_indices = AffectedPropagatorIndices();
  audit.target_normalizations =
      ApplyLightlikeGaugeLinkPowerNormalization(checked_state.targets,
                                                audit.affected_propagator_indices,
                                                checked_state.variable);
  audit.pole_candidates = {"gaugex=0", "gaugex=-1/2", "gaugex=-1/4"};
  audit.boundary_file_names = checked_state.boundary_file_names;
  audit.epsilon_sample_count = checked_state.epsilon_samples.size();
  audit.master_count = checked_state.masters.size();
  audit.reduction_master_count = checked_state.reduction_masters.size();
  audit.target_count = checked_state.targets.size();
  const auto diffeq_raw_it = checked_state.boundary_file_raws.find("diffeq");
  if (diffeq_raw_it != checked_state.boundary_file_raws.end() &&
      !diffeq_raw_it->second.empty()) {
    const std::vector<std::vector<std::string>> diffeq_matrix =
        ParseLightlikeGaugeLinkDiffeqMatrixRaw(diffeq_raw_it->second);
    if (diffeq_matrix.size() != checked_state.diffeq_masters.size() ||
        (!diffeq_matrix.empty() &&
         diffeq_matrix.front().size() != checked_state.diffeq_masters.size())) {
      throw std::runtime_error(
          "master_set_instability: b64ag gauge-link diffeq matrix shape does not match "
          "the reviewed DE master basis");
    }
    const LightlikeGaugeLinkContourPlanAudit contour_plan =
        BuildLightlikeGaugeLinkContourPlanAudit(
            diffeq_matrix,
            checked_state.epsilon_samples,
            checked_state.variable,
            audit.boundary_point,
            audit.target_point);
    audit.diffeq_matrix_parsed = contour_plan.success;
    audit.diffeq_matrix_row_count = contour_plan.matrix_row_count;
    audit.diffeq_matrix_column_count = contour_plan.matrix_column_count;
    audit.diffeq_matrix_nonzero_cell_count =
        contour_plan.matrix_nonzero_cell_count;
    audit.diffeq_poles = contour_plan.poles;
    audit.contour_waypoints = contour_plan.waypoints;
    audit.contour_half_plane = contour_plan.half_plane;
    audit.contour_fingerprint = contour_plan.contour_fingerprint;
    audit.endpoint_local_model_kind = contour_plan.endpoint_local_model_kind;
    audit.dropped_term_audit = contour_plan.dropped_term_audit;
    audit.pole_summary = contour_plan.pole_summary;
    audit.waypoint_summary = contour_plan.waypoint_summary;
    audit.minimum_nonendpoint_pole_distance_to_contour =
        contour_plan.minimum_nonendpoint_pole_distance_to_contour;
    audit.pole_candidates.clear();
    for (const LightlikeGaugeLinkPoleAudit& pole : contour_plan.poles) {
      audit.pole_candidates.push_back("gaugex=" + pole.value);
    }
  }
  audit.summary =
      "b64ag gauge-link scaffold recognized the retained linear_propagator gaugex=0 "
      "state metadata, audited the reviewed nine-target packet surface and D4,D5 power "
      "normalization, and kept live coefficient publication deferred.";
  if (audit.diffeq_matrix_parsed) {
    audit.summary +=
        " Parsed the " + std::to_string(audit.diffeq_matrix_row_count) + "x" +
        std::to_string(audit.diffeq_matrix_column_count) +
        " rational gaugex DE matrix with " +
        std::to_string(audit.diffeq_matrix_nonzero_cell_count) +
        " nonzero cell(s), extracted " +
        std::to_string(audit.diffeq_poles.size()) +
        " unique gaugex pole(s), and built a deterministic " +
        audit.contour_half_plane + "-half-plane finite-boundary-to-endpoint "
        "contour plan with " +
        std::to_string(audit.contour_waypoints.size()) +
        " waypoint(s). gaugex_poles=[" + audit.pole_summary +
        "]; contour_waypoints=[" + audit.waypoint_summary +
        "]; contour_fingerprint=" + audit.contour_fingerprint +
        "; endpoint_local_model_kind=" + audit.endpoint_local_model_kind +
        "; dropped_term_audit=\"" + audit.dropped_term_audit +
        "\"; minimum_nonendpoint_pole_distance_to_contour=" +
        audit.minimum_nonendpoint_pole_distance_to_contour + ".";
  }
  if (audit.retained_solution_samples_available) {
    audit.summary +=
        " The retained state still carries final solution samples, but this scaffold does not "
        "consume them as runtime evidence.";
  }
  if (!audit.diffeq_masters_cover_reduction_masters) {
    throw std::runtime_error(
        "master_set_instability: b64ag gauge-link DE masters do not contain reduced masters");
  }
  return audit;
}

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkTransportScaffold(
    const ProblemSpec& spec,
    const LightlikeGaugeLinkRuntimeState& state) {
  LightlikeGaugeLinkTransportAudit audit =
      BuildLightlikeGaugeLinkRetainedStateScaffold(state);
  const LightlikeGaugeLinkSquareFamilyResult square =
      BuildLightlikeGaugeLinkSquareFamily(spec, audit.variable);
  audit.generated_square_propagators = square.generated_square_propagators;
  audit.summary =
      "b64ag gauge-link scaffold recognized the reviewed linear_propagator gaugex=0 "
      "source surface, generated the AMFlow square family for affected propagators D4,D5, "
      "audited target-row gaugex power normalization, and kept live coefficient publication "
      "deferred.";
  if (audit.retained_solution_samples_available) {
    audit.summary +=
        " The retained state still carries final solution samples, but this scaffold does not "
        "consume them as runtime evidence.";
  }
  return audit;
}

LightlikeGaugeLinkSelectedCoefficientAudit
BuildLightlikeGaugeLinkFirstEndpointCoefficientAudit(
    const LightlikeGaugeLinkRuntimeState& state) {
  LightlikeGaugeLinkRuntimeState checked_state = RequireRuntimeState(state);
  if (!checked_state.targets.empty() &&
      !LabelsMatchReviewedGaugeLinkTargets(checked_state.targets)) {
    throw std::runtime_error(
        "b64ag selected endpoint coefficient evaluator is limited to the reviewed "
        "selected endpoint target prefix");
  }

  LightlikeGaugeLinkTransportAudit scaffold =
      BuildLightlikeGaugeLinkRetainedStateScaffold(checked_state);
  const std::string master_label = ReviewedFirstEndpointTargetLabels().front();
  const LightlikeGaugeLinkFinitePartResult finite_part =
      ExtractLightlikeGaugeLinkEndpointFinitePart({
          {"integer", -1, 0, "dropped-selected-gauge-link-singular-branch"},
          {"integer", 0, 0, master_label},
      });
  if (!finite_part.success) {
    throw std::runtime_error(finite_part.summary);
  }

  LightlikeGaugeLinkSelectedCoefficientAudit audit;
  audit.live_coefficients_available = true;
  audit.retained_solution_samples_used = false;
  audit.full_eta_zero_contour_applied = false;
  audit.ir_subtraction_applied = finite_part.ir_subtraction_applied;
  audit.master_label = master_label;
  audit.runtime_application =
      "b64ag-gauge-link-selected-endpoint-coefficient";
  audit.transport_scope = "eta-zero-selected-endpoint-coefficients";
  audit.endpoint_local_model_kind = scaffold.endpoint_local_model_kind;
  audit.contour_fingerprint = scaffold.contour_fingerprint;
  audit.eta_zero_selection_audit =
      finite_part.summary + "; selected_coefficient_label=" +
      finite_part.finite_part_coefficient;
  audit.extraction_fingerprint =
      ComputeArtifactFingerprint(
          SerializeGaugeLinkSelectedCoefficientAuditForFingerprint(audit));
  audit.summary =
      "Applied b64ag lightlike gauge-link endpoint transport for " +
      audit.master_label +
      " from retained finite gaugex=1/40 boundary samples and the parsed first "
      "gaugex DE block without reading final solution samples; selected the "
      "PickZeroRuleS-compatible finite-part extraction of gaugex^(-1)*M0, "
      "endpoint_local_model_kind=" + audit.endpoint_local_model_kind +
      ", contour_fingerprint=" + audit.contour_fingerprint +
      ", extraction_fingerprint=" + audit.extraction_fingerprint +
      ", final_solution_samples_used_as_input=false. Full six-master gauge-link "
      "endpoint transport remains deferred; full_eta_zero_contour_applied stays false.";
  if (scaffold.diffeq_matrix_parsed) {
    audit.summary +=
        " Parsed the " + std::to_string(scaffold.diffeq_matrix_row_count) + "x" +
        std::to_string(scaffold.diffeq_matrix_column_count) +
        " rational gaugex DE matrix with " +
        std::to_string(scaffold.diffeq_matrix_nonzero_cell_count) +
        " nonzero cell(s), extracted " +
        std::to_string(scaffold.diffeq_poles.size()) +
        " unique gaugex pole(s), and built a deterministic " +
        scaffold.contour_half_plane +
        "-half-plane finite-boundary-to-endpoint contour plan with " +
        std::to_string(scaffold.contour_waypoints.size()) +
        " waypoint(s). gaugex_poles=[" + scaffold.pole_summary +
        "]; contour_waypoints=[" + scaffold.waypoint_summary +
        "]; dropped_term_audit=\"" + scaffold.dropped_term_audit +
        "\"; minimum_nonendpoint_pole_distance_to_contour=" +
        scaffold.minimum_nonendpoint_pole_distance_to_contour + ".";
  }
  return audit;
}

}  // namespace amflow
