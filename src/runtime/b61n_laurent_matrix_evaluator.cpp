#include "amflow/runtime/b61n_laurent_matrix_evaluator.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <boost/math/special_functions/fpclassify.hpp>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

using LaurentSeries = std::map<int, ComplexContourNumber>;

struct MatrixSupportLess {
  bool operator()(const B61nMatrixEpsilonSupport& lhs,
                  const B61nMatrixEpsilonSupport& rhs) const {
    return std::tie(lhs.eps_order, lhs.row, lhs.column) <
           std::tie(rhs.eps_order, rhs.row, rhs.column);
  }
};

bool IsFiniteFloat(const ComplexContourFloat& value) {
  return boost::math::isfinite(value);
}

bool IsFiniteComplex(const ComplexContourNumber& value) {
  return IsFiniteFloat(value.real()) && IsFiniteFloat(value.imag());
}

bool IsZeroComplex(const ComplexContourNumber& value) {
  return value.real() == ComplexContourFloat(0) &&
         value.imag() == ComplexContourFloat(0);
}

ComplexContourNumber OneComplex() {
  return {ComplexContourFloat(1), ComplexContourFloat(0)};
}

ComplexContourNumber ImaginaryUnit() {
  return {ComplexContourFloat(0), ComplexContourFloat(1)};
}

std::string NormalizeMathematicaNumericAtom(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](const unsigned char character) {
                               return std::isspace(character) != 0;
                             }),
              value.end());
  for (std::size_t tick = value.find('`'); tick != std::string::npos;
       tick = value.find('`')) {
    std::size_t end = tick + 1;
    while (end < value.size() &&
           (std::isdigit(static_cast<unsigned char>(value[end])) != 0 ||
            value[end] == '.')) {
      ++end;
    }
    value.erase(tick, end - tick);
  }
  for (std::size_t power = value.find("*^"); power != std::string::npos;
       power = value.find("*^", power + 1)) {
    value.replace(power, 2, "e");
  }
  if (!value.empty() && value.back() == '.') {
    value.push_back('0');
  }
  return value;
}

std::string CompactFloat(const ComplexContourFloat& raw_value,
                         const int precision_digits = 50) {
  if (raw_value == ComplexContourFloat(0)) {
    return "0";
  }
  std::ostringstream stream;
  stream << std::setprecision(precision_digits) << raw_value;
  return stream.str();
}

std::string CompactComplex(const ComplexContourNumber& value,
                           const int precision_digits = 50) {
  return "(" + CompactFloat(value.real(), precision_digits) + "," +
         CompactFloat(value.imag(), precision_digits) + ")";
}

void RequireFiniteComplex(const ComplexContourNumber& value,
                          const std::string& context) {
  if (!IsFiniteComplex(value)) {
    throw std::invalid_argument("b61n Laurent matrix evaluator found nonfinite " +
                                context);
  }
}

void RequireSquareMatrix(const std::vector<std::vector<std::string>>& matrix) {
  if (matrix.empty()) {
    throw std::invalid_argument(
        "b61n Laurent matrix evaluator requires a nonempty eta matrix");
  }
  const std::size_t dimension = matrix.size();
  for (const std::vector<std::string>& row : matrix) {
    if (row.size() != dimension) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator encountered a malformed matrix row");
    }
  }
}

void SetLaurentCoefficient(LaurentSeries& series,
                           const int order,
                           const ComplexContourNumber& value) {
  RequireFiniteComplex(value, "Laurent coefficient");
  if (IsZeroComplex(value)) {
    series.erase(order);
    return;
  }
  series[order] = value;
}

void AddLaurentContribution(LaurentSeries& series,
                            const int order,
                            const ComplexContourNumber& value) {
  RequireFiniteComplex(value, "Laurent contribution");
  if (IsZeroComplex(value)) {
    return;
  }
  const auto term_it = series.find(order);
  if (term_it == series.end()) {
    series.emplace(order, value);
    return;
  }
  const ComplexContourNumber sum = term_it->second + value;
  if (IsZeroComplex(sum)) {
    series.erase(term_it);
  } else {
    term_it->second = sum;
  }
}

LaurentSeries ConstantSeries(const ComplexContourNumber& value) {
  LaurentSeries series;
  SetLaurentCoefficient(series, 0, value);
  return series;
}

LaurentSeries EpsilonSeries() {
  LaurentSeries series;
  SetLaurentCoefficient(series, 1, OneComplex());
  return series;
}

LaurentSeries AddSeries(LaurentSeries lhs, const LaurentSeries& rhs) {
  for (const auto& [order, coefficient] : rhs) {
    AddLaurentContribution(lhs, order, coefficient);
  }
  return lhs;
}

LaurentSeries NegateSeries(const LaurentSeries& value) {
  LaurentSeries result;
  for (const auto& [order, coefficient] : value) {
    SetLaurentCoefficient(result, order, -coefficient);
  }
  return result;
}

LaurentSeries SubtractSeries(LaurentSeries lhs, const LaurentSeries& rhs) {
  return AddSeries(std::move(lhs), NegateSeries(rhs));
}

LaurentSeries MultiplySeries(const LaurentSeries& lhs,
                             const LaurentSeries& rhs) {
  LaurentSeries result;
  for (const auto& [lhs_order, lhs_coefficient] : lhs) {
    for (const auto& [rhs_order, rhs_coefficient] : rhs) {
      AddLaurentContribution(result,
                             lhs_order + rhs_order,
                             lhs_coefficient * rhs_coefficient);
    }
  }
  return result;
}

LaurentSeries DivideSeriesByMonomial(const LaurentSeries& numerator,
                                     const LaurentSeries& denominator,
                                     const std::string& expression) {
  if (denominator.empty()) {
    throw std::invalid_argument(
        "b61n Laurent matrix evaluator divides by zero in \"" + expression +
        "\"");
  }
  if (denominator.size() != 1) {
    throw std::invalid_argument(
        "b61n Laurent matrix evaluator requires epsilon-independent or "
        "monomial epsilon denominators in \"" +
        expression + "\"");
  }
  const int denominator_order = denominator.begin()->first;
  const ComplexContourNumber denominator_value = denominator.begin()->second;
  if (IsZeroComplex(denominator_value)) {
    throw std::invalid_argument(
        "b61n Laurent matrix evaluator divides by zero in \"" + expression +
        "\"");
  }
  LaurentSeries result;
  for (const auto& [order, coefficient] : numerator) {
    SetLaurentCoefficient(result,
                          order - denominator_order,
                          coefficient / denominator_value);
  }
  return result;
}

LaurentSeries PowerSeries(LaurentSeries base,
                          int exponent,
                          const std::string& expression) {
  if (exponent == 0) {
    return ConstantSeries(OneComplex());
  }
  if (exponent < 0) {
    LaurentSeries numerator = ConstantSeries(OneComplex());
    while (exponent < 0) {
      numerator = DivideSeriesByMonomial(numerator, base, expression);
      ++exponent;
    }
    return numerator;
  }
  if (exponent > 64) {
    throw std::invalid_argument(
        "b61n Laurent matrix evaluator refuses an exponent above 64 in \"" +
        expression + "\"");
  }
  LaurentSeries result = ConstantSeries(OneComplex());
  for (int index = 0; index < exponent; ++index) {
    result = MultiplySeries(result, base);
  }
  return result;
}

class LaurentExpressionParser {
 public:
  LaurentExpressionParser(std::string expression,
                          std::string variable_name,
                          B61nLaurentNumericSubstitutions substitutions,
                          ComplexContourNumber eta)
      : expression_(NormalizeMathematicaNumericAtom(std::move(expression))),
        variable_name_(std::move(variable_name)),
        substitutions_(std::move(substitutions)),
        eta_(eta) {}

  LaurentSeries Parse() {
    if (expression_.empty()) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator received an empty expression");
    }
    if (variable_name_.empty()) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator requires a variable name");
    }
    if (substitutions_.find("eps") != substitutions_.end()) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator reserves eps for Laurent expansion");
    }
    if (substitutions_.find("I") != substitutions_.end()) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator reserves I for the imaginary unit");
    }
    RequireFiniteComplex(eta_, "eta binding");
    for (const auto& [symbol, value] : substitutions_) {
      if (symbol.empty()) {
        throw std::invalid_argument(
            "b61n Laurent matrix evaluator received an empty substitution symbol");
      }
      RequireFiniteComplex(value, "numeric substitution " + symbol);
    }

    LaurentSeries value = ParseExpression();
    if (position_ != expression_.size()) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator found trailing input in \"" +
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
    const char character = expression_[position_];
    return character == '(' || character == '.' ||
           std::isdigit(static_cast<unsigned char>(character)) != 0 ||
           std::isalpha(static_cast<unsigned char>(character)) != 0 ||
           character == '_';
  }

  LaurentSeries ParseExpression() {
    LaurentSeries value = ParseTerm();
    while (position_ < expression_.size()) {
      if (Consume('+')) {
        value = AddSeries(std::move(value), ParseTerm());
      } else if (Consume('-')) {
        value = SubtractSeries(std::move(value), ParseTerm());
      } else {
        break;
      }
    }
    return value;
  }

  LaurentSeries ParseTerm() {
    LaurentSeries value = ParsePower();
    while (position_ < expression_.size()) {
      if (Consume('*')) {
        value = MultiplySeries(value, ParsePower());
      } else if (Consume('/')) {
        value = DivideSeriesByMonomial(value, ParsePower(), expression_);
      } else if (StartsPrimary()) {
        value = MultiplySeries(value, ParsePower());
      } else {
        break;
      }
    }
    return value;
  }

  LaurentSeries ParsePower() {
    LaurentSeries value = ParseUnary();
    if (Consume('^')) {
      value = PowerSeries(std::move(value),
                          ParseSignedIntegerExponent(),
                          expression_);
    }
    return value;
  }

  LaurentSeries ParseUnary() {
    if (Consume('+')) {
      return ParseUnary();
    }
    if (Consume('-')) {
      return NegateSeries(ParseUnary());
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
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator requires integer exponents in \"" +
          expression_ + "\"");
    }
    int exponent = 0;
    while (position_ < expression_.size() &&
           std::isdigit(static_cast<unsigned char>(expression_[position_])) !=
               0) {
      exponent = exponent * 10 + (expression_[position_] - '0');
      ++position_;
    }
    if (parenthesized && !Consume(')')) {
      throw std::invalid_argument(
          "b61n Laurent matrix evaluator expected ')' after exponent in \"" +
          expression_ + "\"");
    }
    return negative ? -exponent : exponent;
  }

  LaurentSeries ParsePrimary() {
    if (Consume('(')) {
      LaurentSeries value = ParseExpression();
      if (!Consume(')')) {
        throw std::invalid_argument(
            "b61n Laurent matrix evaluator expected ')' in \"" + expression_ +
            "\"");
      }
      return value;
    }

    if (position_ < expression_.size() &&
        (std::isdigit(static_cast<unsigned char>(expression_[position_])) !=
             0 ||
         expression_[position_] == '.')) {
      const std::size_t begin = position_;
      bool saw_digit = false;
      while (position_ < expression_.size() &&
             std::isdigit(static_cast<unsigned char>(expression_[position_])) !=
                 0) {
        saw_digit = true;
        ++position_;
      }
      if (Consume('.')) {
        while (position_ < expression_.size() &&
               std::isdigit(static_cast<unsigned char>(expression_[position_])) !=
                   0) {
          saw_digit = true;
          ++position_;
        }
      }
      if (!saw_digit) {
        throw std::invalid_argument(
            "b61n Laurent matrix evaluator found malformed number in \"" +
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
            std::isdigit(static_cast<unsigned char>(expression_[position_])) ==
                0) {
          throw std::invalid_argument(
              "b61n Laurent matrix evaluator found malformed exponent in \"" +
              expression_ + "\"");
        }
        while (position_ < expression_.size() &&
               std::isdigit(static_cast<unsigned char>(expression_[position_])) !=
                   0) {
          ++position_;
        }
      }
      return ConstantSeries(
          {ComplexContourFloat(expression_.substr(begin, position_ - begin)),
           ComplexContourFloat(0)});
    }

    if (position_ < expression_.size() &&
        (std::isalpha(static_cast<unsigned char>(expression_[position_])) !=
             0 ||
         expression_[position_] == '_')) {
      const std::size_t begin = position_;
      ++position_;
      while (position_ < expression_.size() &&
             (std::isalnum(static_cast<unsigned char>(expression_[position_])) !=
                  0 ||
              expression_[position_] == '_')) {
        ++position_;
      }
      const std::string identifier = expression_.substr(begin, position_ - begin);
      if (identifier == "eps") {
        return EpsilonSeries();
      }
      if (identifier == "I") {
        return ConstantSeries(ImaginaryUnit());
      }
      if (identifier == variable_name_) {
        return ConstantSeries(eta_);
      }
      const auto binding_it = substitutions_.find(identifier);
      if (binding_it == substitutions_.end()) {
        throw std::invalid_argument(
            "b61n Laurent matrix evaluator requires a binding for " +
            identifier + " in \"" + expression_ + "\"");
      }
      return ConstantSeries(binding_it->second);
    }

    throw std::invalid_argument(
        "b61n Laurent matrix evaluator found malformed expression \"" +
        expression_ + "\"");
  }

  std::string expression_;
  std::string variable_name_;
  B61nLaurentNumericSubstitutions substitutions_;
  ComplexContourNumber eta_;
  std::size_t position_ = 0;
};

std::vector<B61nMatrixEpsilonSupport> SortedSupport(
    const std::vector<B61nMatrixEpsilonSupport>& support) {
  std::set<B61nMatrixEpsilonSupport, MatrixSupportLess> unique(
      support.begin(), support.end());
  return {unique.begin(), unique.end()};
}

std::string FormatSupport(
    const std::vector<B61nMatrixEpsilonSupport>& support) {
  std::ostringstream out;
  for (std::size_t index = 0; index < support.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << "eps^" << support[index].eps_order << ":r" << support[index].row
        << "<-c" << support[index].column;
  }
  return out.str();
}

std::string SerializeTargetGraphForFingerprint(
    const B61nCoefficientTargetGraph& target_graph) {
  std::ostringstream out;
  out << "public_targets=";
  for (const B61nCoefficientNode& node : target_graph.public_targets) {
    out << "(" << node.master_index << "," << node.eps_order << ")";
  }
  out << "\nreviewed_source_anchor_nodes=";
  for (const B61nCoefficientNode& node :
       target_graph.reviewed_source_anchor_nodes) {
    out << "(" << node.master_index << "," << node.eps_order << ")";
  }
  out << "\nclosed_nodes=";
  for (const B61nCoefficientNode& node : target_graph.closed_nodes) {
    out << "(" << node.master_index << "," << node.eps_order << ")";
  }
  out << "\ndependency_edges=";
  for (const B61nCoefficientGraphEdge& edge :
       target_graph.dependency_edges) {
    out << "(" << edge.target.master_index << "," << edge.target.eps_order
        << "<-" << edge.source.master_index << "," << edge.source.eps_order
        << ";m=" << edge.matrix_eps_order << ")";
  }
  out << "\nblocked_edges=";
  for (const B61nBlockedCoefficientGraphEdge& edge :
       target_graph.blocked_edges) {
    out << "(" << edge.target.master_index << "," << edge.target.eps_order
        << "<-" << edge.source_master_index << "," << edge.source_eps_order
        << ";m=" << edge.matrix_eps_order << ";" << edge.reason << ")";
  }
  out << "\nsummary=" << target_graph.summary << "\n";
  return out.str();
}

B61nLaurentMatrixEvaluatorAudit MakeAudit(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name,
    const B61nLaurentNumericSubstitutions& numeric_substitutions,
    const B61nCoefficientTargetGraph& target_graph) {
  RequireSquareMatrix(matrix);
  const std::vector<B61nMatrixEpsilonSupport> support =
      ExtractB61nMatrixEpsilonSupport(matrix);
  B61nLaurentMatrixEvaluatorAudit audit;
  audit.nonzero_support = SortedSupport(support);
  audit.min_matrix_eps_order = audit.nonzero_support.front().eps_order;
  audit.max_matrix_eps_order = audit.nonzero_support.front().eps_order;
  for (const B61nMatrixEpsilonSupport& entry : audit.nonzero_support) {
    audit.min_matrix_eps_order =
        std::min(audit.min_matrix_eps_order, entry.eps_order);
    audit.max_matrix_eps_order =
        std::max(audit.max_matrix_eps_order, entry.eps_order);
  }
  audit.matrix_coefficient_count = static_cast<std::size_t>(
      static_cast<long long>(audit.max_matrix_eps_order) -
      static_cast<long long>(audit.min_matrix_eps_order) + 1LL);

  std::ostringstream fingerprint_payload;
  fingerprint_payload << "kind=b61n-real-laurent-matrix-evaluator\n";
  fingerprint_payload << "variable=" << variable_name << "\n";
  fingerprint_payload << "dimension=" << matrix.size() << "\n";
  fingerprint_payload << "min_matrix_eps_order="
                      << audit.min_matrix_eps_order << "\n";
  fingerprint_payload << "max_matrix_eps_order="
                      << audit.max_matrix_eps_order << "\n";
  fingerprint_payload << "matrix_coefficient_count="
                      << audit.matrix_coefficient_count << "\n";
  fingerprint_payload << "support=" << FormatSupport(audit.nonzero_support)
                      << "\n";
  for (const auto& [symbol, value] : numeric_substitutions) {
    fingerprint_payload << "substitution[" << symbol << "]="
                        << CompactComplex(value, 70) << "\n";
  }
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      fingerprint_payload << "matrix[" << row << "," << column
                          << "]=" << matrix[row][column] << "\n";
    }
  }
  fingerprint_payload << SerializeTargetGraphForFingerprint(target_graph);
  audit.fingerprint = ComputeArtifactFingerprint(fingerprint_payload.str());

  audit.summary =
      "b61n real eta matrix Laurent evaluator prepared " +
      std::to_string(audit.matrix_coefficient_count) +
      " ordered epsilon matrix coefficient(s) over matrix_eps_order_min=" +
      std::to_string(audit.min_matrix_eps_order) +
      "; matrix_eps_order_max=" +
      std::to_string(audit.max_matrix_eps_order) +
      "; nonzero_matrix_support_count=" +
      std::to_string(audit.nonzero_support.size()) +
      "; nonzero_matrix_support=[" + FormatSupport(audit.nonzero_support) +
      "]; target_graph_node_count=" +
      std::to_string(target_graph.closed_nodes.size()) +
      "; target_graph_edge_count=" +
      std::to_string(target_graph.dependency_edges.size()) +
      "; target_graph_blocked_edge_count=" +
      std::to_string(target_graph.blocked_edges.size()) +
      "; fingerprint=" + audit.fingerprint +
      "; target_coefficients_published_from_coefficient_state=false; "
      "target_coefficients_reconstructed_from_epsilon_samples=false";
  return audit;
}

}  // namespace

B61nLaurentMatrixEvaluation EvaluateB61nLaurentMatrixCoefficientsAtEta(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name,
    const B61nLaurentNumericSubstitutions& numeric_substitutions,
    const ComplexContourNumber& eta) {
  RequireSquareMatrix(matrix);
  const std::size_t dimension = matrix.size();
  std::vector<std::tuple<int, std::size_t, std::size_t, ComplexContourNumber>>
      terms;
  std::vector<B61nMatrixEpsilonSupport> support;
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < dimension; ++column) {
      const LaurentSeries series =
          LaurentExpressionParser(matrix[row][column],
                                  variable_name,
                                  numeric_substitutions,
                                  eta)
              .Parse();
      for (const auto& [eps_order, coefficient] : series) {
        RequireFiniteComplex(coefficient, "matrix Laurent coefficient");
        if (IsZeroComplex(coefficient)) {
          continue;
        }
        terms.emplace_back(eps_order, row, column, coefficient);
        support.push_back({row, column, eps_order});
      }
    }
  }
  if (terms.empty()) {
    throw std::invalid_argument(
        "b61n Laurent matrix evaluator found empty matrix epsilon support");
  }

  B61nLaurentMatrixEvaluation evaluation;
  evaluation.nonzero_support = SortedSupport(support);
  evaluation.min_matrix_eps_order = std::get<0>(terms.front());
  evaluation.max_matrix_eps_order = std::get<0>(terms.front());
  for (const auto& term : terms) {
    evaluation.min_matrix_eps_order =
        std::min(evaluation.min_matrix_eps_order, std::get<0>(term));
    evaluation.max_matrix_eps_order =
        std::max(evaluation.max_matrix_eps_order, std::get<0>(term));
  }
  evaluation.matrix_coefficient_count = static_cast<std::size_t>(
      static_cast<long long>(evaluation.max_matrix_eps_order) -
      static_cast<long long>(evaluation.min_matrix_eps_order) + 1LL);
  evaluation.matrix_coefficients.assign(
      evaluation.matrix_coefficient_count,
      ComplexContourMatrix(
          dimension, std::vector<ComplexContourNumber>(dimension,
                                                       ComplexContourNumber{})));
  for (const auto& [eps_order, row, column, coefficient] : terms) {
    const std::size_t order_index =
        static_cast<std::size_t>(eps_order - evaluation.min_matrix_eps_order);
    evaluation.matrix_coefficients[order_index][row][column] = coefficient;
  }
  return evaluation;
}

B61nLaurentMatrixEvaluator BuildB61nRealLaurentMatrixEvaluator(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name,
    const B61nLaurentNumericSubstitutions& numeric_substitutions,
    const B61nCoefficientTargetGraph& target_graph) {
  B61nLaurentMatrixEvaluator result;
  result.audit =
      MakeAudit(matrix, variable_name, numeric_substitutions, target_graph);
  result.evaluator =
      [matrix, variable_name, numeric_substitutions](
          const ComplexContourNumber& eta) {
        return EvaluateB61nLaurentMatrixCoefficientsAtEta(
                   matrix, variable_name, numeric_substitutions, eta)
            .matrix_coefficients;
      };
  return result;
}

}  // namespace amflow
