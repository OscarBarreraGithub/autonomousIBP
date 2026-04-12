#pragma once

#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

enum class PointClassification {
  Regular,
  Singular,
};

struct FiniteSingularPoint {
  ExactRational location;
};

std::vector<FiniteSingularPoint> DetectFiniteSingularPoints(
    const DESystem& system,
    const std::string& variable_name,
    const NumericEvaluationPoint& passive_bindings);

PointClassification ClassifyFinitePoint(const DESystem& system,
                                        const std::string& variable_name,
                                        const std::string& point_expression,
                                        const NumericEvaluationPoint& passive_bindings);

}  // namespace amflow
