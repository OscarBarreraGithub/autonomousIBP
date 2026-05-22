#include "amflow/runtime/complex_contour_propagator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

ComplexContourFloat ComplexAbs(const ComplexContourNumber& value) {
  return sqrt(value.real() * value.real() + value.imag() * value.imag());
}

std::string IntegratorLabel(const ComplexContourIntegrator integrator) {
  switch (integrator) {
    case ComplexContourIntegrator::DormandPrinceRk45:
      return "dormand-prince-rk45-adaptive";
    case ComplexContourIntegrator::FehlbergRk78:
      return "fehlberg-rk78-adaptive";
  }
  return "unknown-adaptive";
}

bool IsFinite(const ComplexContourNumber& value) {
  return std::isfinite(static_cast<long double>(value.real())) &&
         std::isfinite(static_cast<long double>(value.imag()));
}

bool IsFiniteFloat(const ComplexContourFloat& value) {
  return std::isfinite(static_cast<long double>(value));
}

void RequireFiniteVector(const ComplexContourVector& values,
                         const std::string& context) {
  for (const ComplexContourNumber& value : values) {
    if (!IsFinite(value)) {
      throw std::runtime_error("nonfinite-contour-state-" + context);
    }
  }
}

std::string CompactFloat(const ComplexContourFloat& value,
                         const int precision = 70) {
  std::ostringstream out;
  out << std::setprecision(precision) << value;
  return out.str();
}

std::string CompactComplex(const ComplexContourNumber& value,
                           const int precision = 70) {
  std::ostringstream out;
  out << CompactFloat(value.real(), precision);
  if (value.imag() < 0) {
    out << " - " << CompactFloat(-value.imag(), precision) << "*I";
  } else {
    out << " + " << CompactFloat(value.imag(), precision) << "*I";
  }
  return out.str();
}

ComplexContourVector MatrixVectorProduct(const ComplexContourMatrix& matrix,
                                         const ComplexContourVector& vector) {
  if (matrix.size() != vector.size()) {
    throw std::runtime_error("matrix-dimension-mismatch");
  }
  ComplexContourVector product(vector.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    if (matrix[row].size() != vector.size()) {
      throw std::runtime_error("matrix-dimension-mismatch");
    }
    for (std::size_t column = 0; column < vector.size(); ++column) {
      product[row] += matrix[row][column] * vector[column];
    }
  }
  return product;
}

ComplexContourVector EvaluateContourDerivative(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourNumber& eta,
    const ComplexContourVector& state,
    const ComplexContourNumber& segment_velocity) {
  ComplexContourVector derivative =
      MatrixVectorProduct(matrix_evaluator(eta), state);
  for (ComplexContourNumber& entry : derivative) {
    entry *= segment_velocity;
  }
  return derivative;
}

ComplexContourFloat MaxVectorDifference(const ComplexContourVector& lhs,
                                        const ComplexContourVector& rhs);

ComplexContourFloat MaxVectorNorm(const ComplexContourVector& values);

ComplexContourFloat EffectiveRefinementTolerance(
    const ComplexContourPropagationOptions& options,
    const ComplexContourFloat& endpoint_vector_norm);

struct AdaptiveRk45Stats {
  std::size_t accepted_steps = 0;
  std::size_t rejected_steps = 0;
  std::size_t pole_pinched_steps = 0;
  ComplexContourFloat max_embedded_error_abs = 0;
};

struct AdaptiveRk45Result {
  ComplexContourVector values;
  AdaptiveRk45Stats stats;
};

struct EmbeddedStepEstimate {
  ComplexContourVector high_order;
  ComplexContourFloat embedded_error_abs = 0;
};

std::string AdaptiveRk45FailureMessage(const std::string& reason,
                                       const AdaptiveRk45Stats& stats) {
  std::ostringstream out;
  out << reason << "; accepted_steps=" << stats.accepted_steps
      << "; rejected_steps=" << stats.rejected_steps
      << "; pole_pinch_step_count=" << stats.pole_pinched_steps
      << "; max_embedded_error_abs="
      << CompactFloat(stats.max_embedded_error_abs, 40);
  return out.str();
}

std::string ExtractSemicolonDiagnosticValue(const std::string& message,
                                            const std::string& key) {
  const std::string needle = key + "=";
  const std::size_t start = message.find(needle);
  if (start == std::string::npos) {
    return {};
  }
  const std::size_t value_start = start + needle.size();
  const std::size_t value_end = message.find(';', value_start);
  return message.substr(value_start,
                        value_end == std::string::npos
                            ? std::string::npos
                            : value_end - value_start);
}

std::size_t ExtractSizeDiagnosticValue(const std::string& message,
                                       const std::string& key) {
  const std::string value = ExtractSemicolonDiagnosticValue(message, key);
  if (value.empty()) {
    return 0;
  }
  try {
    return static_cast<std::size_t>(std::stoull(value));
  } catch (const std::exception&) {
    return 0;
  }
}

ComplexContourVector AddScaledSum(
    const ComplexContourVector& base,
    const std::vector<const ComplexContourVector*>& directions,
    const std::vector<ComplexContourFloat>& coefficients,
    const ComplexContourFloat& step_size) {
  if (directions.size() != coefficients.size()) {
    throw std::runtime_error("complex contour propagator coefficient size mismatch");
  }
  ComplexContourVector result = base;
  for (std::size_t direction_index = 0; direction_index < directions.size();
       ++direction_index) {
    const ComplexContourVector& direction = *directions[direction_index];
    if (direction.size() != base.size()) {
      throw std::runtime_error("complex contour propagator vector size mismatch");
    }
    const ComplexContourFloat scale = coefficients[direction_index] * step_size;
    for (std::size_t index = 0; index < result.size(); ++index) {
      result[index] += direction[index] * scale;
    }
  }
  return result;
}

EmbeddedStepEstimate DormandPrinceRk45Step(
    const ComplexContourVector& state,
    const ComplexContourNumber& eta_start,
    const ComplexContourNumber& segment,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourFloat& t,
    const ComplexContourFloat& h) {
  const ComplexContourFloat c2 = ComplexContourFloat(1) / ComplexContourFloat(5);
  const ComplexContourFloat c3 =
      ComplexContourFloat(3) / ComplexContourFloat(10);
  const ComplexContourFloat c4 =
      ComplexContourFloat(4) / ComplexContourFloat(5);
  const ComplexContourFloat c5 =
      ComplexContourFloat(8) / ComplexContourFloat(9);

  const ComplexContourVector k1 = EvaluateContourDerivative(
      matrix_evaluator, eta_start + segment * t, state, segment);
  const ComplexContourFloat t2 = t + h * c2;
  const ComplexContourVector k2 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t2,
                                AddScaledSum(state,
                                             {&k1},
                                             {ComplexContourFloat(1) /
                                              ComplexContourFloat(5)},
                                             h),
                                segment);
  const ComplexContourFloat t3 = t + h * c3;
  const ComplexContourVector k3 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t3,
                                AddScaledSum(state,
                                             {&k1, &k2},
                                             {ComplexContourFloat(3) /
                                                  ComplexContourFloat(40),
                                              ComplexContourFloat(9) /
                                                  ComplexContourFloat(40)},
                                             h),
                                segment);
  const ComplexContourFloat t4 = t + h * c4;
  const ComplexContourVector k4 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t4,
                                AddScaledSum(state,
                                             {&k1, &k2, &k3},
                                             {ComplexContourFloat(44) /
                                                  ComplexContourFloat(45),
                                              -ComplexContourFloat(56) /
                                                  ComplexContourFloat(15),
                                              ComplexContourFloat(32) /
                                                  ComplexContourFloat(9)},
                                             h),
                                segment);
  const ComplexContourFloat t5 = t + h * c5;
  const ComplexContourVector k5 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t5,
                                AddScaledSum(state,
                                             {&k1, &k2, &k3, &k4},
                                             {ComplexContourFloat(19372) /
                                                  ComplexContourFloat(6561),
                                              -ComplexContourFloat(25360) /
                                                  ComplexContourFloat(2187),
                                              ComplexContourFloat(64448) /
                                                  ComplexContourFloat(6561),
                                              -ComplexContourFloat(212) /
                                                  ComplexContourFloat(729)},
                                             h),
                                segment);
  const ComplexContourFloat t6 = t + h;
  const ComplexContourVector k6 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t6,
                                AddScaledSum(state,
                                             {&k1, &k2, &k3, &k4, &k5},
                                             {ComplexContourFloat(9017) /
                                                  ComplexContourFloat(3168),
                                              -ComplexContourFloat(355) /
                                                  ComplexContourFloat(33),
                                              ComplexContourFloat(46732) /
                                                  ComplexContourFloat(5247),
                                              ComplexContourFloat(49) /
                                                  ComplexContourFloat(176),
                                              -ComplexContourFloat(5103) /
                                                  ComplexContourFloat(18656)},
                                             h),
                                segment);
  ComplexContourVector fifth_order =
      AddScaledSum(state,
                   {&k1, &k3, &k4, &k5, &k6},
                   {ComplexContourFloat(35) / ComplexContourFloat(384),
                    ComplexContourFloat(500) / ComplexContourFloat(1113),
                    ComplexContourFloat(125) / ComplexContourFloat(192),
                    -ComplexContourFloat(2187) / ComplexContourFloat(6784),
                    ComplexContourFloat(11) / ComplexContourFloat(84)},
                   h);
  const ComplexContourVector k7 = EvaluateContourDerivative(
      matrix_evaluator, eta_start + segment * t6, fifth_order, segment);
  const ComplexContourVector fourth_order =
      AddScaledSum(state,
                   {&k1, &k3, &k4, &k5, &k6, &k7},
                   {ComplexContourFloat(5179) / ComplexContourFloat(57600),
                    ComplexContourFloat(7571) / ComplexContourFloat(16695),
                    ComplexContourFloat(393) / ComplexContourFloat(640),
                    -ComplexContourFloat(92097) / ComplexContourFloat(339200),
                    ComplexContourFloat(187) / ComplexContourFloat(2100),
                    ComplexContourFloat(1) / ComplexContourFloat(40)},
                   h);

  EmbeddedStepEstimate estimate;
  estimate.embedded_error_abs =
      MaxVectorDifference(fifth_order, fourth_order);
  estimate.high_order = std::move(fifth_order);
  return estimate;
}

EmbeddedStepEstimate FehlbergRk78Step(
    const ComplexContourVector& state,
    const ComplexContourNumber& eta_start,
    const ComplexContourNumber& segment,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourFloat& t,
    const ComplexContourFloat& h) {
  const ComplexContourVector k1 = EvaluateContourDerivative(
      matrix_evaluator, eta_start + segment * t, state, segment);

  const ComplexContourFloat t2 =
      t + h * (ComplexContourFloat(2) / ComplexContourFloat(27));
  const ComplexContourVector k2 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t2,
                                AddScaledSum(state,
                                             {&k1},
                                             {ComplexContourFloat(2) /
                                              ComplexContourFloat(27)},
                                             h),
                                segment);

  const ComplexContourFloat t3 =
      t + h * (ComplexContourFloat(1) / ComplexContourFloat(9));
  const ComplexContourVector k3 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t3,
                                AddScaledSum(state,
                                             {&k1, &k2},
                                             {ComplexContourFloat(1) /
                                                  ComplexContourFloat(36),
                                              ComplexContourFloat(1) /
                                                  ComplexContourFloat(12)},
                                             h),
                                segment);

  const ComplexContourFloat t4 =
      t + h * (ComplexContourFloat(1) / ComplexContourFloat(6));
  const ComplexContourVector k4 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t4,
                                AddScaledSum(state,
                                             {&k1, &k3},
                                             {ComplexContourFloat(1) /
                                                  ComplexContourFloat(24),
                                              ComplexContourFloat(1) /
                                                  ComplexContourFloat(8)},
                                             h),
                                segment);

  const ComplexContourFloat t5 =
      t + h * (ComplexContourFloat(5) / ComplexContourFloat(12));
  const ComplexContourVector k5 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t5,
                                AddScaledSum(state,
                                             {&k1, &k3, &k4},
                                             {ComplexContourFloat(5) /
                                                  ComplexContourFloat(12),
                                              -ComplexContourFloat(25) /
                                                  ComplexContourFloat(16),
                                              ComplexContourFloat(25) /
                                                  ComplexContourFloat(16)},
                                             h),
                                segment);

  const ComplexContourFloat t6 =
      t + h * (ComplexContourFloat(1) / ComplexContourFloat(2));
  const ComplexContourVector k6 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t6,
                                AddScaledSum(state,
                                             {&k1, &k4, &k5},
                                             {ComplexContourFloat(1) /
                                                  ComplexContourFloat(20),
                                              ComplexContourFloat(1) /
                                                  ComplexContourFloat(4),
                                              ComplexContourFloat(1) /
                                                  ComplexContourFloat(5)},
                                             h),
                                segment);

  const ComplexContourFloat t7 =
      t + h * (ComplexContourFloat(5) / ComplexContourFloat(6));
  const ComplexContourVector k7 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t7,
                                AddScaledSum(state,
                                             {&k1, &k4, &k5, &k6},
                                             {-ComplexContourFloat(25) /
                                                  ComplexContourFloat(108),
                                              ComplexContourFloat(125) /
                                                  ComplexContourFloat(108),
                                              -ComplexContourFloat(65) /
                                                  ComplexContourFloat(27),
                                              ComplexContourFloat(125) /
                                                  ComplexContourFloat(54)},
                                             h),
                                segment);

  const ComplexContourFloat t8 =
      t + h * (ComplexContourFloat(1) / ComplexContourFloat(6));
  const ComplexContourVector k8 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t8,
                                AddScaledSum(state,
                                             {&k1, &k5, &k6, &k7},
                                             {ComplexContourFloat(31) /
                                                  ComplexContourFloat(300),
                                              ComplexContourFloat(61) /
                                                  ComplexContourFloat(225),
                                              -ComplexContourFloat(2) /
                                                  ComplexContourFloat(9),
                                              ComplexContourFloat(13) /
                                                  ComplexContourFloat(900)},
                                             h),
                                segment);

  const ComplexContourFloat t9 =
      t + h * (ComplexContourFloat(2) / ComplexContourFloat(3));
  const ComplexContourVector k9 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t9,
                                AddScaledSum(state,
                                             {&k1, &k4, &k5, &k6, &k7, &k8},
                                             {ComplexContourFloat(2),
                                              -ComplexContourFloat(53) /
                                                  ComplexContourFloat(6),
                                              ComplexContourFloat(704) /
                                                  ComplexContourFloat(45),
                                              -ComplexContourFloat(107) /
                                                  ComplexContourFloat(9),
                                              ComplexContourFloat(67) /
                                                  ComplexContourFloat(90),
                                              ComplexContourFloat(3)},
                                             h),
                                segment);

  const ComplexContourFloat t10 =
      t + h * (ComplexContourFloat(1) / ComplexContourFloat(3));
  const ComplexContourVector k10 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t10,
                                AddScaledSum(state,
                                             {&k1, &k4, &k5, &k6, &k7, &k8,
                                              &k9},
                                             {-ComplexContourFloat(91) /
                                                  ComplexContourFloat(108),
                                              ComplexContourFloat(23) /
                                                  ComplexContourFloat(108),
                                              -ComplexContourFloat(976) /
                                                  ComplexContourFloat(135),
                                              ComplexContourFloat(311) /
                                                  ComplexContourFloat(54),
                                              -ComplexContourFloat(19) /
                                                  ComplexContourFloat(60),
                                              ComplexContourFloat(17) /
                                                  ComplexContourFloat(6),
                                              -ComplexContourFloat(1) /
                                                  ComplexContourFloat(12)},
                                             h),
                                segment);

  const ComplexContourFloat t11 = t + h;
  const ComplexContourVector k11 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t11,
                                AddScaledSum(state,
                                             {&k1, &k4, &k5, &k6, &k7, &k8,
                                              &k9, &k10},
                                             {ComplexContourFloat(2383) /
                                                  ComplexContourFloat(4100),
                                              -ComplexContourFloat(341) /
                                                  ComplexContourFloat(164),
                                              ComplexContourFloat(4496) /
                                                  ComplexContourFloat(1025),
                                              -ComplexContourFloat(301) /
                                                  ComplexContourFloat(82),
                                              ComplexContourFloat(2133) /
                                                  ComplexContourFloat(4100),
                                              ComplexContourFloat(45) /
                                                  ComplexContourFloat(82),
                                              ComplexContourFloat(45) /
                                                  ComplexContourFloat(164),
                                              ComplexContourFloat(18) /
                                                  ComplexContourFloat(41)},
                                             h),
                                segment);

  const ComplexContourFloat t12 = t;
  const ComplexContourVector k12 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t12,
                                AddScaledSum(state,
                                             {&k1, &k6, &k7, &k8, &k9, &k10},
                                             {ComplexContourFloat(3) /
                                                  ComplexContourFloat(205),
                                              -ComplexContourFloat(6) /
                                                  ComplexContourFloat(41),
                                              -ComplexContourFloat(3) /
                                                  ComplexContourFloat(205),
                                              -ComplexContourFloat(3) /
                                                  ComplexContourFloat(41),
                                              ComplexContourFloat(3) /
                                                  ComplexContourFloat(41),
                                              ComplexContourFloat(6) /
                                                  ComplexContourFloat(41)},
                                             h),
                                segment);

  const ComplexContourVector k13 =
      EvaluateContourDerivative(matrix_evaluator,
                                eta_start + segment * t11,
                                AddScaledSum(state,
                                             {&k1, &k4, &k5, &k6, &k7, &k8,
                                              &k9, &k10, &k12},
                                             {-ComplexContourFloat(1777) /
                                                  ComplexContourFloat(4100),
                                              -ComplexContourFloat(341) /
                                                  ComplexContourFloat(164),
                                              ComplexContourFloat(4496) /
                                                  ComplexContourFloat(1025),
                                              -ComplexContourFloat(289) /
                                                  ComplexContourFloat(82),
                                              ComplexContourFloat(2193) /
                                                  ComplexContourFloat(4100),
                                              ComplexContourFloat(51) /
                                                  ComplexContourFloat(82),
                                              ComplexContourFloat(33) /
                                                  ComplexContourFloat(164),
                                              ComplexContourFloat(12) /
                                                  ComplexContourFloat(41),
                                              ComplexContourFloat(1)},
                                             h),
                                segment);

  ComplexContourVector primary_order =
      AddScaledSum(state,
                   {&k1, &k6, &k7, &k8, &k9, &k10, &k11},
                   {ComplexContourFloat(41) / ComplexContourFloat(840),
                    ComplexContourFloat(34) / ComplexContourFloat(105),
                    ComplexContourFloat(9) / ComplexContourFloat(35),
                    ComplexContourFloat(9) / ComplexContourFloat(35),
                    ComplexContourFloat(9) / ComplexContourFloat(280),
                    ComplexContourFloat(9) / ComplexContourFloat(280),
                    ComplexContourFloat(41) / ComplexContourFloat(840)},
                   h);
  const ComplexContourVector embedded_order =
      AddScaledSum(state,
                   {&k6, &k7, &k8, &k9, &k10, &k12, &k13},
                   {ComplexContourFloat(34) / ComplexContourFloat(105),
                    ComplexContourFloat(9) / ComplexContourFloat(35),
                    ComplexContourFloat(9) / ComplexContourFloat(35),
                    ComplexContourFloat(9) / ComplexContourFloat(280),
                    ComplexContourFloat(9) / ComplexContourFloat(280),
                    ComplexContourFloat(41) / ComplexContourFloat(840),
                    ComplexContourFloat(41) / ComplexContourFloat(840)},
                   h);

  EmbeddedStepEstimate estimate;
  estimate.embedded_error_abs =
      MaxVectorDifference(primary_order, embedded_order);
  estimate.high_order = std::move(primary_order);
  return estimate;
}

EmbeddedStepEstimate EmbeddedRungeKuttaStep(
    const ComplexContourVector& state,
    const ComplexContourNumber& eta_start,
    const ComplexContourNumber& segment,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourFloat& t,
    const ComplexContourFloat& h,
    const ComplexContourIntegrator integrator) {
  switch (integrator) {
    case ComplexContourIntegrator::DormandPrinceRk45:
      return DormandPrinceRk45Step(
          state, eta_start, segment, matrix_evaluator, t, h);
    case ComplexContourIntegrator::FehlbergRk78:
      return FehlbergRk78Step(
          state, eta_start, segment, matrix_evaluator, t, h);
  }
  throw std::runtime_error("unsupported-complex-contour-integrator");
}

ComplexContourFloat AdaptiveStepTolerance(
    const ComplexContourPropagationOptions& options,
    const ComplexContourVector& state) {
  const ComplexContourFloat state_norm =
      std::max(ComplexContourFloat(1), MaxVectorNorm(state));
  return EffectiveRefinementTolerance(options, state_norm) /
         ComplexContourFloat(4);
}

ComplexContourFloat AdaptiveStepFactor(const ComplexContourFloat& error,
                                       const ComplexContourFloat& tolerance,
                                       const bool accepted_step) {
  if (error <= 0 || tolerance <= 0) {
    return accepted_step ? ComplexContourFloat(2) : ComplexContourFloat("0.5");
  }
  const ComplexContourFloat ratio = tolerance / error;
  if (accepted_step) {
    if (ratio >= ComplexContourFloat(1024)) {
      return ComplexContourFloat(2);
    }
    if (ratio >= ComplexContourFloat(32)) {
      return ComplexContourFloat("1.5");
    }
    if (ratio >= ComplexContourFloat(2)) {
      return ComplexContourFloat("1.125");
    }
    return ComplexContourFloat(1);
  }
  if (ratio <= ComplexContourFloat("1e-8")) {
    return ComplexContourFloat("0.1");
  }
  if (ratio <= ComplexContourFloat("1e-4")) {
    return ComplexContourFloat("0.25");
  }
  return ComplexContourFloat("0.5");
}

ComplexContourFloat ApplyPoleStepLimit(
    const ComplexContourFloat& t,
    const ComplexContourFloat& requested_h,
    const ComplexContourNumber& eta_start,
    const ComplexContourNumber& eta_end,
    const ComplexContourNumber& segment,
    const ComplexContourPropagationOptions& options,
    bool& pinched) {
  pinched = false;
  if (options.contour_poles.empty()) {
    return requested_h;
  }
  const ComplexContourFloat segment_abs = ComplexAbs(segment);
  if (segment_abs == 0) {
    return requested_h;
  }
  const ComplexContourNumber eta = eta_start + segment * t;
  ComplexContourFloat nearest_distance =
      std::numeric_limits<ComplexContourFloat>::infinity();
  const ComplexContourFloat scaled_endpoint_exclusion =
      segment_abs * ComplexContourFloat("1e-60");
  const ComplexContourFloat endpoint_exclusion =
      std::max(ComplexContourFloat("1e-70"),
               scaled_endpoint_exclusion);
  for (const ComplexContourNumber& pole : options.contour_poles) {
    if (ComplexAbs(pole - eta_end) <= endpoint_exclusion) {
      continue;
    }
    nearest_distance = std::min(nearest_distance, ComplexAbs(eta - pole));
  }
  if (!IsFiniteFloat(nearest_distance)) {
    return requested_h;
  }
  const ComplexContourFloat pinched_h =
      options.pole_step_safety_factor * nearest_distance / segment_abs;
  if (pinched_h > 0 && pinched_h < requested_h) {
    pinched = true;
    return pinched_h;
  }
  return requested_h;
}

AdaptiveRk45Result PropagateSegmentWithAdaptiveRk45(
    const ComplexContourVector& initial_values,
    const ComplexContourNumber& eta_start,
    const ComplexContourNumber& eta_end,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options,
    const std::size_t initial_steps) {
  if (initial_steps == 0) {
    throw std::runtime_error("invalid-step-count");
  }
  const ComplexContourNumber segment = eta_end - eta_start;
  if (ComplexAbs(segment) == 0) {
    throw std::runtime_error("zero-length-contour-segment");
  }

  AdaptiveRk45Result result;
  result.values = initial_values;
  ComplexContourFloat t = 0;
  ComplexContourFloat h =
      ComplexContourFloat(1) / ComplexContourFloat(initial_steps);
  const ComplexContourFloat h_max = h;
  const ComplexContourFloat h_min =
      ComplexContourFloat(1) /
      (ComplexContourFloat(options.max_adaptive_steps_per_segment) *
       ComplexContourFloat(1024));
  while (t < ComplexContourFloat(1)) {
    if (result.stats.accepted_steps >=
        options.max_adaptive_steps_per_segment) {
      throw std::runtime_error(
          AdaptiveRk45FailureMessage("adaptive-step-limit-exceeded",
                                     result.stats));
    }
    if (result.stats.rejected_steps >
        options.max_adaptive_steps_per_segment * 4) {
      throw std::runtime_error(
          AdaptiveRk45FailureMessage("adaptive-rejection-limit-exceeded",
                                     result.stats));
    }

    const ComplexContourFloat remaining = ComplexContourFloat(1) - t;
    ComplexContourFloat requested_h = std::min(h, remaining);
    bool pole_pinched = false;
    requested_h = ApplyPoleStepLimit(t,
                                     requested_h,
                                     eta_start,
                                     eta_end,
                                     segment,
                                     options,
                                     pole_pinched);
    if (requested_h <= 0) {
      throw std::runtime_error(
          AdaptiveRk45FailureMessage("adaptive-nonpositive-step",
                                     result.stats));
    }
    if (pole_pinched && requested_h < h_min) {
      throw std::runtime_error(
          AdaptiveRk45FailureMessage("pole-pinch-step-underflow",
                                     result.stats));
    }
    requested_h = std::max(requested_h, h_min);
    requested_h = std::min(requested_h, remaining);

    const EmbeddedStepEstimate estimate =
        EmbeddedRungeKuttaStep(result.values,
                               eta_start,
                               segment,
                               matrix_evaluator,
                               t,
                               requested_h,
                               options.integrator);
    const ComplexContourFloat tolerance =
        AdaptiveStepTolerance(options, estimate.high_order);
    result.stats.max_embedded_error_abs = std::max(
        result.stats.max_embedded_error_abs, estimate.embedded_error_abs);
    if (estimate.embedded_error_abs <= tolerance) {
      result.values = estimate.high_order;
      t += requested_h;
      ++result.stats.accepted_steps;
      if (pole_pinched) {
        ++result.stats.pole_pinched_steps;
      }
      const ComplexContourFloat next_h =
          requested_h * AdaptiveStepFactor(estimate.embedded_error_abs,
                                           tolerance,
                                           true);
      h = std::min(h_max, next_h);
    } else {
      if (requested_h <= h_min) {
        throw std::runtime_error(
            AdaptiveRk45FailureMessage("adaptive-step-underflow",
                                       result.stats));
      }
      ++result.stats.rejected_steps;
      const ComplexContourFloat next_h =
          requested_h * AdaptiveStepFactor(estimate.embedded_error_abs,
                                           tolerance,
                                           false);
      h = std::max(h_min, next_h);
    }
  }
  return result;
}

AdaptiveRk45Result PropagateWaypointsWithAdaptiveRk45(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options,
    const std::size_t initial_steps_per_segment) {
  AdaptiveRk45Result result;
  result.values = initial_values;
  for (std::size_t index = 1; index < waypoints.size(); ++index) {
    AdaptiveRk45Result segment_result =
        PropagateSegmentWithAdaptiveRk45(result.values,
                                         waypoints[index - 1],
                                         waypoints[index],
                                         matrix_evaluator,
                                         options,
                                         initial_steps_per_segment);
    result.values = std::move(segment_result.values);
    result.stats.accepted_steps += segment_result.stats.accepted_steps;
    result.stats.rejected_steps += segment_result.stats.rejected_steps;
    result.stats.pole_pinched_steps += segment_result.stats.pole_pinched_steps;
    result.stats.max_embedded_error_abs =
        std::max(result.stats.max_embedded_error_abs,
                 segment_result.stats.max_embedded_error_abs);
  }
  return result;
}

ComplexContourFloat MaxVectorDifference(const ComplexContourVector& lhs,
                                        const ComplexContourVector& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("complex contour propagator vector size mismatch");
  }
  ComplexContourFloat difference = 0;
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    difference = std::max(difference, ComplexAbs(lhs[index] - rhs[index]));
  }
  return difference;
}

ComplexContourFloat MaxVectorNorm(const ComplexContourVector& values) {
  ComplexContourFloat norm = 0;
  for (const ComplexContourNumber& value : values) {
    norm = std::max(norm, ComplexAbs(value));
  }
  return norm;
}

ComplexContourFloat EffectiveRefinementTolerance(
    const ComplexContourPropagationOptions& options,
    const ComplexContourFloat& endpoint_vector_norm) {
  const ComplexContourFloat relative_scale =
      std::max(ComplexContourFloat(1), endpoint_vector_norm);
  const ComplexContourFloat relative_tolerance =
      options.refinement_relative_error_tolerance * relative_scale;
  return std::max(options.refinement_error_tolerance, relative_tolerance);
}

std::string SerializePropagationForFingerprint(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options,
    const std::size_t refined_steps_per_segment,
    const bool coefficient_publication,
    const bool endpoint_extraction_applied) {
  std::ostringstream out;
  out << "kind=b61n-complex-contour-propagator\n";
  out << "dimension=" << initial_values.size() << "\n";
  out << "half_plane=" << ToString(options.half_plane) << "\n";
  out << "branch_policy=" << options.branch_policy << "\n";
  out << "matrix_fingerprint=" << options.matrix_fingerprint << "\n";
  out << "endpoint_target=eta=0\n";
  out << "integrator=" << IntegratorLabel(options.integrator) << "\n";
  if (!options.endpoint_integral_id.empty()) {
    out << "endpoint_integral_id=" << options.endpoint_integral_id << "\n";
  }
  out << "endpoint_local_model_kind=" << options.endpoint_local_model_kind << "\n";
  out << "working_precision_digits=" << options.working_precision_digits << "\n";
  out << "refinement_error_tolerance_abs="
      << CompactFloat(options.refinement_error_tolerance) << "\n";
  out << "refinement_error_tolerance_rel="
      << CompactFloat(options.refinement_relative_error_tolerance) << "\n";
  out << "steps_per_segment=" << options.steps_per_segment << "\n";
  out << "refined_steps_per_segment=" << refined_steps_per_segment << "\n";
  out << "max_adaptive_steps_per_segment="
      << options.max_adaptive_steps_per_segment << "\n";
  out << "pole_step_safety_factor="
      << CompactFloat(options.pole_step_safety_factor) << "\n";
  out << "retained_solution_samples_used=false\n";
  out << "coefficient_publication="
      << (coefficient_publication ? "true" : "false") << "\n";
  out << "endpoint_extraction_applied="
      << (endpoint_extraction_applied ? "true" : "false") << "\n";
  out << "full_eta_zero_contour_applied=false\n";
  for (std::size_t index = 0; index < waypoints.size(); ++index) {
    out << "waypoint[" << index << "]=" << CompactComplex(waypoints[index]) << "\n";
  }
  for (std::size_t index = 0; index < options.contour_poles.size(); ++index) {
    out << "contour_pole[" << index
        << "]=" << CompactComplex(options.contour_poles[index]) << "\n";
  }
  for (std::size_t index = 0; index < initial_values.size(); ++index) {
    out << "initial[" << index << "]=" << CompactComplex(initial_values[index]) << "\n";
  }
  return out.str();
}

ComplexContourPropagationResult FailureResult(
    const std::string& failure_code,
    const std::string& summary,
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options) {
  ComplexContourPropagationResult result;
  result.diagnostics.success = false;
  result.diagnostics.dimension = initial_values.size();
  result.diagnostics.waypoint_count = waypoints.size();
  result.diagnostics.segment_count =
      waypoints.empty() ? 0 : waypoints.size() - 1;
  result.diagnostics.half_plane = options.half_plane;
  result.diagnostics.eta_zero_endpoint_reached = false;
  result.diagnostics.working_precision_digits = options.working_precision_digits;
  result.diagnostics.runtime_application =
      "b61n-complex-contour-propagator-harness";
  result.diagnostics.transport_scope =
      "lower-half-plane-complex-ode-vector-propagation";
  result.diagnostics.integrator = IntegratorLabel(options.integrator);
  result.diagnostics.branch_policy = options.branch_policy;
  result.diagnostics.endpoint_target = "eta=0";
  result.diagnostics.endpoint_integral_id = options.endpoint_integral_id;
  result.diagnostics.endpoint_local_model_kind = options.endpoint_local_model_kind;
  result.diagnostics.matrix_fingerprint = options.matrix_fingerprint;
  result.diagnostics.refinement_error_tolerance_abs =
      CompactFloat(options.refinement_error_tolerance, 40);
  result.diagnostics.refinement_error_tolerance_rel =
      CompactFloat(options.refinement_relative_error_tolerance, 40);
  result.diagnostics.failure_code = failure_code;
  result.diagnostics.summary = summary;
  return result;
}

bool IsReviewedLane142PrimitiveBubbleEndpoint(
    const ComplexContourPropagationOptions& options) {
  if (options.endpoint_local_model_kind !=
      "b61n-primitive-bubble-regular-taylor-r0") {
    return false;
  }
  if (options.matrix_fingerprint !=
      "lane142-b61n-selected5-primitive-bubble-v1") {
    return false;
  }
  return options.endpoint_integral_id == "box[1,0,1,0]" ||
         options.endpoint_integral_id == "box[1,0,0,1]" ||
         options.endpoint_integral_id == "box[0,1,0,1]" ||
         options.endpoint_integral_id == "box[0,0,1,1]";
}

bool IsReviewedB61nPublicationContour(
    const std::vector<ComplexContourNumber>& waypoints) {
  if (waypoints.size() < 2) {
    return false;
  }
  for (std::size_t index = 0; index < waypoints.size(); ++index) {
    if (!IsFinite(waypoints[index]) || waypoints[index].real() != 0) {
      return false;
    }
    const ComplexContourFloat imaginary = waypoints[index].imag();
    const bool final_waypoint = index + 1 == waypoints.size();
    if (final_waypoint) {
      if (imaginary != 0) {
        return false;
      }
    } else if (imaginary >= 0) {
      return false;
    }
    if (index > 0 && imaginary <= waypoints[index - 1].imag()) {
      return false;
    }
  }
  return true;
}

bool AppliesReviewedB61nEndpointExtraction(
    const ComplexContourVector& endpoint_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options) {
  if (!IsReviewedB61nPublicationContour(waypoints)) {
    return false;
  }
  if (endpoint_values.size() != 1) {
    return false;
  }
  if (options.endpoint_local_model_kind == "regular-taylor-r0") {
    return true;
  }
  return IsReviewedLane142PrimitiveBubbleEndpoint(options);
}

}  // namespace

ComplexContourPropagationResult PropagateComplexContourVector(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options) {
  if (initial_values.empty()) {
    return FailureResult("empty-initial-vector",
                         "b61n complex contour propagator requires at least one master value",
                         initial_values,
                         waypoints,
                         options);
  }
  if (waypoints.size() < 2) {
    return FailureResult("insufficient-contour-waypoints",
                         "b61n complex contour propagator requires at least two waypoints",
                         initial_values,
                         waypoints,
                         options);
  }
  if (!matrix_evaluator) {
    return FailureResult("missing-matrix-evaluator",
                         "b61n complex contour propagator requires a matrix evaluator",
                         initial_values,
                         waypoints,
                         options);
  }
  if (options.steps_per_segment == 0) {
    return FailureResult("invalid-step-count",
                         "b61n complex contour propagator requires a positive step count",
                         initial_values,
                         waypoints,
                         options);
  }
  if (options.max_adaptive_steps_per_segment == 0) {
    return FailureResult(
        "invalid-adaptive-step-limit",
        "b61n complex contour propagator requires a positive adaptive step limit",
        initial_values,
        waypoints,
        options);
  }
  if (options.max_refinement_doublings < options.refinement_doublings) {
    return FailureResult(
        "invalid-refinement-budget",
        "b61n complex contour propagator requires max_refinement_doublings to cover the "
        "requested refinement_doublings",
        initial_values,
        waypoints,
        options);
  }
  if (!IsFiniteFloat(options.refinement_error_tolerance) ||
      options.refinement_error_tolerance <= 0) {
    return FailureResult(
        "invalid-refinement-tolerance",
        "b61n complex contour propagator requires a positive finite refinement error "
        "tolerance",
        initial_values,
        waypoints,
        options);
  }
  if (!IsFiniteFloat(options.refinement_relative_error_tolerance) ||
      options.refinement_relative_error_tolerance < 0) {
    return FailureResult(
        "invalid-refinement-relative-tolerance",
        "b61n complex contour propagator requires a nonnegative finite relative "
        "refinement error tolerance",
        initial_values,
        waypoints,
        options);
  }
  if (!IsFiniteFloat(options.pole_step_safety_factor) ||
      options.pole_step_safety_factor <= 0) {
    return FailureResult(
        "invalid-pole-step-safety-factor",
        "b61n complex contour propagator requires a positive finite pole step safety "
        "factor",
        initial_values,
        waypoints,
        options);
  }
  if (options.half_plane != EtaContourHalfPlane::Lower) {
    return FailureResult(
        "unsupported-contour-half-plane",
        "b61n complex contour propagator is currently reviewed only for the NegIm "
        "lower-half-plane contour; requested half_plane=" + ToString(options.half_plane),
        initial_values,
        waypoints,
        options);
  }
  for (std::size_t index = 0; index < waypoints.size(); ++index) {
    if (!IsFinite(waypoints[index])) {
      return FailureResult(
          "nonfinite-contour-waypoint",
          "b61n complex contour propagator requires finite contour waypoint coordinates",
          initial_values,
          waypoints,
          options);
    }
    const ComplexContourFloat imaginary = waypoints[index].imag();
    const bool final_waypoint = index + 1 == waypoints.size();
    if ((!final_waypoint && imaginary >= 0) || (final_waypoint && imaginary > 0)) {
      return FailureResult(
          "non-lower-half-plane-waypoint",
          "b61n complex contour propagator requires every non-final waypoint to stay "
          "strictly below the real axis for the reviewed NegIm branch",
          initial_values,
          waypoints,
          options);
    }
  }
  for (const ComplexContourNumber& pole : options.contour_poles) {
    if (!IsFinite(pole)) {
      return FailureResult(
          "nonfinite-contour-pole",
          "b61n complex contour propagator requires finite contour pole coordinates",
          initial_values,
          waypoints,
          options);
    }
  }
  if (options.matrix_fingerprint.empty()) {
    return FailureResult(
        "missing-matrix-fingerprint",
        "b61n complex contour propagator requires caller-supplied matrix provenance before "
        "publishing propagated samples",
        initial_values,
        waypoints,
        options);
  }
  if (options.endpoint_local_model_kind.empty()) {
    return FailureResult(
        "missing-endpoint-local-model-kind",
        "b61n complex contour propagator requires eta=0 endpoint local-model provenance "
        "before publishing propagated samples",
        initial_values,
        waypoints,
        options);
  }
  if (waypoints.back().real() != 0 || waypoints.back().imag() != 0) {
    return FailureResult(
        "non-eta-zero-contour-endpoint",
        "b61n complex contour propagator requires the final waypoint to be exactly eta=0; "
        "got " + CompactComplex(waypoints.back(), 40),
        initial_values,
        waypoints,
        options);
  }

  try {
    RequireFiniteVector(initial_values, "initial");
    AdaptiveRk45Result coarse =
        PropagateWaypointsWithAdaptiveRk45(initial_values,
                                           waypoints,
                                           matrix_evaluator,
                                           options,
                                           options.steps_per_segment);
    RequireFiniteVector(coarse.values, "coarse");

    const std::size_t first_required_refinement =
        std::max<std::size_t>(1, options.refinement_doublings);
    AdaptiveRk45Result previous = coarse;
    AdaptiveRk45Result refined = previous;
    ComplexContourFloat refinement_error =
        std::numeric_limits<ComplexContourFloat>::infinity();
    ComplexContourFloat endpoint_vector_norm = 0;
    ComplexContourFloat effective_refinement_tolerance =
        options.refinement_error_tolerance;
    std::size_t refined_steps_per_segment = options.steps_per_segment;
    std::size_t refinement_doublings_used = 0;
    bool refinement_passed = false;
    for (std::size_t doubling = 1; doubling <= options.max_refinement_doublings;
         ++doubling) {
      refined_steps_per_segment = options.steps_per_segment;
      for (std::size_t level = 0; level < doubling; ++level) {
        refined_steps_per_segment *= 2;
      }
      refined = PropagateWaypointsWithAdaptiveRk45(initial_values,
                                                   waypoints,
                                                   matrix_evaluator,
                                                   options,
                                                   refined_steps_per_segment);
      RequireFiniteVector(refined.values, "refined");
      refinement_error = MaxVectorDifference(previous.values, refined.values);
      endpoint_vector_norm = MaxVectorNorm(refined.values);
      effective_refinement_tolerance =
          EffectiveRefinementTolerance(options, endpoint_vector_norm);
      previous = refined;
      refinement_doublings_used = doubling;
      if (doubling >= first_required_refinement &&
          refinement_error <= effective_refinement_tolerance) {
        refinement_passed = true;
        break;
      }
    }
    if (!refinement_passed) {
      return FailureResult(
          "refinement-tolerance-failed",
          "b61n complex contour propagation failed closed because " +
              IntegratorLabel(options.integrator) + " refinement error " +
              CompactFloat(refinement_error, 40) + " exceeded effective tolerance " +
              CompactFloat(effective_refinement_tolerance, 40) +
              " from abs_floor=" +
              CompactFloat(options.refinement_error_tolerance, 40) +
              ", rel_floor=" +
              CompactFloat(options.refinement_relative_error_tolerance, 40) +
              ", endpoint_vector_norm_abs=" +
              CompactFloat(endpoint_vector_norm, 40),
          initial_values,
          waypoints,
          options);
    }

    ComplexContourPropagationResult result;
    result.success = true;
    result.endpoint_values = refined.values;
    const bool endpoint_extraction_applied =
        AppliesReviewedB61nEndpointExtraction(result.endpoint_values,
                                             waypoints,
                                             options);
    const bool coefficient_publication = endpoint_extraction_applied;
    result.diagnostics.success = true;
    result.diagnostics.ode_propagation_applied = true;
    result.diagnostics.coefficient_publication = coefficient_publication;
    result.diagnostics.retained_solution_samples_used = false;
    result.diagnostics.full_eta_zero_contour_applied = false;
    result.diagnostics.endpoint_extraction_applied = endpoint_extraction_applied;
    result.diagnostics.dimension = initial_values.size();
    result.diagnostics.waypoint_count = waypoints.size();
    result.diagnostics.segment_count = waypoints.size() - 1;
    result.diagnostics.coarse_step_count =
        options.steps_per_segment * result.diagnostics.segment_count;
    result.diagnostics.refined_step_count =
        refined_steps_per_segment * result.diagnostics.segment_count;
    result.diagnostics.refinement_doublings_used = refinement_doublings_used;
    result.diagnostics.adaptive_step_count = refined.stats.accepted_steps;
    result.diagnostics.adaptive_rejected_step_count =
        refined.stats.rejected_steps;
    result.diagnostics.pole_pinch_step_count =
        refined.stats.pole_pinched_steps;
    result.diagnostics.working_precision_digits = options.working_precision_digits;
    result.diagnostics.half_plane = options.half_plane;
    result.diagnostics.eta_zero_endpoint_reached = true;
    result.diagnostics.runtime_application =
        "b61n-complex-contour-propagator-harness";
    result.diagnostics.transport_scope =
        "lower-half-plane-complex-ode-vector-propagation";
    result.diagnostics.integrator = IntegratorLabel(options.integrator);
    result.diagnostics.branch_policy = options.branch_policy;
    result.diagnostics.endpoint_target = "eta=0";
    result.diagnostics.endpoint_integral_id = options.endpoint_integral_id;
    result.diagnostics.endpoint_local_model_kind = options.endpoint_local_model_kind;
    result.diagnostics.matrix_fingerprint = options.matrix_fingerprint;
    result.diagnostics.endpoint_vector_norm_abs =
        CompactFloat(endpoint_vector_norm, 40);
    result.diagnostics.refinement_error_abs = CompactFloat(refinement_error, 40);
    result.diagnostics.refinement_error_tolerance_abs =
        CompactFloat(options.refinement_error_tolerance, 40);
    result.diagnostics.refinement_error_tolerance_rel =
        CompactFloat(options.refinement_relative_error_tolerance, 40);
    result.diagnostics.refinement_effective_tolerance_abs =
        CompactFloat(effective_refinement_tolerance, 40);
    result.diagnostics.max_embedded_error_abs =
        CompactFloat(refined.stats.max_embedded_error_abs, 40);
    result.diagnostics.contour_fingerprint = ComputeArtifactFingerprint(
        SerializePropagationForFingerprint(initial_values,
                                           waypoints,
                                           options,
                                           refined_steps_per_segment,
                                           coefficient_publication,
                                           endpoint_extraction_applied));
    const std::string endpoint_integral_summary =
        result.diagnostics.endpoint_integral_id.empty()
            ? std::string()
            : "endpoint_integral_id=" + result.diagnostics.endpoint_integral_id + "; ";
    result.diagnostics.summary =
        "Propagated a b61n complex ODE vector over " +
        std::to_string(result.diagnostics.segment_count) +
        " lower-half-plane contour segment(s) with dimension " +
        std::to_string(result.diagnostics.dimension) +
        "; ode_propagation_applied=true; coefficient_publication=" +
        (result.diagnostics.coefficient_publication ? std::string("true")
                                                    : std::string("false")) +
        "; "
        "final_solution_samples_used_as_input=false; full_eta_zero_contour_applied=false; "
        "endpoint_target=eta=0; eta_zero_endpoint_reached=true; "
        + endpoint_integral_summary +
        "endpoint_local_model_kind=" + result.diagnostics.endpoint_local_model_kind +
        "; endpoint_extraction_applied=" +
        (result.diagnostics.endpoint_extraction_applied ? std::string("true")
                                                        : std::string("false")) +
        "; "
        "matrix_fingerprint=" + result.diagnostics.matrix_fingerprint +
        "; integrator=" + result.diagnostics.integrator +
        "; working_precision_digits=" +
        std::to_string(result.diagnostics.working_precision_digits) +
        "; refinement_doublings_used=" +
        std::to_string(result.diagnostics.refinement_doublings_used) +
        "; adaptive_step_count=" +
        std::to_string(result.diagnostics.adaptive_step_count) +
        "; adaptive_rejected_step_count=" +
        std::to_string(result.diagnostics.adaptive_rejected_step_count) +
        "; pole_pinch_step_count=" +
        std::to_string(result.diagnostics.pole_pinch_step_count) +
        "; refinement_error_tolerance_abs=" +
        result.diagnostics.refinement_error_tolerance_abs +
        "; refinement_error_tolerance_rel=" +
        result.diagnostics.refinement_error_tolerance_rel +
        "; endpoint_vector_norm_abs=" +
        result.diagnostics.endpoint_vector_norm_abs +
        "; refinement_effective_tolerance_abs=" +
        result.diagnostics.refinement_effective_tolerance_abs +
        "; max_embedded_error_abs=" +
        result.diagnostics.max_embedded_error_abs +
        "; "
        "refinement_error_abs=" + result.diagnostics.refinement_error_abs +
        "; contour_fingerprint=" + result.diagnostics.contour_fingerprint + ".";
    return result;
  } catch (const std::exception& error) {
    const std::string error_message = error.what();
    if (error_message.find("adaptive-step") != std::string::npos ||
        error_message.find("adaptive-rejection") != std::string::npos ||
        error_message.find("pole-pinch-step") != std::string::npos) {
      ComplexContourPropagationResult failure = FailureResult(
          "refinement-tolerance-failed",
          "b61n complex contour propagation failed closed because " +
              IntegratorLabel(options.integrator) +
              " refinement could not satisfy the effective tolerance: " +
              error_message,
          initial_values,
          waypoints,
          options);
      failure.diagnostics.adaptive_step_count =
          ExtractSizeDiagnosticValue(error_message, "accepted_steps");
      failure.diagnostics.adaptive_rejected_step_count =
          ExtractSizeDiagnosticValue(error_message, "rejected_steps");
      failure.diagnostics.pole_pinch_step_count =
          ExtractSizeDiagnosticValue(error_message, "pole_pinch_step_count");
      failure.diagnostics.max_embedded_error_abs =
          ExtractSemicolonDiagnosticValue(error_message,
                                          "max_embedded_error_abs");
      failure.diagnostics.refinement_error_abs =
          "not_available_before_endpoint";
      failure.diagnostics.refinement_effective_tolerance_abs =
          "not_available_before_endpoint";
      return failure;
    }
    return FailureResult("propagation-failed",
                         std::string("b61n complex contour propagation failed closed: ") +
                             error_message,
                         initial_values,
                         waypoints,
                         options);
  }
}

}  // namespace amflow
