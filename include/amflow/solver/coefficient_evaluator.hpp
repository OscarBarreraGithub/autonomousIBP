#pragma once

#include <map>
#include <string>
#include <vector>

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

using ExactRationalMatrix = std::vector<std::vector<ExactRational>>;
using NumericEvaluationPoint = std::map<std::string, std::string>;

ExactRational EvaluateCoefficientExpression(const std::string& expression,
                                            const NumericEvaluationPoint& evaluation_point);

ExactRationalMatrix EvaluateCoefficientMatrix(const DESystem& system,
                                              const std::string& variable_name,
                                              const NumericEvaluationPoint& evaluation_point);

}  // namespace amflow
