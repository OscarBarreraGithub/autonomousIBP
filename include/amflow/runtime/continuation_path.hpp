#pragma once

#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

enum class EtaContourHalfPlane {
  Upper,
  Lower
};

std::string ToString(EtaContourHalfPlane half_plane);

struct EtaContourSingularPoint {
  std::string expression;
  ExactComplexRational value;
  int branch_winding = 0;
};

struct EtaContinuationPlan {
  std::string eta_symbol;
  std::string start_location;
  std::string target_location;
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Upper;
  std::vector<ExactComplexRational> contour_points;
  std::vector<EtaContourSingularPoint> singular_points;
  std::string contour_fingerprint;
};

EtaContinuationPlan FinalizeEtaContinuationContour(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::vector<std::string>& contour_point_expressions,
    EtaContourHalfPlane half_plane = EtaContourHalfPlane::Upper);

EtaContinuationPlan PlanEtaContinuationContour(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::string& start_location,
    const std::string& target_location,
    EtaContourHalfPlane half_plane = EtaContourHalfPlane::Upper);

}  // namespace amflow
