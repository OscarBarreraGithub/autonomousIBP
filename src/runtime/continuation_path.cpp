#include "amflow/runtime/continuation_path.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

ExactRational ExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, {});
}

ExactRational ZeroRational() {
  return {"0", "1"};
}

std::string Parenthesize(const ExactRational& value) {
  return "(" + value.ToString() + ")";
}

ExactRational AddRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "+" + Parenthesize(rhs));
}

ExactRational SubtractRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "-" + Parenthesize(rhs));
}

ExactRational MultiplyRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "*" + Parenthesize(rhs));
}

ExactRational DivideRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "/" + Parenthesize(rhs));
}

ExactComplexRational MakeComplexRational(const ExactRational& real,
                                         const ExactRational& imaginary) {
  ExactComplexRational value;
  value.real = real;
  value.imaginary = imaginary;
  return value;
}

int CompareRational(const ExactRational& lhs, const ExactRational& rhs) {
  const ExactRational difference = SubtractRational(lhs, rhs);
  if (difference.IsZero()) {
    return 0;
  }
  return !difference.numerator.empty() && difference.numerator.front() == '-' ? -1 : 1;
}

bool IsStrictlyBetween(const ExactRational& value,
                       const ExactRational& first,
                       const ExactRational& second) {
  if (CompareRational(first, second) < 0) {
    return CompareRational(first, value) < 0 && CompareRational(value, second) < 0;
  }
  return CompareRational(second, value) < 0 && CompareRational(value, first) < 0;
}

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index != 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

std::size_t CountDecimalDigits(const std::string& value) {
  return !value.empty() && value.front() == '-' ? value.size() - 1 : value.size();
}

void ValidateReviewedWindingProjectionValue(const ExactRational& value) {
  constexpr std::size_t kReviewedDigits =
      static_cast<std::size_t>(std::numeric_limits<long double>::digits10 - 1);
  if (CountDecimalDigits(value.numerator) > kReviewedDigits ||
      CountDecimalDigits(value.denominator) > kReviewedDigits) {
    throw std::invalid_argument(
        "eta continuation contour branch ledger currently supports only moderate-size exact "
        "rational coordinates within the reviewed long-double projection range");
  }
}

long double RationalToLongDouble(const ExactRational& value) {
  ValidateReviewedWindingProjectionValue(value);
  const long double numerator = std::stold(value.numerator);
  const long double denominator = std::stold(value.denominator);
  const long double projected = numerator / denominator;
  if (!std::isfinite(projected)) {
    throw std::invalid_argument(
        "eta continuation contour branch ledger currently supports only finite projected "
        "coordinates");
  }
  return projected;
}

long double UnwrapAngleDelta(long double delta) {
  constexpr long double kPi = 3.141592653589793238462643383279502884L;
  constexpr long double kTwoPi = 2.0L * kPi;
  while (delta <= -kPi) {
    delta += kTwoPi;
  }
  while (delta > kPi) {
    delta -= kTwoPi;
  }
  return delta;
}

std::vector<EtaContourSingularPoint> EvaluateEtaSingularPoints(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol) {
  const NumericEvaluationPoint evaluation_point = BuildComplexNumericEvaluationPoint(spec);

  std::vector<EtaContourSingularPoint> singular_points;
  singular_points.reserve(system.singular_points.size());
  for (const std::string& expression : system.singular_points) {
    EtaContourSingularPoint singular_point;
    singular_point.expression = expression;
    singular_point.value =
        EvaluateComplexPointExpression(eta_symbol, expression, evaluation_point);
    const auto duplicate = std::find_if(
        singular_points.begin(),
        singular_points.end(),
        [&singular_point](const EtaContourSingularPoint& candidate) {
          return candidate.value == singular_point.value;
        });
    if (duplicate != singular_points.end()) {
      if (singular_point.expression < duplicate->expression) {
        duplicate->expression = singular_point.expression;
      }
      continue;
    }
    singular_points.push_back(std::move(singular_point));
  }
  std::sort(singular_points.begin(),
            singular_points.end(),
            [](const EtaContourSingularPoint& lhs, const EtaContourSingularPoint& rhs) {
              const int real_comparison = CompareRational(lhs.value.real, rhs.value.real);
              if (real_comparison != 0) {
                return real_comparison < 0;
              }
              const int imaginary_comparison =
                  CompareRational(lhs.value.imaginary, rhs.value.imaginary);
              if (imaginary_comparison != 0) {
                return imaginary_comparison < 0;
              }
              return lhs.expression < rhs.expression;
            });
  return singular_points;
}

std::vector<ExactComplexRational> EvaluateContourPoints(
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::vector<std::string>& contour_point_expressions) {
  const NumericEvaluationPoint evaluation_point = BuildComplexNumericEvaluationPoint(spec);

  std::vector<ExactComplexRational> contour_points;
  contour_points.reserve(contour_point_expressions.size());
  for (const std::string& expression : contour_point_expressions) {
    contour_points.push_back(
        EvaluateComplexPointExpression(eta_symbol, expression, evaluation_point));
  }
  return contour_points;
}

EtaContourSingularPoint ResolveTargetEndpointSingular(
    const std::vector<EtaContourSingularPoint>& singular_points,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::string& target_endpoint_singular_expression,
    const ExactComplexRational& target) {
  if (target_endpoint_singular_expression.empty()) {
    throw std::invalid_argument(
        "eta continuation target endpoint singular expression must not be empty");
  }

  const NumericEvaluationPoint evaluation_point = BuildComplexNumericEvaluationPoint(spec);
  const ExactComplexRational marker_value = EvaluateComplexPointExpression(
      eta_symbol, target_endpoint_singular_expression, evaluation_point);
  if (marker_value != target) {
    throw std::invalid_argument(
        "eta continuation target endpoint singular expression " +
        target_endpoint_singular_expression + " evaluates to " + marker_value.ToString() +
        " instead of target endpoint " + target.ToString());
  }

  const auto declared_singular = std::find_if(
      singular_points.begin(),
      singular_points.end(),
      [&marker_value](const EtaContourSingularPoint& singular_point) {
        return singular_point.value == marker_value;
      });
  if (declared_singular == singular_points.end()) {
    throw std::invalid_argument(
        "eta continuation target endpoint singular expression " +
        target_endpoint_singular_expression +
        " must match a declared evaluated singular point by exact value");
  }

  EtaContourSingularPoint endpoint_singular = *declared_singular;
  endpoint_singular.branch_winding = 0;
  return endpoint_singular;
}

void ValidateEtaContinuationSymbol(const DESystem& system,
                                   const std::string& eta_symbol) {
  if (eta_symbol.empty()) {
    throw std::invalid_argument("eta continuation contour eta_symbol must not be empty");
  }
  const bool declares_eta_symbol =
      std::any_of(system.variables.begin(),
                  system.variables.end(),
                  [&eta_symbol](const DifferentiationVariable& variable) {
                    return variable.name == eta_symbol &&
                           variable.kind == DifferentiationVariableKind::Eta;
                  });
  if (!declares_eta_symbol) {
    throw std::invalid_argument(
        "eta continuation contour eta_symbol must name a declared Eta differentiation variable");
  }
}

bool SegmentContainsSingularPoint(const ExactComplexRational& start,
                                  const ExactComplexRational& end,
                                  const ExactComplexRational& singular_point) {
  const ExactRational delta_real = SubtractRational(end.real, start.real);
  const ExactRational delta_imaginary = SubtractRational(end.imaginary, start.imaginary);
  const ExactRational offset_real = SubtractRational(singular_point.real, start.real);
  const ExactRational offset_imaginary =
      SubtractRational(singular_point.imaginary, start.imaginary);
  const ExactRational cross_product = SubtractRational(
      MultiplyRational(offset_real, delta_imaginary),
      MultiplyRational(offset_imaginary, delta_real));
  if (!cross_product.IsZero()) {
    return false;
  }

  const ExactRational dot_product = AddRational(
      MultiplyRational(offset_real, delta_real),
      MultiplyRational(offset_imaginary, delta_imaginary));
  if (CompareRational(dot_product, ZeroRational()) < 0) {
    return false;
  }

  const ExactRational squared_length = AddRational(
      MultiplyRational(delta_real, delta_real),
      MultiplyRational(delta_imaginary, delta_imaginary));
  return CompareRational(dot_product, squared_length) <= 0;
}

long long ComputeBranchWinding(const std::vector<ExactComplexRational>& contour_points,
                               const ExactComplexRational& singular_point) {
  constexpr long double kPi = 3.141592653589793238462643383279502884L;
  constexpr long double kTwoPi = 2.0L * kPi;

  long double total_argument_change = 0.0L;
  for (std::size_t index = 1; index < contour_points.size(); ++index) {
    const ExactComplexRational& previous = contour_points[index - 1];
    const ExactComplexRational& current = contour_points[index];
    const long double previous_angle =
        std::atan2(RationalToLongDouble(SubtractRational(previous.imaginary,
                                                         singular_point.imaginary)),
                   RationalToLongDouble(SubtractRational(previous.real,
                                                         singular_point.real)));
    const long double current_angle =
        std::atan2(RationalToLongDouble(SubtractRational(current.imaginary,
                                                         singular_point.imaginary)),
                   RationalToLongDouble(SubtractRational(current.real,
                                                         singular_point.real)));
    total_argument_change += UnwrapAngleDelta(current_angle - previous_angle);
  }

  const ExactComplexRational& start = contour_points.front();
  const ExactComplexRational& target = contour_points.back();
  const long double start_angle =
      std::atan2(RationalToLongDouble(SubtractRational(start.imaginary,
                                                       singular_point.imaginary)),
                 RationalToLongDouble(SubtractRational(start.real, singular_point.real)));
  const long double target_angle =
      std::atan2(RationalToLongDouble(SubtractRational(target.imaginary,
                                                       singular_point.imaginary)),
                 RationalToLongDouble(SubtractRational(target.real, singular_point.real)));
  const long double principal_delta = UnwrapAngleDelta(target_angle - start_angle);

  return static_cast<long long>(
      std::llround((total_argument_change - principal_delta) / kTwoPi));
}

bool SameSingularEndpointIdentity(const EtaContourSingularPoint& lhs,
                                  const EtaContourSingularPoint& rhs) {
  return lhs.expression == rhs.expression && lhs.value == rhs.value;
}

bool IsMarkedTargetEndpointSingular(const EtaContinuationPlan& plan,
                                    const EtaContourSingularPoint& singular_point) {
  return plan.target_endpoint_singular.has_value() &&
         SameSingularEndpointIdentity(*plan.target_endpoint_singular, singular_point);
}

std::string SerializeEtaContinuationPlanForFingerprint(const EtaContinuationPlan& plan) {
  std::ostringstream out;
  out << "eta_symbol=" << plan.eta_symbol << "\n";
  out << "start_location=" << plan.start_location << "\n";
  out << "target_location=" << plan.target_location << "\n";
  out << "half_plane=" << ToString(plan.half_plane) << "\n";
  out << "contour_points=" << plan.contour_points.size() << "\n";
  for (std::size_t index = 0; index < plan.contour_points.size(); ++index) {
    out << "contour_point[" << index << "]=" << plan.contour_points[index].ToString() << "\n";
  }
  out << "singular_points=" << plan.singular_points.size() << "\n";
  for (std::size_t index = 0; index < plan.singular_points.size(); ++index) {
    const EtaContourSingularPoint& singular_point = plan.singular_points[index];
    out << "singular_point[" << index << "].expression=" << singular_point.expression << "\n";
    out << "singular_point[" << index << "].value=" << singular_point.value.ToString() << "\n";
    out << "singular_point[" << index << "].branch_winding="
        << singular_point.branch_winding << "\n";
  }
  if (plan.target_endpoint_singular.has_value()) {
    const EtaContourSingularPoint& endpoint_singular = *plan.target_endpoint_singular;
    out << "target_endpoint_singular.expression=" << endpoint_singular.expression << "\n";
    out << "target_endpoint_singular.value=" << endpoint_singular.value.ToString() << "\n";
    out << "target_endpoint_singular.branch_winding="
        << endpoint_singular.branch_winding << "\n";
  }
  return out.str();
}

EtaContinuationPlan FinalizeEtaContinuationContourImpl(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::string& start_location,
    const std::string& target_location,
    const EtaContourHalfPlane half_plane,
    const std::optional<std::string>& target_endpoint_singular_expression,
    const std::vector<ExactComplexRational>& contour_points) {
  ValidateEtaContinuationSymbol(system, eta_symbol);
  if (contour_points.size() < 2) {
    throw std::invalid_argument(
        "eta continuation contour requires at least two contour points");
  }
  for (std::size_t index = 1; index < contour_points.size(); ++index) {
    if (contour_points[index - 1] == contour_points[index]) {
      throw std::invalid_argument(
          "eta continuation contour requires distinct adjacent contour points");
    }
  }

  EtaContinuationPlan plan;
  plan.eta_symbol = eta_symbol;
  plan.start_location = start_location;
  plan.target_location = target_location;
  plan.half_plane = half_plane;
  plan.contour_points = contour_points;
  plan.singular_points = EvaluateEtaSingularPoints(system, spec, eta_symbol);
  if (target_endpoint_singular_expression.has_value()) {
    plan.target_endpoint_singular =
        ResolveTargetEndpointSingular(plan.singular_points,
                                      spec,
                                      eta_symbol,
                                      *target_endpoint_singular_expression,
                                      plan.contour_points.back());
  }

  for (EtaContourSingularPoint& singular_point : plan.singular_points) {
    const bool marked_target_endpoint =
        IsMarkedTargetEndpointSingular(plan, singular_point);
    for (std::size_t point_index = 0; point_index < plan.contour_points.size();
         ++point_index) {
      if (plan.contour_points[point_index] == singular_point.value) {
        const bool is_marked_target_point =
            marked_target_endpoint && point_index + 1 == plan.contour_points.size();
        if (is_marked_target_point) {
          continue;
        }
        throw std::invalid_argument("eta continuation contour point lands on evaluated singular "
                                    "point " +
                                    singular_point.expression + " = " +
                                    singular_point.value.ToString());
      }
    }

    for (std::size_t index = 1; index < plan.contour_points.size(); ++index) {
      if (SegmentContainsSingularPoint(plan.contour_points[index - 1],
                                      plan.contour_points[index],
                                      singular_point.value)) {
        const bool segment_terminates_at_marked_target =
            marked_target_endpoint && index + 1 == plan.contour_points.size() &&
            plan.contour_points[index] == singular_point.value;
        if (segment_terminates_at_marked_target) {
          continue;
        }
        throw std::invalid_argument("eta continuation contour crosses evaluated singular point " +
                                    singular_point.expression + " = " +
                                    singular_point.value.ToString());
      }
    }

    singular_point.branch_winding =
        marked_target_endpoint
            ? 0
            : static_cast<int>(ComputeBranchWinding(plan.contour_points,
                                                    singular_point.value));
    if (marked_target_endpoint) {
      plan.target_endpoint_singular->branch_winding = singular_point.branch_winding;
    }
  }

  plan.contour_fingerprint =
      ComputeArtifactFingerprint(SerializeEtaContinuationPlanForFingerprint(plan));
  return plan;
}

std::string MakePointExpression(const std::string& eta_symbol,
                                const ExactComplexRational& value) {
  return eta_symbol + "=" + value.ToString();
}

}  // namespace

std::string ToString(const EtaContourHalfPlane half_plane) {
  switch (half_plane) {
    case EtaContourHalfPlane::Upper:
      return "upper";
    case EtaContourHalfPlane::Lower:
      return "lower";
  }
  throw std::invalid_argument("unknown eta contour half-plane");
}

EtaContinuationPlan FinalizeEtaContinuationContour(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::vector<std::string>& contour_point_expressions,
    const EtaContourHalfPlane half_plane) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }
  if (contour_point_expressions.size() < 2) {
    throw std::invalid_argument(
        "eta continuation contour requires at least two contour-point expressions");
  }
  ValidateEtaContinuationSymbol(system, eta_symbol);

  return FinalizeEtaContinuationContourImpl(system,
                                            spec,
                                            eta_symbol,
                                            contour_point_expressions.front(),
                                            contour_point_expressions.back(),
                                            half_plane,
                                            std::nullopt,
                                            EvaluateContourPoints(
                                                spec, eta_symbol, contour_point_expressions));
}

EtaContinuationPlan FinalizeEtaContinuationContourWithTargetEndpointSingular(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::vector<std::string>& contour_point_expressions,
    const std::string& target_endpoint_singular_expression,
    const EtaContourHalfPlane half_plane) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }
  if (contour_point_expressions.size() < 2) {
    throw std::invalid_argument(
        "eta continuation contour requires at least two contour-point expressions");
  }
  ValidateEtaContinuationSymbol(system, eta_symbol);

  return FinalizeEtaContinuationContourImpl(system,
                                            spec,
                                            eta_symbol,
                                            contour_point_expressions.front(),
                                            contour_point_expressions.back(),
                                            half_plane,
                                            target_endpoint_singular_expression,
                                            EvaluateContourPoints(
                                                spec, eta_symbol, contour_point_expressions));
}

EtaContinuationPlan PlanEtaContinuationContour(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::string& start_location,
    const std::string& target_location,
    const EtaContourHalfPlane half_plane) {
  return PlanEtaContinuationContourWithTargetEndpointSingular(system,
                                                             spec,
                                                             eta_symbol,
                                                             start_location,
                                                             target_location,
                                                             "",
                                                             half_plane);
}

EtaContinuationPlan PlanEtaContinuationContourWithTargetEndpointSingular(
    const DESystem& system,
    const ProblemSpec& spec,
    const std::string& eta_symbol,
    const std::string& start_location,
    const std::string& target_location,
    const std::string& target_endpoint_singular_expression,
    const EtaContourHalfPlane half_plane) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }
  ValidateEtaContinuationSymbol(system, eta_symbol);

  const NumericEvaluationPoint evaluation_point = BuildComplexNumericEvaluationPoint(spec);
  const ExactComplexRational start =
      EvaluateComplexPointExpression(eta_symbol, start_location, evaluation_point);
  const ExactComplexRational target =
      EvaluateComplexPointExpression(eta_symbol, target_location, evaluation_point);
  if (start == target) {
    throw std::invalid_argument(
        "eta continuation contour requires distinct start and target locations");
  }
  if (start.imaginary != target.imaginary) {
    throw std::invalid_argument(
        "eta continuation contour currently supports only finite start and target points on one "
        "horizontal line");
  }

  const std::vector<EtaContourSingularPoint> singular_points =
      EvaluateEtaSingularPoints(system, spec, eta_symbol);
  std::vector<EtaContourSingularPoint> on_path_singular_points;
  for (const EtaContourSingularPoint& singular_point : singular_points) {
    if (singular_point.value.imaginary == start.imaginary &&
        IsStrictlyBetween(singular_point.value.real, start.real, target.real)) {
      on_path_singular_points.push_back(singular_point);
    }
  }

  const bool decreasing_real = CompareRational(target.real, start.real) < 0;
  std::sort(on_path_singular_points.begin(),
            on_path_singular_points.end(),
            [decreasing_real](const EtaContourSingularPoint& lhs,
                              const EtaContourSingularPoint& rhs) {
              const int comparison = CompareRational(lhs.value.real, rhs.value.real);
              return decreasing_real ? comparison > 0 : comparison < 0;
            });

  std::vector<std::string> contour_point_expressions = {start_location};
  if (!on_path_singular_points.empty()) {
    std::vector<ExactRational> sorted_real_positions = {start.real, target.real};
    for (const EtaContourSingularPoint& singular_point : on_path_singular_points) {
      sorted_real_positions.push_back(singular_point.value.real);
    }
    std::sort(sorted_real_positions.begin(),
              sorted_real_positions.end(),
              [](const ExactRational& lhs, const ExactRational& rhs) {
                return CompareRational(lhs, rhs) < 0;
              });

    ExactRational minimum_gap = SubtractRational(sorted_real_positions[1], sorted_real_positions[0]);
    for (std::size_t index = 2; index < sorted_real_positions.size(); ++index) {
      const ExactRational gap =
          SubtractRational(sorted_real_positions[index], sorted_real_positions[index - 1]);
      if (CompareRational(gap, minimum_gap) < 0) {
        minimum_gap = gap;
      }
    }
    if (CompareRational(minimum_gap, ZeroRational()) <= 0) {
      throw std::invalid_argument(
          "eta continuation contour requires distinct singular-point locations along the reviewed "
          "horizontal path");
    }

    const ExactRational clearance = DivideRational(minimum_gap, {"4", "1"});
    const ExactRational detour_imaginary =
        half_plane == EtaContourHalfPlane::Upper
            ? AddRational(start.imaginary, clearance)
            : SubtractRational(start.imaginary, clearance);
    for (const EtaContourSingularPoint& singular_point : on_path_singular_points) {
      const ExactRational before_real =
          decreasing_real ? AddRational(singular_point.value.real, clearance)
                          : SubtractRational(singular_point.value.real, clearance);
      const ExactRational after_real =
          decreasing_real ? SubtractRational(singular_point.value.real, clearance)
                          : AddRational(singular_point.value.real, clearance);
      contour_point_expressions.push_back(MakePointExpression(
          eta_symbol, MakeComplexRational(before_real, start.imaginary)));
      contour_point_expressions.push_back(MakePointExpression(
          eta_symbol, MakeComplexRational(singular_point.value.real, detour_imaginary)));
      contour_point_expressions.push_back(MakePointExpression(
          eta_symbol, MakeComplexRational(after_real, start.imaginary)));
    }
  }
  contour_point_expressions.push_back(target_location);

  const std::optional<std::string> endpoint_singular_expression =
      target_endpoint_singular_expression.empty()
          ? std::nullopt
          : std::optional<std::string>{target_endpoint_singular_expression};

  return FinalizeEtaContinuationContourImpl(system,
                                            spec,
                                            eta_symbol,
                                            start_location,
                                            target_location,
                                            half_plane,
                                            endpoint_singular_expression,
                                            EvaluateContourPoints(
                                                spec, eta_symbol, contour_point_expressions));
}

}  // namespace amflow
