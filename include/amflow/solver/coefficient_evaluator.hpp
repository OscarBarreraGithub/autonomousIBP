#pragma once

#include <map>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"
#include "amflow/core/de_system.hpp"

namespace amflow {

struct ExactRational {
  std::string numerator = "0";
  std::string denominator = "1";

  bool IsZero() const;
  std::string ToString() const;
};

bool operator==(const ExactRational& lhs, const ExactRational& rhs);
bool operator!=(const ExactRational& lhs, const ExactRational& rhs);

struct ExactComplexRational {
  ExactRational real = {"0", "1"};
  ExactRational imaginary = {"0", "1"};

  bool IsReal() const;
  std::string ToString() const;
};

bool operator==(const ExactComplexRational& lhs, const ExactComplexRational& rhs);
bool operator!=(const ExactComplexRational& lhs, const ExactComplexRational& rhs);

using ExactRationalMatrix = std::vector<std::vector<ExactRational>>;
using ExactComplexRationalMatrix = std::vector<std::vector<ExactComplexRational>>;
using NumericEvaluationPoint = std::map<std::string, std::string>;

ExactRational EvaluateCoefficientExpression(const std::string& expression,
                                            const NumericEvaluationPoint& evaluation_point);

ExactRationalMatrix EvaluateCoefficientMatrix(const DESystem& system,
                                              const std::string& variable_name,
                                              const NumericEvaluationPoint& evaluation_point);

NumericEvaluationPoint BuildComplexNumericEvaluationPoint(const ProblemSpec& spec);

ExactComplexRational EvaluateComplexCoefficientExpression(
    const std::string& expression,
    const NumericEvaluationPoint& evaluation_point);

ExactComplexRationalMatrix EvaluateComplexCoefficientMatrix(
    const DESystem& system,
    const std::string& variable_name,
    const NumericEvaluationPoint& evaluation_point);

ExactComplexRational EvaluateComplexPointExpression(
    const std::string& variable_name,
    const std::string& point_expression,
    const NumericEvaluationPoint& evaluation_point);

}  // namespace amflow
