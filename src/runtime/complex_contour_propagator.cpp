#include "amflow/runtime/complex_contour_propagator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <boost/math/constants/constants.hpp>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

ComplexContourFloat ComplexAbs(const ComplexContourNumber& value) {
  return sqrt(value.real() * value.real() + value.imag() * value.imag());
}

constexpr std::size_t kScalarFrobeniusRuntimeEndpointOrder = 32;
constexpr std::size_t kScalarFrobeniusRuntimeSampleCount = 160;

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

struct MatrixPivotDiagnostics {
  std::size_t rank = 0;
  ComplexContourFloat min_pivot_abs = 0;
  ComplexContourFloat max_pivot_abs = 0;
  ComplexContourFloat pivot_ratio_abs = 0;
};

struct NearestPoleDiagnostics {
  bool has_pole = false;
  ComplexContourNumber pole;
  ComplexContourFloat distance_abs = 0;
};

struct ContourScaleDiagnostics {
  std::size_t segment_index = 0;
  ComplexContourNumber eta;
  ComplexContourFloat state_norm_abs = 0;
  ComplexContourFloat rhs_norm_abs = 0;
  ComplexContourFloat matrix_max_entry_abs = 0;
  ComplexContourFloat matrix_max_row_l1_abs = 0;
  MatrixPivotDiagnostics matrix_pivots;
  NearestPoleDiagnostics nearest_pole;
};

struct ContourLocationDiagnostics {
  std::size_t segment_index = 0;
  ComplexContourNumber eta;
  ComplexContourVector state;
};

struct AdaptiveRk45Stats {
  std::size_t accepted_steps = 0;
  std::size_t rejected_steps = 0;
  std::size_t pole_pinched_steps = 0;
  ComplexContourFloat max_embedded_error_abs = 0;
  ContourLocationDiagnostics max_embedded_error_location;
};

struct AdaptiveRk45Result {
  ComplexContourVector values;
  std::vector<ComplexContourVector> waypoint_values;
  AdaptiveRk45Stats stats;
};

struct EmbeddedStepEstimate {
  ComplexContourVector high_order;
  ComplexContourFloat embedded_error_abs = 0;
};

struct RefinementPeakDiagnostics {
  bool available = false;
  std::size_t segment_index = 0;
  ComplexContourNumber eta;
  ComplexContourVector state;
  ComplexContourFloat waypoint_error_abs = 0;
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

void RequireMatrixShape(const ComplexContourMatrix& matrix,
                        const std::size_t dimension) {
  if (matrix.size() != dimension) {
    throw std::runtime_error("matrix-dimension-mismatch");
  }
  for (const auto& row : matrix) {
    if (row.size() != dimension) {
      throw std::runtime_error("matrix-dimension-mismatch");
    }
  }
}

ComplexContourFloat MaxMatrixEntryNorm(const ComplexContourMatrix& matrix) {
  ComplexContourFloat norm = 0;
  for (const auto& row : matrix) {
    for (const ComplexContourNumber& value : row) {
      norm = std::max(norm, ComplexAbs(value));
    }
  }
  return norm;
}

ComplexContourFloat MaxMatrixRowL1Norm(const ComplexContourMatrix& matrix) {
  ComplexContourFloat norm = 0;
  for (const auto& row : matrix) {
    ComplexContourFloat row_norm = 0;
    for (const ComplexContourNumber& value : row) {
      row_norm += ComplexAbs(value);
    }
    norm = std::max(norm, row_norm);
  }
  return norm;
}

MatrixPivotDiagnostics ComputeMatrixPivotDiagnostics(
    ComplexContourMatrix matrix) {
  MatrixPivotDiagnostics diagnostics;
  const std::size_t rows = matrix.size();
  if (rows == 0) {
    return diagnostics;
  }
  const std::size_t columns = matrix.front().size();
  if (columns == 0) {
    return diagnostics;
  }
  for (const auto& row : matrix) {
    if (row.size() != columns) {
      throw std::runtime_error("matrix-dimension-mismatch");
    }
  }
  ComplexContourFloat min_pivot =
      std::numeric_limits<ComplexContourFloat>::infinity();
  ComplexContourFloat max_pivot = 0;
  const std::size_t pivot_count = std::min(rows, columns);
  for (std::size_t pivot = 0; pivot < pivot_count; ++pivot) {
    std::size_t best_row = pivot;
    ComplexContourFloat best_abs = ComplexAbs(matrix[pivot][pivot]);
    for (std::size_t row = pivot + 1; row < rows; ++row) {
      const ComplexContourFloat candidate_abs =
          ComplexAbs(matrix[row][pivot]);
      if (candidate_abs > best_abs) {
        best_abs = candidate_abs;
        best_row = row;
      }
    }
    if (best_abs == 0) {
      diagnostics.rank = pivot;
      diagnostics.min_pivot_abs = 0;
      diagnostics.max_pivot_abs = max_pivot;
      diagnostics.pivot_ratio_abs =
          max_pivot == 0 ? ComplexContourFloat(0)
                         : std::numeric_limits<ComplexContourFloat>::infinity();
      return diagnostics;
    }
    if (best_row != pivot) {
      std::swap(matrix[best_row], matrix[pivot]);
    }
    min_pivot = std::min(min_pivot, best_abs);
    max_pivot = std::max(max_pivot, best_abs);
    for (std::size_t row = pivot + 1; row < rows; ++row) {
      const ComplexContourNumber factor = matrix[row][pivot] / matrix[pivot][pivot];
      for (std::size_t column = pivot; column < columns; ++column) {
        matrix[row][column] -= factor * matrix[pivot][column];
      }
    }
  }
  diagnostics.rank = pivot_count;
  diagnostics.min_pivot_abs =
      IsFiniteFloat(min_pivot) ? min_pivot : ComplexContourFloat(0);
  diagnostics.max_pivot_abs = max_pivot;
  diagnostics.pivot_ratio_abs =
      diagnostics.min_pivot_abs == 0
          ? std::numeric_limits<ComplexContourFloat>::infinity()
          : diagnostics.max_pivot_abs / diagnostics.min_pivot_abs;
  return diagnostics;
}

NearestPoleDiagnostics FindNearestPole(
    const ComplexContourNumber& eta,
    const std::vector<ComplexContourNumber>& poles) {
  NearestPoleDiagnostics diagnostics;
  if (poles.empty()) {
    return diagnostics;
  }
  diagnostics.has_pole = true;
  diagnostics.pole = poles.front();
  diagnostics.distance_abs = ComplexAbs(eta - poles.front());
  for (std::size_t index = 1; index < poles.size(); ++index) {
    const ComplexContourFloat distance = ComplexAbs(eta - poles[index]);
    if (distance < diagnostics.distance_abs) {
      diagnostics.distance_abs = distance;
      diagnostics.pole = poles[index];
    }
  }
  return diagnostics;
}

ContourScaleDiagnostics EvaluateContourScaleDiagnostics(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options,
    const std::size_t segment_index,
    const ComplexContourNumber& eta,
    const ComplexContourVector& state) {
  ContourScaleDiagnostics diagnostics;
  diagnostics.segment_index = segment_index;
  diagnostics.eta = eta;
  diagnostics.state_norm_abs = MaxVectorNorm(state);
  const ComplexContourMatrix matrix = matrix_evaluator(eta);
  RequireMatrixShape(matrix, state.size());
  diagnostics.matrix_max_entry_abs = MaxMatrixEntryNorm(matrix);
  diagnostics.matrix_max_row_l1_abs = MaxMatrixRowL1Norm(matrix);
  diagnostics.matrix_pivots = ComputeMatrixPivotDiagnostics(matrix);
  diagnostics.rhs_norm_abs = MaxVectorNorm(MatrixVectorProduct(matrix, state));
  const std::vector<ComplexContourNumber>& poles =
      options.diagnostic_poles.empty() ? options.contour_poles
                                       : options.diagnostic_poles;
  diagnostics.nearest_pole = FindNearestPole(eta, poles);
  return diagnostics;
}

std::string CompactNearestPole(const NearestPoleDiagnostics& diagnostics) {
  if (!diagnostics.has_pole) {
    return "none";
  }
  return CompactComplex(diagnostics.pole, 40);
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
    const std::size_t initial_steps,
    const std::size_t segment_index) {
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
    if (estimate.embedded_error_abs >=
        result.stats.max_embedded_error_abs) {
      result.stats.max_embedded_error_abs = estimate.embedded_error_abs;
      const ComplexContourFloat error_t = t + requested_h;
      const ComplexContourNumber error_eta =
          eta_start + segment * error_t;
      result.stats.max_embedded_error_location.segment_index = segment_index;
      result.stats.max_embedded_error_location.eta = error_eta;
      result.stats.max_embedded_error_location.state = estimate.high_order;
    }
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
  result.waypoint_values.push_back(initial_values);
  for (std::size_t index = 1; index < waypoints.size(); ++index) {
    AdaptiveRk45Result segment_result =
        PropagateSegmentWithAdaptiveRk45(result.values,
                                         waypoints[index - 1],
                                         waypoints[index],
                                         matrix_evaluator,
                                         options,
                                         initial_steps_per_segment,
                                         index - 1);
    result.values = std::move(segment_result.values);
    result.waypoint_values.push_back(result.values);
    result.stats.accepted_steps += segment_result.stats.accepted_steps;
    result.stats.rejected_steps += segment_result.stats.rejected_steps;
    result.stats.pole_pinched_steps += segment_result.stats.pole_pinched_steps;
    if (segment_result.stats.max_embedded_error_abs >=
        result.stats.max_embedded_error_abs) {
      result.stats.max_embedded_error_abs =
          segment_result.stats.max_embedded_error_abs;
      result.stats.max_embedded_error_location =
          segment_result.stats.max_embedded_error_location;
    }
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

RefinementPeakDiagnostics FindRefinementPeakDiagnostics(
    const AdaptiveRk45Result& previous,
    const AdaptiveRk45Result& refined,
    const std::vector<ComplexContourNumber>& waypoints) {
  RefinementPeakDiagnostics diagnostics;
  const std::size_t waypoint_count =
      std::min({previous.waypoint_values.size(),
                refined.waypoint_values.size(),
                waypoints.size()});
  for (std::size_t waypoint_index = 1; waypoint_index < waypoint_count;
       ++waypoint_index) {
    const ComplexContourFloat difference =
        MaxVectorDifference(previous.waypoint_values[waypoint_index],
                            refined.waypoint_values[waypoint_index]);
    if (!diagnostics.available ||
        difference >= diagnostics.waypoint_error_abs) {
      diagnostics.available = true;
      diagnostics.segment_index = waypoint_index - 1;
      diagnostics.eta = waypoints[waypoint_index];
      diagnostics.state = refined.waypoint_values[waypoint_index];
      diagnostics.waypoint_error_abs = difference;
    }
  }
  return diagnostics;
}

void PopulateScaleDiagnostics(
    ComplexContourPropagationDiagnostics& diagnostics,
    const ContourScaleDiagnostics& scale,
    const std::string& prefix) {
  const std::string segment_key = prefix + "_segment_index";
  if (segment_key == "refinement_error_peak_segment_index") {
    diagnostics.refinement_error_peak_segment_index = scale.segment_index;
    diagnostics.refinement_error_peak_eta = CompactComplex(scale.eta, 40);
    diagnostics.refinement_error_peak_state_norm_abs =
        CompactFloat(scale.state_norm_abs, 40);
    diagnostics.refinement_error_peak_rhs_norm_abs =
        CompactFloat(scale.rhs_norm_abs, 40);
    diagnostics.refinement_error_peak_matrix_max_entry_abs =
        CompactFloat(scale.matrix_max_entry_abs, 40);
    diagnostics.refinement_error_peak_matrix_max_row_l1_abs =
        CompactFloat(scale.matrix_max_row_l1_abs, 40);
    diagnostics.refinement_error_peak_matrix_min_lu_pivot_abs =
        CompactFloat(scale.matrix_pivots.min_pivot_abs, 40);
    diagnostics.refinement_error_peak_matrix_pivot_ratio_abs =
        CompactFloat(scale.matrix_pivots.pivot_ratio_abs, 40);
    diagnostics.refinement_error_peak_nearest_pole =
        CompactNearestPole(scale.nearest_pole);
    diagnostics.refinement_error_peak_nearest_pole_distance_abs =
        scale.nearest_pole.has_pole
            ? CompactFloat(scale.nearest_pole.distance_abs, 40)
            : std::string("none");
    return;
  }
  if (segment_key == "max_embedded_error_segment_index") {
    diagnostics.max_embedded_error_segment_index = scale.segment_index;
    diagnostics.max_embedded_error_eta = CompactComplex(scale.eta, 40);
    diagnostics.max_embedded_error_state_norm_abs =
        CompactFloat(scale.state_norm_abs, 40);
    diagnostics.max_embedded_error_rhs_norm_abs =
        CompactFloat(scale.rhs_norm_abs, 40);
    diagnostics.max_embedded_error_matrix_max_entry_abs =
        CompactFloat(scale.matrix_max_entry_abs, 40);
    diagnostics.max_embedded_error_matrix_max_row_l1_abs =
        CompactFloat(scale.matrix_max_row_l1_abs, 40);
    diagnostics.max_embedded_error_matrix_min_lu_pivot_abs =
        CompactFloat(scale.matrix_pivots.min_pivot_abs, 40);
    diagnostics.max_embedded_error_matrix_pivot_ratio_abs =
        CompactFloat(scale.matrix_pivots.pivot_ratio_abs, 40);
    diagnostics.max_embedded_error_nearest_pole =
        CompactNearestPole(scale.nearest_pole);
    diagnostics.max_embedded_error_nearest_pole_distance_abs =
        scale.nearest_pole.has_pole
            ? CompactFloat(scale.nearest_pole.distance_abs, 40)
            : std::string("none");
  }
}

void PopulateRefinementPeakDiagnostics(
    ComplexContourPropagationDiagnostics& diagnostics,
    const RefinementPeakDiagnostics& peak,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options) {
  if (!peak.available || peak.state.empty()) {
    return;
  }
  const ContourScaleDiagnostics scale =
      EvaluateContourScaleDiagnostics(matrix_evaluator,
                                      options,
                                      peak.segment_index,
                                      peak.eta,
                                      peak.state);
  PopulateScaleDiagnostics(diagnostics, scale, "refinement_error_peak");
  diagnostics.refinement_error_peak_waypoint_error_abs =
      CompactFloat(peak.waypoint_error_abs, 40);
}

void PopulateEmbeddedErrorPeakDiagnostics(
    ComplexContourPropagationDiagnostics& diagnostics,
    const AdaptiveRk45Stats& stats,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options) {
  if (stats.max_embedded_error_location.state.empty()) {
    return;
  }
  const ContourScaleDiagnostics scale =
      EvaluateContourScaleDiagnostics(matrix_evaluator,
                                      options,
                                      stats.max_embedded_error_location.segment_index,
                                      stats.max_embedded_error_location.eta,
                                      stats.max_embedded_error_location.state);
  PopulateScaleDiagnostics(diagnostics, scale, "max_embedded_error");
}

std::string ScaleDiagnosticSummary(
    const ComplexContourPropagationDiagnostics& diagnostics) {
  std::ostringstream out;
  if (!diagnostics.refinement_error_peak_eta.empty()) {
    out << "; refinement_error_peak_segment_index="
        << diagnostics.refinement_error_peak_segment_index
        << "; refinement_error_peak_eta="
        << diagnostics.refinement_error_peak_eta
        << "; refinement_error_peak_waypoint_error_abs="
        << diagnostics.refinement_error_peak_waypoint_error_abs
        << "; refinement_error_peak_rhs_norm_abs="
        << diagnostics.refinement_error_peak_rhs_norm_abs
        << "; refinement_error_peak_matrix_max_entry_abs="
        << diagnostics.refinement_error_peak_matrix_max_entry_abs
        << "; refinement_error_peak_matrix_max_row_l1_abs="
        << diagnostics.refinement_error_peak_matrix_max_row_l1_abs
        << "; refinement_error_peak_matrix_min_lu_pivot_abs="
        << diagnostics.refinement_error_peak_matrix_min_lu_pivot_abs
        << "; refinement_error_peak_matrix_pivot_ratio_abs="
        << diagnostics.refinement_error_peak_matrix_pivot_ratio_abs
        << "; refinement_error_peak_nearest_pole="
        << diagnostics.refinement_error_peak_nearest_pole
        << "; refinement_error_peak_nearest_pole_distance_abs="
        << diagnostics.refinement_error_peak_nearest_pole_distance_abs;
  }
  if (!diagnostics.max_embedded_error_eta.empty()) {
    out << "; max_embedded_error_segment_index="
        << diagnostics.max_embedded_error_segment_index
        << "; max_embedded_error_eta="
        << diagnostics.max_embedded_error_eta
        << "; max_embedded_error_rhs_norm_abs="
        << diagnostics.max_embedded_error_rhs_norm_abs
        << "; max_embedded_error_matrix_max_entry_abs="
        << diagnostics.max_embedded_error_matrix_max_entry_abs
        << "; max_embedded_error_matrix_max_row_l1_abs="
        << diagnostics.max_embedded_error_matrix_max_row_l1_abs
        << "; max_embedded_error_matrix_min_lu_pivot_abs="
        << diagnostics.max_embedded_error_matrix_min_lu_pivot_abs
        << "; max_embedded_error_matrix_pivot_ratio_abs="
        << diagnostics.max_embedded_error_matrix_pivot_ratio_abs
        << "; max_embedded_error_nearest_pole="
        << diagnostics.max_embedded_error_nearest_pole
        << "; max_embedded_error_nearest_pole_distance_abs="
        << diagnostics.max_embedded_error_nearest_pole_distance_abs;
  }
  return out.str();
}

std::string SerializePropagationForFingerprint(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options,
    const std::size_t refined_steps_per_segment,
    const bool coefficient_publication,
    const bool endpoint_extraction_applied,
    const bool scalar_frobenius_endpoint_patch_applied) {
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
  out << "scalar_frobenius_endpoint_patch_applied="
      << (scalar_frobenius_endpoint_patch_applied ? "true" : "false")
      << "\n";
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

bool AppliesReviewedScalarFrobeniusRuntimeEndpointPatch(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options) {
  return initial_values.size() == 1 && waypoints.size() > 2 &&
         options.endpoint_local_model_kind == "regular-taylor-r0" &&
         IsReviewedB61nPublicationContour(waypoints);
}

ComplexContourFloat ScalarFrobeniusEndpointPatchRadius(
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options) {
  const ComplexContourNumber endpoint = waypoints.back();
  const ComplexContourNumber match_eta = waypoints[waypoints.size() - 2];
  const ComplexContourFloat match_radius =
      ComplexAbs(match_eta - endpoint) / ComplexContourFloat(4);
  ComplexContourFloat radius =
      std::max(match_radius, ComplexContourFloat("1e-40"));
  for (const ComplexContourNumber& pole : options.contour_poles) {
    const ComplexContourFloat distance = ComplexAbs(pole - endpoint);
    if (distance > 0) {
      const ComplexContourFloat pole_radius =
          distance / ComplexContourFloat(4);
      radius = std::min(radius, pole_radius);
    }
  }
  return std::max(radius, ComplexContourFloat("1e-40"));
}

ComplexContourMatrix MakeIdentityMatrix(const std::size_t dimension) {
  ComplexContourMatrix matrix(
      dimension, std::vector<ComplexContourNumber>(dimension, ComplexContourNumber{}));
  for (std::size_t index = 0; index < dimension; ++index) {
    matrix[index][index] = ComplexContourNumber{1, 0};
  }
  return matrix;
}

ComplexContourMatrix MatrixMatrixProduct(const ComplexContourMatrix& lhs,
                                         const ComplexContourMatrix& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("matrix-dimension-mismatch");
  }
  const std::size_t dimension = lhs.size();
  RequireMatrixShape(lhs, dimension);
  RequireMatrixShape(rhs, dimension);
  ComplexContourMatrix product(
      dimension, std::vector<ComplexContourNumber>(dimension, ComplexContourNumber{}));
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t inner = 0; inner < dimension; ++inner) {
      for (std::size_t column = 0; column < dimension; ++column) {
        product[row][column] += lhs[row][inner] * rhs[inner][column];
      }
    }
  }
  return product;
}

ComplexContourNumber MatrixTrace(const ComplexContourMatrix& matrix) {
  ComplexContourNumber trace;
  for (std::size_t index = 0; index < matrix.size(); ++index) {
    trace += matrix[index][index];
  }
  return trace;
}

std::vector<ComplexContourNumber> ComputeCharacteristicPolynomialCoefficients(
    const ComplexContourMatrix& matrix) {
  const std::size_t dimension = matrix.size();
  std::vector<ComplexContourNumber> coefficients;
  coefficients.reserve(dimension + 1);
  coefficients.push_back({1, 0});

  ComplexContourMatrix leverrier_matrix = MakeIdentityMatrix(dimension);
  for (std::size_t degree = 1; degree <= dimension; ++degree) {
    ComplexContourMatrix product =
        MatrixMatrixProduct(matrix, leverrier_matrix);
    const ComplexContourNumber coefficient =
        -MatrixTrace(product) / ComplexContourFloat(std::to_string(degree));
    coefficients.push_back(coefficient);
    for (std::size_t index = 0; index < dimension; ++index) {
      product[index][index] += coefficient;
    }
    leverrier_matrix = std::move(product);
  }
  return coefficients;
}

std::string ClassifyTriangularResidue(const ComplexContourMatrix& residue_matrix,
                                      const ComplexContourFloat& tolerance) {
  bool has_upper_entry = false;
  bool has_lower_entry = false;
  for (std::size_t row = 0; row < residue_matrix.size(); ++row) {
    for (std::size_t column = 0; column < residue_matrix[row].size(); ++column) {
      if (row == column || ComplexAbs(residue_matrix[row][column]) <= tolerance) {
        continue;
      }
      if (row < column) {
        has_upper_entry = true;
      } else {
        has_lower_entry = true;
      }
    }
  }
  if (!has_upper_entry && !has_lower_entry) {
    return "diagonal";
  }
  if (!has_upper_entry) {
    return "lower";
  }
  if (!has_lower_entry) {
    return "upper";
  }
  return "dense";
}

bool HasTriangularIndicialRoots(const std::string& triangular_form) {
  return triangular_form == "diagonal" || triangular_form == "lower" ||
         triangular_form == "upper";
}

std::string CompactComplexVector(const std::vector<ComplexContourNumber>& values) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << CompactComplex(values[index], 40);
  }
  out << "]";
  return out.str();
}

ComplexContourMatrix SampleEtaTimesMatrix(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t dimension,
    const ComplexContourNumber& eta) {
  ComplexContourMatrix residue_matrix = matrix_evaluator(eta);
  RequireMatrixShape(residue_matrix, dimension);
  for (std::vector<ComplexContourNumber>& row : residue_matrix) {
    for (ComplexContourNumber& entry : row) {
      entry *= eta;
      if (!IsFinite(entry)) {
        throw std::runtime_error("nonfinite-residue-entry");
      }
    }
  }
  return residue_matrix;
}

ComplexContourFloat MaxMatrixDifference(const ComplexContourMatrix& lhs,
                                        const ComplexContourMatrix& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("matrix-dimension-mismatch");
  }
  ComplexContourFloat difference = 0;
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    if (lhs[row].size() != rhs[row].size()) {
      throw std::runtime_error("matrix-dimension-mismatch");
    }
    for (std::size_t column = 0; column < lhs[row].size(); ++column) {
      difference =
          std::max(difference, ComplexAbs(lhs[row][column] - rhs[row][column]));
    }
  }
  return difference;
}

ComplexContourIndicialEquation IndicialFailure(
    const std::string& failure_code,
    const std::string& summary,
    const std::size_t dimension,
    const ComplexContourNumber& residue_probe_eta,
    const ComplexContourFloat& residue_tolerance) {
  ComplexContourIndicialEquation equation;
  equation.success = false;
  equation.dimension = dimension;
  equation.residue_probe_eta = residue_probe_eta;
  equation.residue_tolerance = residue_tolerance;
  equation.failure_code = failure_code;
  equation.summary = summary;
  return equation;
}

ComplexContourFrobeniusRecurrence FrobeniusFailure(
    const std::string& failure_code,
    const std::string& summary,
    const std::size_t dimension,
    const ComplexContourFrobeniusRecurrenceOptions& options) {
  ComplexContourFrobeniusRecurrence recurrence;
  recurrence.success = false;
  recurrence.dimension = dimension;
  recurrence.selected_root_index = options.selected_root_index;
  recurrence.order = options.order;
  recurrence.residue_probe_eta = options.residue_probe_eta;
  recurrence.residue_tolerance = options.residue_tolerance;
  recurrence.tail_fit_tolerance = options.tail_fit_tolerance;
  recurrence.failure_code = failure_code;
  recurrence.summary = summary;
  return recurrence;
}

ComplexContourFrobeniusEndpointEvaluation FrobeniusEndpointFailure(
    const std::string& failure_code,
    const std::string& summary,
    const std::string& limit_classification = {}) {
  ComplexContourFrobeniusEndpointEvaluation evaluation;
  evaluation.success = false;
  evaluation.endpoint_value_available = false;
  evaluation.failure_code = failure_code;
  evaluation.limit_classification = limit_classification;
  evaluation.summary = summary;
  return evaluation;
}

ComplexContourScalarReducibleEndpointRows ScalarReducibleEndpointFailure(
    const std::string& failure_code,
    const std::string& summary,
    const std::size_t dimension,
    const ComplexContourNumber& match_eta,
    const ComplexContourScalarReducibleEndpointOptions& options) {
  ComplexContourScalarReducibleEndpointRows rows;
  rows.success = false;
  rows.dimension = dimension;
  rows.endpoint = options.endpoint;
  rows.match_eta = match_eta;
  rows.residue_probe_eta = options.residue_probe_eta;
  rows.residue_tolerance = options.residue_tolerance;
  rows.tail_fit_tolerance = options.tail_fit_tolerance;
  rows.coupling_tolerance = options.coupling_tolerance;
  rows.tail_order = options.tail_order;
  rows.frobenius_order = options.frobenius_order;
  rows.sample_count = options.sample_count;
  rows.failure_code = failure_code;
  rows.summary = summary;
  return rows;
}

std::string CompactIndexVector(const std::vector<std::size_t>& values) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << values[index];
  }
  out << "]";
  return out.str();
}

ComplexContourMatrix MakeZeroMatrix(const std::size_t dimension) {
  return ComplexContourMatrix(
      dimension, std::vector<ComplexContourNumber>(dimension, ComplexContourNumber{}));
}

ComplexContourVector MakeZeroVector(const std::size_t dimension) {
  return ComplexContourVector(dimension, ComplexContourNumber{});
}

ComplexContourMatrix AddScaledMatrix(ComplexContourMatrix matrix,
                                     const ComplexContourMatrix& addend,
                                     const ComplexContourNumber& scale) {
  RequireMatrixShape(matrix, addend.size());
  RequireMatrixShape(addend, matrix.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      matrix[row][column] += addend[row][column] * scale;
    }
  }
  return matrix;
}

std::vector<ComplexContourNumber> LagrangeBasisPolynomial(
    const std::vector<ComplexContourNumber>& sample_points,
    const std::size_t basis_index) {
  std::vector<ComplexContourNumber> coefficients = {{1, 0}};
  ComplexContourNumber denominator{1, 0};
  for (std::size_t point_index = 0; point_index < sample_points.size();
       ++point_index) {
    if (point_index == basis_index) {
      continue;
    }
    std::vector<ComplexContourNumber> next(coefficients.size() + 1,
                                           ComplexContourNumber{});
    for (std::size_t power = 0; power < coefficients.size(); ++power) {
      next[power] -= coefficients[power] * sample_points[point_index];
      next[power + 1] += coefficients[power];
    }
    coefficients = std::move(next);
    denominator *= sample_points[basis_index] - sample_points[point_index];
  }
  for (ComplexContourNumber& coefficient : coefficients) {
    coefficient /= denominator;
  }
  return coefficients;
}

std::vector<ComplexContourMatrix> FitEtaTimesMatrixCoefficients(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t dimension,
    const ComplexContourNumber& probe_eta,
    const std::size_t coefficient_count) {
  std::vector<ComplexContourNumber> sample_points;
  std::vector<ComplexContourMatrix> sample_values;
  sample_points.reserve(coefficient_count);
  sample_values.reserve(coefficient_count);
  for (std::size_t index = 0; index < coefficient_count; ++index) {
    const ComplexContourNumber sample_eta =
        probe_eta / ComplexContourFloat(std::to_string(index + 1));
    sample_points.push_back(sample_eta);
    sample_values.push_back(
        SampleEtaTimesMatrix(matrix_evaluator, dimension, sample_eta));
  }

  std::vector<ComplexContourMatrix> coefficients(coefficient_count,
                                                 MakeZeroMatrix(dimension));
  for (std::size_t basis_index = 0; basis_index < coefficient_count;
       ++basis_index) {
    const std::vector<ComplexContourNumber> basis =
        LagrangeBasisPolynomial(sample_points, basis_index);
    for (std::size_t power = 0; power < coefficient_count; ++power) {
      coefficients[power] =
          AddScaledMatrix(std::move(coefficients[power]),
                          sample_values[basis_index],
                          basis[power]);
    }
  }
  return coefficients;
}

ComplexContourMatrix EvaluateTailPolynomial(
    const std::vector<ComplexContourMatrix>& coefficients,
    const ComplexContourNumber& eta) {
  if (coefficients.empty()) {
    return {};
  }
  const std::size_t dimension = coefficients.front().size();
  ComplexContourMatrix value = MakeZeroMatrix(dimension);
  ComplexContourNumber eta_power{1, 0};
  for (const ComplexContourMatrix& coefficient : coefficients) {
    value = AddScaledMatrix(std::move(value), coefficient, eta_power);
    eta_power *= eta;
  }
  return value;
}

ComplexContourMatrix MakeFrobeniusRecurrenceMatrix(
    const ComplexContourMatrix& residue_matrix,
    const ComplexContourNumber& indicial_root,
    const std::size_t power) {
  const std::size_t dimension = residue_matrix.size();
  ComplexContourMatrix matrix = MakeZeroMatrix(dimension);
  const ComplexContourNumber diagonal_shift =
      indicial_root + ComplexContourFloat(std::to_string(power));
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < dimension; ++column) {
      matrix[row][column] = -residue_matrix[row][column];
    }
    matrix[row][row] += diagonal_shift;
  }
  return matrix;
}

ComplexContourVector AddVectors(ComplexContourVector lhs,
                                const ComplexContourVector& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("vector-dimension-mismatch");
  }
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    lhs[index] += rhs[index];
  }
  return lhs;
}

ComplexContourVector SolveComplexLinearSystem(ComplexContourMatrix matrix,
                                              ComplexContourVector rhs,
                                              ComplexContourFloat tolerance) {
  const std::size_t dimension = matrix.size();
  RequireMatrixShape(matrix, dimension);
  if (rhs.size() != dimension) {
    throw std::runtime_error("vector-dimension-mismatch");
  }
  ComplexContourFloat scale = std::max<ComplexContourFloat>(
      ComplexContourFloat(1), MaxMatrixEntryNorm(matrix));
  for (const ComplexContourNumber& value : rhs) {
    scale = std::max(scale, ComplexAbs(value));
  }
  const ComplexContourFloat scaled_tolerance = tolerance * scale;
  const ComplexContourFloat effective_tolerance =
      std::max(tolerance, scaled_tolerance);

  for (std::size_t pivot = 0; pivot < dimension; ++pivot) {
    std::size_t best_row = pivot;
    ComplexContourFloat best_abs = ComplexAbs(matrix[pivot][pivot]);
    for (std::size_t row = pivot + 1; row < dimension; ++row) {
      const ComplexContourFloat candidate_abs = ComplexAbs(matrix[row][pivot]);
      if (candidate_abs > best_abs) {
        best_abs = candidate_abs;
        best_row = row;
      }
    }
    if (best_abs <= effective_tolerance) {
      throw std::runtime_error("frobenius-recurrence-singular-pivot");
    }
    if (best_row != pivot) {
      std::swap(matrix[best_row], matrix[pivot]);
      std::swap(rhs[best_row], rhs[pivot]);
    }
    for (std::size_t row = pivot + 1; row < dimension; ++row) {
      const ComplexContourNumber factor = matrix[row][pivot] / matrix[pivot][pivot];
      for (std::size_t column = pivot; column < dimension; ++column) {
        matrix[row][column] -= factor * matrix[pivot][column];
      }
      rhs[row] -= factor * rhs[pivot];
    }
  }

  ComplexContourVector solution(dimension, ComplexContourNumber{});
  for (std::size_t reverse_index = 0; reverse_index < dimension; ++reverse_index) {
    const std::size_t row = dimension - 1 - reverse_index;
    ComplexContourNumber remainder = rhs[row];
    for (std::size_t column = row + 1; column < dimension; ++column) {
      remainder -= matrix[row][column] * solution[column];
    }
    solution[row] = remainder / matrix[row][row];
  }
  return solution;
}

ComplexContourVector ComputeTriangularFrobeniusLeadingCoefficients(
    const ComplexContourMatrix& residue_matrix,
    const ComplexContourNumber& indicial_root,
    const std::size_t selected_root_index,
    const ComplexContourFloat& tolerance) {
  const std::size_t dimension = residue_matrix.size();
  if (selected_root_index >= dimension) {
    throw std::runtime_error("frobenius-leading-root-index-out-of-range");
  }
  ComplexContourMatrix matrix =
      MakeFrobeniusRecurrenceMatrix(residue_matrix, indicial_root, 0);
  ComplexContourFloat scale =
      std::max<ComplexContourFloat>(ComplexContourFloat(1),
                                    MaxMatrixEntryNorm(matrix));
  const ComplexContourFloat scaled_tolerance = tolerance * scale;
  const ComplexContourFloat pivot_tolerance =
      std::max(tolerance, scaled_tolerance);

  std::size_t pivot_row = 0;
  std::vector<std::size_t> pivot_columns;
  for (std::size_t column = 0; column < dimension && pivot_row < dimension;
       ++column) {
    std::size_t best_row = pivot_row;
    ComplexContourFloat best_abs = ComplexAbs(matrix[pivot_row][column]);
    for (std::size_t row = pivot_row + 1; row < dimension; ++row) {
      const ComplexContourFloat candidate_abs = ComplexAbs(matrix[row][column]);
      if (candidate_abs > best_abs) {
        best_abs = candidate_abs;
        best_row = row;
      }
    }
    if (best_abs <= pivot_tolerance) {
      continue;
    }
    if (best_row != pivot_row) {
      std::swap(matrix[best_row], matrix[pivot_row]);
    }
    const ComplexContourNumber pivot_value = matrix[pivot_row][column];
    for (std::size_t col = column; col < dimension; ++col) {
      matrix[pivot_row][col] /= pivot_value;
    }
    for (std::size_t row = 0; row < dimension; ++row) {
      if (row == pivot_row) {
        continue;
      }
      const ComplexContourNumber factor = matrix[row][column];
      if (ComplexAbs(factor) <= pivot_tolerance) {
        continue;
      }
      for (std::size_t col = column; col < dimension; ++col) {
        matrix[row][col] -= factor * matrix[pivot_row][col];
      }
    }
    pivot_columns.push_back(column);
    ++pivot_row;
  }

  std::vector<bool> pivot_column_mask(dimension, false);
  for (const std::size_t column : pivot_columns) {
    pivot_column_mask[column] = true;
  }
  std::vector<std::size_t> free_columns;
  free_columns.reserve(dimension - pivot_columns.size());
  if (!pivot_column_mask[selected_root_index]) {
    free_columns.push_back(selected_root_index);
  }
  for (std::size_t column = 0; column < dimension; ++column) {
    if (!pivot_column_mask[column] && column != selected_root_index) {
      free_columns.push_back(column);
    }
  }
  if (free_columns.empty()) {
    throw std::runtime_error("frobenius-leading-nullspace-empty");
  }

  for (const std::size_t free_column : free_columns) {
    ComplexContourVector leading = MakeZeroVector(dimension);
    leading[free_column] = ComplexContourNumber{1, 0};
    for (std::size_t row = 0; row < pivot_columns.size(); ++row) {
      const std::size_t pivot_column = pivot_columns[row];
      ComplexContourNumber value;
      for (std::size_t column = 0; column < dimension; ++column) {
        if (!pivot_column_mask[column]) {
          value -= matrix[row][column] * leading[column];
        }
      }
      leading[pivot_column] = value;
    }
    if (ComplexAbs(leading[selected_root_index]) <= pivot_tolerance) {
      continue;
    }
    const ComplexContourNumber normalization =
        leading[selected_root_index];
    for (ComplexContourNumber& entry : leading) {
      entry /= normalization;
    }
    const ComplexContourVector residual = MatrixVectorProduct(
        MakeFrobeniusRecurrenceMatrix(residue_matrix, indicial_root, 0),
        leading);
    if (MaxVectorNorm(residual) <= pivot_tolerance) {
      return leading;
    }
  }
  throw std::runtime_error("frobenius-leading-nullspace-normalization-failed");
}

std::size_t DefaultFrobeniusSampleCount(const std::size_t order) {
  return std::max<std::size_t>(32, 4 * (order + 2));
}

ComplexContourNumber ComplexIntegerPower(ComplexContourNumber base,
                                         long long exponent) {
  if (exponent == 0) {
    return {1, 0};
  }
  bool invert = false;
  if (exponent < 0) {
    invert = true;
    exponent = -exponent;
  }
  ComplexContourNumber result{1, 0};
  while (exponent > 0) {
    if ((exponent & 1LL) != 0) {
      result *= base;
    }
    exponent >>= 1LL;
    if (exponent > 0) {
      base *= base;
    }
  }
  return invert ? ComplexContourNumber{1, 0} / result : result;
}

ComplexContourNumber UnitComplexAtRootOfUnity(const std::size_t index,
                                              const std::size_t sample_count) {
  const ComplexContourFloat two_pi =
      boost::math::constants::two_pi<ComplexContourFloat>();
  const ComplexContourFloat theta =
      two_pi * ComplexContourFloat(index) / ComplexContourFloat(sample_count);
  return {cos(theta), sin(theta)};
}

void RequireScalarFrobeniusMatrix(const ComplexContourMatrix& matrix,
                                  const std::string& context) {
  if (matrix.size() != 1 || matrix.front().size() != 1) {
    throw std::runtime_error(context + " requires a scalar 1x1 eta matrix");
  }
  if (!IsFinite(matrix.front().front())) {
    throw std::runtime_error(context + " encountered a nonfinite scalar eta-matrix entry");
  }
}

ComplexContourNumber ComplexLogForEndpointBranch(
    const ComplexContourNumber& value,
    const EtaContourHalfPlane half_plane) {
  const ComplexContourFloat radius = ComplexAbs(value);
  if (radius == 0) {
    throw std::runtime_error(
        "scalar Frobenius endpoint evaluation requires a nonzero match displacement");
  }
  const ComplexContourFloat pi =
      boost::math::constants::pi<ComplexContourFloat>();
  ComplexContourFloat argument = atan2(value.imag(), value.real());
  if (value.imag() == 0 && value.real() < 0) {
    argument = half_plane == EtaContourHalfPlane::Upper ? pi : -pi;
  } else if (half_plane == EtaContourHalfPlane::Lower && argument > 0) {
    argument -= ComplexContourFloat(2) * pi;
  } else if (half_plane == EtaContourHalfPlane::Upper && argument < 0) {
    argument += ComplexContourFloat(2) * pi;
  }
  return {log(radius), argument};
}

ComplexContourNumber ComplexPowerForEndpointBranch(
    const ComplexContourNumber& base,
    const ComplexContourNumber& exponent,
    const EtaContourHalfPlane half_plane) {
  return exp(exponent * ComplexLogForEndpointBranch(base, half_plane));
}

void ValidateScalarFrobeniusPatch(
    const ComplexContourScalarFrobeniusSeriesPatch& patch,
    const std::string& context) {
  if (patch.series_coefficients.size() != patch.order + 1) {
    throw std::runtime_error(context +
                             " requires order+1 Frobenius series coefficients");
  }
  if (patch.regular_tail_coefficients.size() != patch.order) {
    throw std::runtime_error(context +
                             " requires order regular-tail coefficients");
  }
}

}  // namespace

ComplexContourIndicialEquation ComputeComplexContourEtaZeroIndicialEquation(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t dimension) {
  return ComputeComplexContourEtaZeroIndicialEquation(
      matrix_evaluator,
      dimension,
      ComplexContourNumber{0, ComplexContourFloat("-1e-40")},
      ComplexContourFloat("1e-28"));
}

ComplexContourIndicialEquation ComputeComplexContourEtaZeroIndicialEquation(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t dimension,
    const ComplexContourNumber residue_probe_eta,
    const ComplexContourFloat residue_tolerance) {
  if (dimension == 0) {
    return IndicialFailure(
        "empty-indicial-dimension",
        "b61n eta=0 indicial equation requires at least one eta-matrix row",
        dimension,
        residue_probe_eta,
        residue_tolerance);
  }
  if (!matrix_evaluator) {
    return IndicialFailure(
        "missing-matrix-evaluator",
        "b61n eta=0 indicial equation requires a matrix evaluator",
        dimension,
        residue_probe_eta,
        residue_tolerance);
  }
  if (!IsFinite(residue_probe_eta) || ComplexAbs(residue_probe_eta) == 0) {
    return IndicialFailure(
        "invalid-residue-probe",
        "b61n eta=0 indicial equation requires a finite nonzero residue probe eta",
        dimension,
        residue_probe_eta,
        residue_tolerance);
  }
  if (!IsFiniteFloat(residue_tolerance) || residue_tolerance < 0) {
    return IndicialFailure(
        "invalid-residue-tolerance",
        "b61n eta=0 indicial equation requires a finite nonnegative residue tolerance",
        dimension,
        residue_probe_eta,
        residue_tolerance);
  }

  try {
    ComplexContourIndicialEquation equation;
    equation.success = true;
    equation.dimension = dimension;
    equation.residue_probe_eta = residue_probe_eta;
    equation.residue_tolerance = residue_tolerance;
    equation.residue_matrix =
        SampleEtaTimesMatrix(matrix_evaluator, dimension, residue_probe_eta);
    const ComplexContourNumber refined_probe_eta =
        residue_probe_eta / ComplexContourFloat(2);
    const ComplexContourMatrix refined_residue_matrix =
        SampleEtaTimesMatrix(matrix_evaluator, dimension, refined_probe_eta);
    const ComplexContourFloat residue_stability =
        MaxMatrixDifference(equation.residue_matrix, refined_residue_matrix);
    ComplexContourFloat residue_scale = 1;
    residue_scale =
        std::max(residue_scale, MaxMatrixEntryNorm(equation.residue_matrix));
    residue_scale =
        std::max(residue_scale, MaxMatrixEntryNorm(refined_residue_matrix));
    const ComplexContourFloat scaled_residue_tolerance =
        residue_tolerance * residue_scale;
    const ComplexContourFloat effective_residue_tolerance =
        std::max(residue_tolerance, scaled_residue_tolerance);
    equation.sampled_residue_stability_abs =
        CompactFloat(residue_stability, 40);
    equation.sampled_residue_effective_tolerance_abs =
        CompactFloat(effective_residue_tolerance, 40);
    if (residue_stability > effective_residue_tolerance) {
      ComplexContourIndicialEquation failure = IndicialFailure(
          "non-fuchsian-residue-probe",
          "b61n eta=0 indicial equation refused to treat the sampled eta*A(eta) "
          "matrix as a simple-pole residue because the eta/2 probe changed it by " +
              CompactFloat(residue_stability, 40) +
              ", exceeding effective tolerance " +
              CompactFloat(effective_residue_tolerance, 40) +
              "; higher-order or non-Fuchsian endpoint behavior remains deferred",
          dimension,
          residue_probe_eta,
          residue_tolerance);
      failure.sampled_residue_stability_abs =
          equation.sampled_residue_stability_abs;
      failure.sampled_residue_effective_tolerance_abs =
          equation.sampled_residue_effective_tolerance_abs;
      return failure;
    }

    equation.characteristic_polynomial_coefficients_descending =
        ComputeCharacteristicPolynomialCoefficients(equation.residue_matrix);
    equation.triangular_form =
        ClassifyTriangularResidue(equation.residue_matrix, residue_tolerance);
    equation.indicial_roots_available =
        HasTriangularIndicialRoots(equation.triangular_form);
    if (equation.indicial_roots_available) {
      equation.indicial_roots.reserve(dimension);
      for (std::size_t index = 0; index < dimension; ++index) {
        equation.indicial_roots.push_back(equation.residue_matrix[index][index]);
      }
    }

    equation.summary =
        "Computed b61n probed eta=0 indicial equation "
        "det(rho*I - sampled_eta_times_A_eta)=0; "
        "dimension=" +
        std::to_string(equation.dimension) +
        "; residue_probe_eta=" + CompactComplex(equation.residue_probe_eta, 40) +
        "; residue_tolerance=" + CompactFloat(equation.residue_tolerance, 40) +
        "; sampled_residue_stability_abs=" +
        equation.sampled_residue_stability_abs +
        "; sampled_residue_effective_tolerance_abs=" +
        equation.sampled_residue_effective_tolerance_abs +
        "; triangular_form=" + equation.triangular_form +
        "; polynomial_degree=" + std::to_string(equation.dimension) +
        "; coefficient_count=" +
        std::to_string(
            equation.characteristic_polynomial_coefficients_descending.size()) +
        "; indicial_roots_available=" +
        (equation.indicial_roots_available ? std::string("true")
                                           : std::string("false"));
    if (equation.indicial_roots_available) {
      equation.summary += "; indicial_roots=" +
                          CompactComplexVector(equation.indicial_roots);
    }
    return equation;
  } catch (const std::exception& error) {
    const std::string error_message = error.what();
    if (error_message.find("matrix-dimension-mismatch") != std::string::npos) {
      return IndicialFailure(
          "matrix-dimension-mismatch",
          "b61n eta=0 indicial equation requires the sampled eta matrix to be square "
          "and dimension-matched",
          dimension,
          residue_probe_eta,
          residue_tolerance);
    }
    if (error_message.find("nonfinite-residue-entry") != std::string::npos) {
      return IndicialFailure(
          "nonfinite-residue-entry",
          "b61n eta=0 indicial equation found a nonfinite sampled eta*A(eta) entry",
          dimension,
          residue_probe_eta,
          residue_tolerance);
    }
    return IndicialFailure(
        "indicial-equation-failed",
        std::string("b61n eta=0 indicial equation failed closed: ") + error_message,
        dimension,
        residue_probe_eta,
        residue_tolerance);
  }
}

ComplexContourFrobeniusRecurrence
ComputeComplexContourEtaZeroFrobeniusRecurrence(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t dimension,
    const ComplexContourFrobeniusRecurrenceOptions& options) {
  if (dimension == 0) {
    return FrobeniusFailure(
        "empty-frobenius-dimension",
        "b61n eta=0 Frobenius recurrence requires at least one eta-matrix row",
        dimension,
        options);
  }
  if (!matrix_evaluator) {
    return FrobeniusFailure(
        "missing-matrix-evaluator",
        "b61n eta=0 Frobenius recurrence requires a matrix evaluator",
        dimension,
        options);
  }
  if (options.selected_root_index >= dimension) {
    return FrobeniusFailure(
        "selected-root-index-out-of-range",
        "b61n eta=0 Frobenius recurrence selected root index is outside the "
        "matrix dimension",
        dimension,
        options);
  }
  if (!options.leading_coefficients.empty() &&
      options.leading_coefficients.size() != dimension) {
    return FrobeniusFailure(
        "leading-coefficient-dimension-mismatch",
        "b61n eta=0 Frobenius recurrence leading coefficient vector does not "
        "match the matrix dimension",
        dimension,
        options);
  }
  if (!IsFinite(options.residue_probe_eta) ||
      ComplexAbs(options.residue_probe_eta) == 0) {
    return FrobeniusFailure(
        "invalid-residue-probe",
        "b61n eta=0 Frobenius recurrence requires a finite nonzero residue "
        "probe eta",
        dimension,
        options);
  }
  if (!IsFiniteFloat(options.tail_fit_tolerance) ||
      options.tail_fit_tolerance < 0) {
    return FrobeniusFailure(
        "invalid-tail-fit-tolerance",
        "b61n eta=0 Frobenius recurrence requires a finite nonnegative tail-fit "
        "tolerance",
        dimension,
        options);
  }

  const ComplexContourIndicialEquation indicial =
      ComputeComplexContourEtaZeroIndicialEquation(matrix_evaluator,
                                                   dimension,
                                                   options.residue_probe_eta,
                                                   options.residue_tolerance);
  if (!indicial.success) {
    return FrobeniusFailure(
        "indicial-" + indicial.failure_code,
        "b61n eta=0 Frobenius recurrence could not start because the indicial "
        "probe failed: " +
            indicial.summary,
        dimension,
        options);
  }
  if (!indicial.indicial_roots_available ||
      indicial.indicial_roots.size() != dimension) {
    return FrobeniusFailure(
        "indicial-roots-unavailable",
        "b61n eta=0 Frobenius recurrence requires triangular indicial roots "
        "before extracting branch coefficients",
        dimension,
        options);
  }

  try {
    ComplexContourFrobeniusRecurrence recurrence;
    recurrence.success = true;
    recurrence.dimension = dimension;
    recurrence.selected_root_index = options.selected_root_index;
    recurrence.order = options.order;
    recurrence.indicial_root =
        indicial.indicial_roots[options.selected_root_index];
    recurrence.residue_probe_eta = options.residue_probe_eta;
    recurrence.residue_tolerance = options.residue_tolerance;
    recurrence.tail_fit_tolerance = options.tail_fit_tolerance;
    const std::vector<ComplexContourMatrix> eta_times_coefficients =
        FitEtaTimesMatrixCoefficients(matrix_evaluator,
                                      dimension,
                                      options.residue_probe_eta,
                                      options.order + 1);
    recurrence.residue_matrix = eta_times_coefficients.front();
    recurrence.regular_tail_matrices.assign(eta_times_coefficients.begin() + 1,
                                            eta_times_coefficients.end());

    if (!eta_times_coefficients.empty()) {
      const ComplexContourNumber check_eta =
          options.residue_probe_eta /
          ComplexContourFloat(std::to_string(options.order + 2));
      const ComplexContourMatrix sampled_eta_times_matrix =
          SampleEtaTimesMatrix(matrix_evaluator, dimension, check_eta);
      const ComplexContourMatrix fitted_eta_times_matrix =
          EvaluateTailPolynomial(eta_times_coefficients, check_eta);
      const ComplexContourFloat tail_residual =
          MaxMatrixDifference(sampled_eta_times_matrix, fitted_eta_times_matrix);
      recurrence.tail_fit_residual_abs = CompactFloat(tail_residual, 40);
      ComplexContourFloat tail_scale = std::max<ComplexContourFloat>(
          ComplexContourFloat(1), MaxMatrixEntryNorm(sampled_eta_times_matrix));
      tail_scale = std::max(tail_scale, MaxMatrixEntryNorm(fitted_eta_times_matrix));
      const ComplexContourFloat scaled_tail_tolerance =
          options.tail_fit_tolerance * tail_scale;
      const ComplexContourFloat effective_tail_tolerance =
          std::max(options.tail_fit_tolerance, scaled_tail_tolerance);
      if (tail_residual > effective_tail_tolerance) {
        return FrobeniusFailure(
            "regular-tail-fit-residual-too-large",
            "b61n eta=0 Frobenius recurrence refused to publish tail "
            "coefficients because the polynomial tail fit residual " +
                CompactFloat(tail_residual, 40) +
                " exceeded effective tolerance " +
                CompactFloat(effective_tail_tolerance, 40),
            dimension,
            options);
      }
    } else {
      recurrence.tail_fit_residual_abs = "0";
    }

    recurrence.coefficients.reserve(options.order + 1);
    std::string leading_coefficient_source;
    ComplexContourVector leading_coefficients =
        options.leading_coefficients.empty()
            ? ComputeTriangularFrobeniusLeadingCoefficients(
                  recurrence.residue_matrix,
                  recurrence.indicial_root,
                  options.selected_root_index,
                  options.residue_tolerance)
            : options.leading_coefficients;
    leading_coefficient_source =
        options.leading_coefficients.empty()
            ? "canonical-triangular-indicial-null-vector"
            : "caller-supplied";
    RequireFiniteVector(leading_coefficients, "frobenius-leading");
    const ComplexContourVector indicial_residual = MatrixVectorProduct(
        MakeFrobeniusRecurrenceMatrix(recurrence.residue_matrix,
                                      recurrence.indicial_root,
                                      0),
        leading_coefficients);
    const ComplexContourFloat indicial_residual_norm =
        MaxVectorNorm(indicial_residual);
    ComplexContourFloat leading_scale = std::max<ComplexContourFloat>(
        ComplexContourFloat(1), MaxVectorNorm(leading_coefficients));
    leading_scale = std::max(leading_scale,
                             MaxMatrixEntryNorm(recurrence.residue_matrix));
    const ComplexContourFloat scaled_indicial_tolerance =
        options.residue_tolerance * leading_scale;
    const ComplexContourFloat effective_indicial_tolerance =
        std::max(options.residue_tolerance, scaled_indicial_tolerance);
    if (indicial_residual_norm > effective_indicial_tolerance) {
      return FrobeniusFailure(
          "leading-coefficients-fail-indicial-system",
          "b61n eta=0 Frobenius recurrence requires c_0 to satisfy "
          "(rho*I - R)c_0=0; residual " +
              CompactFloat(indicial_residual_norm, 40) +
              " exceeded effective tolerance " +
              CompactFloat(effective_indicial_tolerance, 40),
          dimension,
          options);
    }
    recurrence.coefficients.push_back(std::move(leading_coefficients));

    const ComplexContourFloat pivot_tolerance =
        std::max(options.residue_tolerance, ComplexContourFloat("1e-70"));
    for (std::size_t power = 1; power <= options.order; ++power) {
      ComplexContourVector rhs = MakeZeroVector(dimension);
      for (std::size_t tail_power = 0; tail_power < power; ++tail_power) {
        rhs = AddVectors(
            std::move(rhs),
            MatrixVectorProduct(
                recurrence.regular_tail_matrices[tail_power],
                recurrence.coefficients[power - 1 - tail_power]));
      }
      recurrence.coefficients.push_back(SolveComplexLinearSystem(
          MakeFrobeniusRecurrenceMatrix(recurrence.residue_matrix,
                                        recurrence.indicial_root,
                                        power),
          rhs,
          pivot_tolerance));
    }

    recurrence.summary =
        "Computed b61n eta=0 Frobenius recurrence coefficients; dimension=" +
        std::to_string(recurrence.dimension) +
        "; selected_root_index=" +
        std::to_string(recurrence.selected_root_index) +
        "; indicial_root=" + CompactComplex(recurrence.indicial_root, 40) +
        "; order=" + std::to_string(recurrence.order) +
        "; coefficient_count=" +
        std::to_string(recurrence.coefficients.size()) +
        "; leading_coefficient_source=" + leading_coefficient_source +
        "; regular_tail_coefficient_count=" +
        std::to_string(recurrence.regular_tail_matrices.size()) +
        "; recurrence_convention=" + recurrence.recurrence_convention +
        "; tail_fit_residual_abs=" + recurrence.tail_fit_residual_abs +
        "; m7_prep=recurrence-ready-for-endpoint-handler";
    return recurrence;
  } catch (const std::exception& error) {
    return FrobeniusFailure(
        "frobenius-recurrence-failed",
        std::string("b61n eta=0 Frobenius recurrence failed closed: ") +
            error.what(),
        dimension,
        options);
  }
}

ComplexContourFrobeniusEndpointEvaluation
EvaluateComplexContourFrobeniusEtaZeroEndpoint(
    const ComplexContourFrobeniusRecurrence& recurrence) {
  if (!recurrence.success) {
    return FrobeniusEndpointFailure(
        "recurrence-unsuccessful",
        "b61n eta=0 Frobenius endpoint evaluation requires a successful "
        "recurrence");
  }
  if (recurrence.coefficients.empty()) {
    return FrobeniusEndpointFailure(
        "missing-frobenius-coefficients",
        "b61n eta=0 Frobenius endpoint evaluation requires at least c_0");
  }
  if (recurrence.dimension == 0 ||
      recurrence.coefficients.front().size() != recurrence.dimension) {
    return FrobeniusEndpointFailure(
        "frobenius-coefficient-dimension-mismatch",
        "b61n eta=0 Frobenius endpoint evaluation requires c_0 to match the "
        "recurrence dimension");
  }
  if (!IsFinite(recurrence.indicial_root)) {
    return FrobeniusEndpointFailure(
        "nonfinite-indicial-root",
        "b61n eta=0 Frobenius endpoint evaluation requires a finite indicial "
        "root");
  }
  const ComplexContourFloat tolerance =
      std::max(recurrence.residue_tolerance, ComplexContourFloat("1e-70"));
  const ComplexContourFloat real_abs = abs(recurrence.indicial_root.real());
  const ComplexContourFloat imag_abs = abs(recurrence.indicial_root.imag());

  ComplexContourFrobeniusEndpointEvaluation evaluation;
  evaluation.success = true;
  evaluation.endpoint_value_available = true;
  if (real_abs <= tolerance && imag_abs <= tolerance) {
    evaluation.limit_classification = "finite-rho-zero";
    evaluation.endpoint_value = recurrence.coefficients.front();
  } else if (recurrence.indicial_root.real() > tolerance) {
    evaluation.limit_classification = "vanishing-positive-real-exponent";
    evaluation.endpoint_value = MakeZeroVector(recurrence.dimension);
  } else if (recurrence.indicial_root.real() < -tolerance) {
    return FrobeniusEndpointFailure(
        "singular-negative-real-exponent",
        "b61n eta=0 Frobenius endpoint evaluation found a negative-real "
        "indicial exponent, so the analytic eta=0 limit is singular",
        "singular-negative-real-exponent");
  } else {
    return FrobeniusEndpointFailure(
        "branch-dependent-zero-real-exponent",
        "b61n eta=0 Frobenius endpoint evaluation found a zero-real nonzero "
        "imaginary indicial exponent, so the eta=0 limit is branch dependent",
        "branch-dependent-zero-real-exponent");
  }

  evaluation.summary =
      "Evaluated b61n eta=0 Frobenius analytic endpoint; "
      "limit_classification=" +
      evaluation.limit_classification +
      "; endpoint_value_available=true; coefficient_source=c_0; "
      "full_eta_zero_contour_applied=false";
  return evaluation;
}

ComplexContourScalarReducibleEndpointRows
ApplyComplexContourScalarReducibleFrobeniusEndpointRows(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourNumber& match_eta,
    const ComplexContourVector& match_values,
    const ComplexContourScalarReducibleEndpointOptions& options) {
  const std::size_t dimension = match_values.size();
  if (dimension == 0) {
    return ScalarReducibleEndpointFailure(
        "empty-match-vector",
        "b61n scalar-reducible Frobenius endpoint rows require at least one "
        "matched master value",
        dimension,
        match_eta,
        options);
  }
  if (!matrix_evaluator) {
    return ScalarReducibleEndpointFailure(
        "missing-matrix-evaluator",
        "b61n scalar-reducible Frobenius endpoint rows require a matrix evaluator",
        dimension,
        match_eta,
        options);
  }
  if (!IsFinite(match_eta) || ComplexAbs(match_eta - options.endpoint) == 0) {
    return ScalarReducibleEndpointFailure(
        "invalid-match-eta",
        "b61n scalar-reducible Frobenius endpoint rows require a finite non-endpoint "
        "match eta",
        dimension,
        match_eta,
        options);
  }
  if (!IsFinite(options.endpoint)) {
    return ScalarReducibleEndpointFailure(
        "invalid-endpoint",
        "b61n scalar-reducible Frobenius endpoint rows require a finite endpoint",
        dimension,
        match_eta,
        options);
  }
  if (!IsFiniteFloat(options.residue_tolerance) ||
      options.residue_tolerance < 0) {
    return ScalarReducibleEndpointFailure(
        "invalid-residue-tolerance",
        "b61n scalar-reducible Frobenius endpoint rows require a finite "
        "nonnegative residue tolerance",
        dimension,
        match_eta,
        options);
  }
  if (!IsFiniteFloat(options.tail_fit_tolerance) ||
      options.tail_fit_tolerance < 0) {
    return ScalarReducibleEndpointFailure(
        "invalid-tail-fit-tolerance",
        "b61n scalar-reducible Frobenius endpoint rows require a finite "
        "nonnegative tail-fit tolerance",
        dimension,
        match_eta,
        options);
  }
  if (!IsFiniteFloat(options.coupling_tolerance) ||
      options.coupling_tolerance < 0) {
    return ScalarReducibleEndpointFailure(
        "invalid-coupling-tolerance",
        "b61n scalar-reducible Frobenius endpoint rows require a finite "
        "nonnegative coupling tolerance",
        dimension,
        match_eta,
        options);
  }
  if (!IsFiniteFloat(options.sample_radius) || options.sample_radius <= 0) {
    return ScalarReducibleEndpointFailure(
        "invalid-sample-radius",
        "b61n scalar-reducible Frobenius endpoint rows require a positive finite "
        "scalar sample radius",
        dimension,
        match_eta,
        options);
  }
  try {
    RequireFiniteVector(match_values, "scalar-reducible-match-values");
    const ComplexContourIndicialEquation indicial =
        ComputeComplexContourEtaZeroIndicialEquation(matrix_evaluator,
                                                     dimension,
                                                     options.residue_probe_eta,
                                                     options.residue_tolerance);
    if (!indicial.success) {
      return ScalarReducibleEndpointFailure(
          "indicial-" + indicial.failure_code,
          "b61n scalar-reducible Frobenius endpoint rows could not start because "
          "the indicial probe failed: " +
              indicial.summary,
          dimension,
          match_eta,
          options);
    }
    if (!indicial.indicial_roots_available ||
        indicial.indicial_roots.size() != dimension) {
      return ScalarReducibleEndpointFailure(
          "indicial-roots-unavailable",
          "b61n scalar-reducible Frobenius endpoint rows require triangular "
          "indicial roots before row classification",
          dimension,
          match_eta,
          options);
    }

    const std::vector<ComplexContourMatrix> eta_times_coefficients =
        FitEtaTimesMatrixCoefficients(matrix_evaluator,
                                      dimension,
                                      options.residue_probe_eta,
                                      options.tail_order + 1);
    const ComplexContourNumber check_eta =
        options.residue_probe_eta /
        ComplexContourFloat(std::to_string(options.tail_order + 2));
    const ComplexContourMatrix sampled_eta_times_matrix =
        SampleEtaTimesMatrix(matrix_evaluator, dimension, check_eta);
    const ComplexContourMatrix fitted_eta_times_matrix =
        EvaluateTailPolynomial(eta_times_coefficients, check_eta);
    const ComplexContourFloat tail_residual =
        MaxMatrixDifference(sampled_eta_times_matrix, fitted_eta_times_matrix);
    ComplexContourFloat tail_scale = std::max<ComplexContourFloat>(
        ComplexContourFloat(1), MaxMatrixEntryNorm(sampled_eta_times_matrix));
    tail_scale = std::max(tail_scale, MaxMatrixEntryNorm(fitted_eta_times_matrix));
    const ComplexContourFloat scaled_tail_tolerance =
        options.tail_fit_tolerance * tail_scale;
    const ComplexContourFloat effective_tail_tolerance =
        std::max(options.tail_fit_tolerance, scaled_tail_tolerance);
    if (tail_residual > effective_tail_tolerance) {
      return ScalarReducibleEndpointFailure(
          "regular-tail-fit-residual-too-large",
          "b61n scalar-reducible Frobenius endpoint rows refused to classify rows "
          "because the eta*A(eta) tail fit residual " +
              CompactFloat(tail_residual, 40) +
              " exceeded effective tolerance " +
              CompactFloat(effective_tail_tolerance, 40),
          dimension,
          match_eta,
          options);
    }

    ComplexContourScalarReducibleEndpointRows rows;
    rows.success = true;
    rows.dimension = dimension;
    rows.endpoint = options.endpoint;
    rows.match_eta = match_eta;
    rows.residue_probe_eta = options.residue_probe_eta;
    rows.residue_tolerance = options.residue_tolerance;
    rows.tail_fit_tolerance = options.tail_fit_tolerance;
    rows.coupling_tolerance = options.coupling_tolerance;
    rows.tail_order = options.tail_order;
    rows.frobenius_order = options.frobenius_order;
    rows.sample_count = options.sample_count;
    rows.tail_fit_residual_abs = CompactFloat(tail_residual, 40);
    rows.indicial_roots = indicial.indicial_roots;
    rows.endpoint_values = MakeZeroVector(dimension);
    rows.endpoint_value_available.assign(dimension, false);

    const ComplexContourFloat endpoint_tolerance =
        std::max(std::max(options.residue_tolerance, options.coupling_tolerance),
                 ComplexContourFloat("1e-70"));
    for (std::size_t row = 0; row < dimension; ++row) {
      bool row_is_scalar_reducible = true;
      for (const ComplexContourMatrix& coefficient_matrix : eta_times_coefficients) {
        for (std::size_t column = 0; column < dimension; ++column) {
          if (column == row) {
            continue;
          }
          if (ComplexAbs(coefficient_matrix[row][column]) >
              options.coupling_tolerance) {
            row_is_scalar_reducible = false;
            break;
          }
        }
        if (!row_is_scalar_reducible) {
          break;
        }
      }

      if (!row_is_scalar_reducible) {
        rows.irreducible_row_indices.push_back(row);
        rows.deferred_endpoint_row_indices.push_back(row);
        continue;
      }

      rows.scalar_reducible_row_indices.push_back(row);
      const auto scalar_row_evaluator =
          [matrix_evaluator, dimension, row](const ComplexContourNumber& eta) {
            const ComplexContourMatrix matrix = matrix_evaluator(eta);
            RequireMatrixShape(matrix, dimension);
            return ComplexContourMatrix{{matrix[row][row]}};
          };
      const ComplexContourScalarFrobeniusSeriesPatch patch =
          GenerateScalarComplexFrobeniusEndpointPatch(
              scalar_row_evaluator,
              options.endpoint,
              options.sample_radius,
              options.frobenius_order,
              options.sample_count);
      const ComplexContourNumber endpoint_coefficient =
          MatchScalarComplexFrobeniusEndpointCoefficient(
              patch, match_eta, match_values[row], options.half_plane);
      const ComplexContourNumber indicial_root = patch.indicial_exponent;
      const ComplexContourFloat real_abs = abs(indicial_root.real());
      const ComplexContourFloat imag_abs = abs(indicial_root.imag());
      if (real_abs <= endpoint_tolerance && imag_abs <= endpoint_tolerance) {
        rows.endpoint_values[row] = endpoint_coefficient;
        rows.endpoint_value_available[row] = true;
        rows.endpoint_value_row_indices.push_back(row);
      } else if (indicial_root.real() > endpoint_tolerance) {
        rows.endpoint_values[row] = ComplexContourNumber{0, 0};
        rows.endpoint_value_available[row] = true;
        rows.endpoint_value_row_indices.push_back(row);
      } else {
        rows.deferred_endpoint_row_indices.push_back(row);
      }
    }

    rows.summary =
        "Applied b61n scalar-reducible Frobenius endpoint row classifier; "
        "dimension=" +
        std::to_string(rows.dimension) +
        "; scalar_reducible_row_count=" +
        std::to_string(rows.scalar_reducible_row_indices.size()) +
        "; scalar_reducible_rows=" +
        CompactIndexVector(rows.scalar_reducible_row_indices) +
        "; irreducible_row_count=" +
        std::to_string(rows.irreducible_row_indices.size()) +
        "; irreducible_rows=" +
        CompactIndexVector(rows.irreducible_row_indices) +
        "; endpoint_value_row_count=" +
        std::to_string(rows.endpoint_value_row_indices.size()) +
        "; endpoint_value_rows=" +
        CompactIndexVector(rows.endpoint_value_row_indices) +
        "; deferred_endpoint_rows=" +
        CompactIndexVector(rows.deferred_endpoint_row_indices) +
        "; tail_order=" + std::to_string(rows.tail_order) +
        "; frobenius_order=" + std::to_string(rows.frobenius_order) +
        "; tail_fit_residual_abs=" + rows.tail_fit_residual_abs +
        "; indicial_roots=" + CompactComplexVector(rows.indicial_roots) +
        "; irreducible subsystem remains on RK78/coupled-row path; "
        "coefficient_publication=false; full_eta_zero_contour_applied=false";
    return rows;
  } catch (const std::exception& error) {
    return ScalarReducibleEndpointFailure(
        "scalar-reducible-endpoint-row-classification-failed",
        std::string("b61n scalar-reducible Frobenius endpoint rows failed closed: ") +
            error.what(),
        dimension,
        match_eta,
        options);
  }
}

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
    const bool scalar_frobenius_endpoint_patch_applied =
        AppliesReviewedScalarFrobeniusRuntimeEndpointPatch(
            initial_values, waypoints, options);
    std::vector<ComplexContourNumber> propagation_waypoints = waypoints;
    if (scalar_frobenius_endpoint_patch_applied) {
      propagation_waypoints.pop_back();
    }
    const std::size_t integrated_segment_count =
        propagation_waypoints.empty() ? 0 : propagation_waypoints.size() - 1;

    AdaptiveRk45Result coarse =
        PropagateWaypointsWithAdaptiveRk45(initial_values,
                                           propagation_waypoints,
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
    ComplexContourFloat endpoint_vector_norm = MaxVectorNorm(coarse.values);
    ComplexContourFloat effective_refinement_tolerance =
        options.refinement_error_tolerance;
    std::size_t refined_steps_per_segment = options.steps_per_segment;
    std::size_t refinement_doublings_used = 0;
    bool refinement_passed = false;
    RefinementPeakDiagnostics refinement_peak;
    for (std::size_t doubling = 1; doubling <= options.max_refinement_doublings;
         ++doubling) {
      refined_steps_per_segment = options.steps_per_segment;
      for (std::size_t level = 0; level < doubling; ++level) {
        refined_steps_per_segment *= 2;
      }
      refined = PropagateWaypointsWithAdaptiveRk45(initial_values,
                                                   propagation_waypoints,
                                                   matrix_evaluator,
                                                   options,
                                                   refined_steps_per_segment);
      RequireFiniteVector(refined.values, "refined");
      refinement_error = MaxVectorDifference(previous.values, refined.values);
      refinement_peak =
          FindRefinementPeakDiagnostics(previous, refined, propagation_waypoints);
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
      ComplexContourPropagationResult failure = FailureResult(
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
      failure.diagnostics.coarse_step_count =
          options.steps_per_segment * integrated_segment_count;
      failure.diagnostics.refined_step_count =
          refined_steps_per_segment * integrated_segment_count;
      failure.diagnostics.refinement_doublings_used =
          refinement_doublings_used;
      failure.diagnostics.adaptive_step_count = refined.stats.accepted_steps;
      failure.diagnostics.adaptive_rejected_step_count =
          refined.stats.rejected_steps;
      failure.diagnostics.pole_pinch_step_count =
          refined.stats.pole_pinched_steps;
      failure.diagnostics.endpoint_vector_norm_abs =
          CompactFloat(endpoint_vector_norm, 40);
      failure.diagnostics.refinement_error_abs =
          CompactFloat(refinement_error, 40);
      failure.diagnostics.refinement_effective_tolerance_abs =
          CompactFloat(effective_refinement_tolerance, 40);
      failure.diagnostics.max_embedded_error_abs =
          CompactFloat(refined.stats.max_embedded_error_abs, 40);
      PopulateRefinementPeakDiagnostics(
          failure.diagnostics, refinement_peak, matrix_evaluator, options);
      PopulateEmbeddedErrorPeakDiagnostics(
          failure.diagnostics, refined.stats, matrix_evaluator, options);
      failure.diagnostics.summary += ScaleDiagnosticSummary(failure.diagnostics);
      return failure;
    }

    ComplexContourPropagationResult result;
    result.success = true;
    result.endpoint_values = refined.values;
    std::string scalar_frobenius_endpoint_patch_summary;
    if (scalar_frobenius_endpoint_patch_applied) {
      const ComplexContourNumber endpoint = waypoints.back();
      const ComplexContourNumber match_eta = propagation_waypoints.back();
      const ComplexContourScalarFrobeniusSeriesPatch patch =
          GenerateScalarComplexFrobeniusEndpointPatch(
              matrix_evaluator,
              endpoint,
              ScalarFrobeniusEndpointPatchRadius(waypoints, options),
              kScalarFrobeniusRuntimeEndpointOrder,
              kScalarFrobeniusRuntimeSampleCount);
      const ComplexContourFloat indicial_tolerance =
          ComplexContourFloat("1e-24");
      if (ComplexAbs(patch.indicial_exponent) > indicial_tolerance) {
        throw std::runtime_error(
            "scalar-frobenius-endpoint-patch-nonregular-indicial-exponent");
      }
      const ComplexContourNumber endpoint_value =
          MatchScalarComplexFrobeniusEndpointCoefficient(
              patch,
              match_eta,
              refined.values.front(),
              options.half_plane);
      if (!IsFinite(endpoint_value)) {
        throw std::runtime_error(
            "scalar-frobenius-endpoint-patch-nonfinite-endpoint-value");
      }
      result.endpoint_values = {endpoint_value};
      endpoint_vector_norm = MaxVectorNorm(result.endpoint_values);
      scalar_frobenius_endpoint_patch_summary =
          "; scalar_frobenius_endpoint_patch_applied=true; "
          "scalar_frobenius_endpoint_order=" +
          std::to_string(patch.order) +
          "; scalar_frobenius_sample_count=" +
          std::to_string(patch.sample_count) +
          "; scalar_frobenius_indicial_exponent=" +
          CompactComplex(patch.indicial_exponent, 40) +
          "; scalar_frobenius_match_eta=" +
          CompactComplex(match_eta, 40) +
          "; scalar_frobenius_endpoint_coefficient=" +
          CompactComplex(endpoint_value, 40);
    }
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
    result.diagnostics.scalar_frobenius_endpoint_patch_applied =
        scalar_frobenius_endpoint_patch_applied;
    result.diagnostics.dimension = initial_values.size();
    result.diagnostics.waypoint_count = waypoints.size();
    result.diagnostics.segment_count = waypoints.size() - 1;
    result.diagnostics.coarse_step_count =
        options.steps_per_segment * integrated_segment_count;
    result.diagnostics.refined_step_count =
        refined_steps_per_segment * integrated_segment_count;
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
        scalar_frobenius_endpoint_patch_applied
            ? "lower-half-plane-complex-ode-vector-propagation+"
              "scalar-frobenius-endpoint-patch"
            : "lower-half-plane-complex-ode-vector-propagation";
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
    PopulateRefinementPeakDiagnostics(
        result.diagnostics, refinement_peak, matrix_evaluator, options);
    PopulateEmbeddedErrorPeakDiagnostics(
        result.diagnostics, refined.stats, matrix_evaluator, options);
    result.diagnostics.contour_fingerprint = ComputeArtifactFingerprint(
        SerializePropagationForFingerprint(initial_values,
                                           waypoints,
                                           options,
                                           refined_steps_per_segment,
                                           coefficient_publication,
                                           endpoint_extraction_applied,
                                           scalar_frobenius_endpoint_patch_applied));
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
        scalar_frobenius_endpoint_patch_summary +
        ScaleDiagnosticSummary(result.diagnostics) +
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

ComplexContourScalarFrobeniusSeriesPatch
GenerateScalarComplexFrobeniusEndpointPatch(
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourNumber& endpoint,
    const ComplexContourFloat& sample_radius,
    const std::size_t order,
    std::size_t sample_count) {
  if (!matrix_evaluator) {
    throw std::runtime_error(
        "scalar Frobenius endpoint patch generation requires a matrix evaluator");
  }
  if (!IsFinite(endpoint)) {
    throw std::runtime_error(
        "scalar Frobenius endpoint patch generation requires a finite endpoint");
  }
  if (!IsFiniteFloat(sample_radius) || sample_radius <= 0) {
    throw std::runtime_error(
        "scalar Frobenius endpoint patch generation requires a positive finite "
        "sample radius");
  }
  if (sample_count == 0) {
    sample_count = DefaultFrobeniusSampleCount(order);
  }
  if (sample_count < 2 * (order + 2)) {
    throw std::runtime_error(
        "scalar Frobenius endpoint patch generation requires enough contour "
        "samples to resolve the requested local order");
  }

  std::vector<ComplexContourNumber> laurent_coefficients(order + 1);
  for (std::size_t sample_index = 0; sample_index < sample_count;
       ++sample_index) {
    const ComplexContourNumber displacement =
        sample_radius * UnitComplexAtRootOfUnity(sample_index, sample_count);
    const ComplexContourMatrix matrix = matrix_evaluator(endpoint + displacement);
    RequireScalarFrobeniusMatrix(matrix, "scalar Frobenius endpoint patch generation");
    const ComplexContourNumber value = matrix.front().front();
    for (std::size_t coefficient_index = 0;
         coefficient_index < laurent_coefficients.size();
         ++coefficient_index) {
      const long long degree =
          static_cast<long long>(coefficient_index) - 1LL;
      laurent_coefficients[coefficient_index] +=
          value * ComplexIntegerPower(displacement, -degree);
    }
  }
  const ComplexContourFloat sample_count_float(sample_count);
  for (ComplexContourNumber& coefficient : laurent_coefficients) {
    coefficient /= sample_count_float;
  }

  ComplexContourScalarFrobeniusSeriesPatch patch;
  patch.center = endpoint;
  patch.sample_radius = sample_radius;
  patch.order = order;
  patch.sample_count = sample_count;
  patch.indicial_exponent = laurent_coefficients.front();
  patch.regular_tail_coefficients.reserve(order);
  for (std::size_t index = 0; index < order; ++index) {
    patch.regular_tail_coefficients.push_back(laurent_coefficients[index + 1]);
  }

  patch.series_coefficients.assign(order + 1, ComplexContourNumber{});
  patch.series_coefficients.front() = ComplexContourNumber{1, 0};
  for (std::size_t term = 1; term <= order; ++term) {
    ComplexContourNumber numerator;
    for (std::size_t tail_index = 0; tail_index < term; ++tail_index) {
      numerator += patch.regular_tail_coefficients[tail_index] *
                   patch.series_coefficients[term - 1 - tail_index];
    }
    patch.series_coefficients[term] =
        numerator / ComplexContourFloat(term);
  }
  patch.indicial_equation =
      "lambda - Res[(eta-eta0) A(eta)] = 0";
  patch.summary =
      "Generated scalar b61n-style Frobenius endpoint patch from numeric eta-matrix "
      "samples; indicial_equation=lambda-residue=0; indicial_exponent=" +
      CompactComplex(patch.indicial_exponent, 40) +
      "; order=" + std::to_string(order) +
      "; sample_radius=" + CompactFloat(sample_radius, 40) +
      "; sample_count=" + std::to_string(sample_count) +
      "; endpoint_coefficient_convention=leading-Frobenius-coefficient";
  return patch;
}

ComplexContourNumber EvaluateScalarComplexFrobeniusSeries(
    const ComplexContourScalarFrobeniusSeriesPatch& patch,
    const ComplexContourNumber& eta,
    const EtaContourHalfPlane half_plane) {
  ValidateScalarFrobeniusPatch(patch, "scalar Frobenius endpoint evaluation");
  const ComplexContourNumber displacement = eta - patch.center;
  if (ComplexAbs(displacement) == 0) {
    if (ComplexAbs(patch.indicial_exponent) == 0) {
      return patch.series_coefficients.front();
    }
    throw std::runtime_error(
        "scalar Frobenius endpoint evaluation at the singular point is "
        "branch-sensitive; match the leading endpoint coefficient instead");
  }

  ComplexContourNumber regular_factor;
  ComplexContourNumber displacement_power{1, 0};
  for (const ComplexContourNumber& coefficient : patch.series_coefficients) {
    regular_factor += coefficient * displacement_power;
    displacement_power *= displacement;
  }
  return ComplexPowerForEndpointBranch(displacement,
                                       patch.indicial_exponent,
                                       half_plane) *
         regular_factor;
}

ComplexContourNumber MatchScalarComplexFrobeniusEndpointCoefficient(
    const ComplexContourScalarFrobeniusSeriesPatch& patch,
    const ComplexContourNumber& match_eta,
    const ComplexContourNumber& match_value,
    const EtaContourHalfPlane half_plane) {
  const ComplexContourNumber normalized_basis =
      EvaluateScalarComplexFrobeniusSeries(patch, match_eta, half_plane);
  if (ComplexAbs(normalized_basis) == 0) {
    throw std::runtime_error(
        "scalar Frobenius endpoint matching encountered a zero normalized basis");
  }
  return match_value / normalized_basis;
}

}  // namespace amflow
