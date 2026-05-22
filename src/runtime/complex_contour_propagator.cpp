#include "amflow/runtime/complex_contour_propagator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

ComplexContourFloat ComplexAbs(const ComplexContourNumber& value) {
  return sqrt(value.real() * value.real() + value.imag() * value.imag());
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

ComplexContourVector AddScaled(const ComplexContourVector& base,
                               const ComplexContourVector& direction,
                               const ComplexContourFloat& scale) {
  if (base.size() != direction.size()) {
    throw std::runtime_error("complex contour propagator vector size mismatch");
  }
  ComplexContourVector result(base.size());
  for (std::size_t index = 0; index < base.size(); ++index) {
    result[index] = base[index] + direction[index] * scale;
  }
  return result;
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

ComplexContourVector PropagateSegmentWithRk4(
    const ComplexContourVector& initial_values,
    const ComplexContourNumber& eta_start,
    const ComplexContourNumber& eta_end,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t steps) {
  if (steps == 0) {
    throw std::runtime_error("invalid-step-count");
  }
  const ComplexContourNumber segment = eta_end - eta_start;
  if (ComplexAbs(segment) == 0) {
    throw std::runtime_error("zero-length-contour-segment");
  }

  ComplexContourVector state = initial_values;
  const ComplexContourFloat h = ComplexContourFloat(1) / ComplexContourFloat(steps);
  const ComplexContourFloat h_over_two = h / ComplexContourFloat(2);
  const ComplexContourFloat h_over_six = h / ComplexContourFloat(6);
  const ComplexContourFloat two = 2;
  for (std::size_t step = 0; step < steps; ++step) {
    const ComplexContourFloat t0 = ComplexContourFloat(step) * h;
    const ComplexContourFloat t_mid = t0 + h_over_two;
    const ComplexContourFloat t1 = t0 + h;
    const ComplexContourNumber eta0 = eta_start + segment * t0;
    const ComplexContourNumber eta_mid = eta_start + segment * t_mid;
    const ComplexContourNumber eta1 = eta_start + segment * t1;

    const ComplexContourVector k1 =
        EvaluateContourDerivative(matrix_evaluator, eta0, state, segment);
    const ComplexContourVector k2 =
        EvaluateContourDerivative(matrix_evaluator,
                                  eta_mid,
                                  AddScaled(state, k1, h_over_two),
                                  segment);
    const ComplexContourVector k3 =
        EvaluateContourDerivative(matrix_evaluator,
                                  eta_mid,
                                  AddScaled(state, k2, h_over_two),
                                  segment);
    const ComplexContourVector k4 =
        EvaluateContourDerivative(matrix_evaluator,
                                  eta1,
                                  AddScaled(state, k3, h),
                                  segment);

    for (std::size_t index = 0; index < state.size(); ++index) {
      const ComplexContourNumber weighted =
          k1[index] + k2[index] * two + k3[index] * two + k4[index];
      state[index] += weighted * h_over_six;
    }
  }
  return state;
}

ComplexContourVector PropagateWaypointsWithRk4(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const std::size_t steps_per_segment) {
  ComplexContourVector state = initial_values;
  for (std::size_t index = 1; index < waypoints.size(); ++index) {
    state = PropagateSegmentWithRk4(state,
                                    waypoints[index - 1],
                                    waypoints[index],
                                    matrix_evaluator,
                                    steps_per_segment);
  }
  return state;
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

std::string SerializePropagationForFingerprint(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourPropagationOptions& options,
    const std::size_t refined_steps_per_segment,
    const bool endpoint_extraction_applied) {
  std::ostringstream out;
  out << "kind=b61n-complex-contour-propagator\n";
  out << "dimension=" << initial_values.size() << "\n";
  out << "half_plane=" << ToString(options.half_plane) << "\n";
  out << "branch_policy=" << options.branch_policy << "\n";
  out << "matrix_fingerprint=" << options.matrix_fingerprint << "\n";
  out << "endpoint_target=eta=0\n";
  out << "endpoint_local_model_kind=" << options.endpoint_local_model_kind << "\n";
  out << "working_precision_digits=" << options.working_precision_digits << "\n";
  out << "refinement_error_tolerance_abs="
      << CompactFloat(options.refinement_error_tolerance) << "\n";
  out << "steps_per_segment=" << options.steps_per_segment << "\n";
  out << "refined_steps_per_segment=" << refined_steps_per_segment << "\n";
  out << "retained_solution_samples_used=false\n";
  out << "endpoint_extraction_applied="
      << (endpoint_extraction_applied ? "true" : "false") << "\n";
  out << "full_eta_zero_contour_applied=false\n";
  for (std::size_t index = 0; index < waypoints.size(); ++index) {
    out << "waypoint[" << index << "]=" << CompactComplex(waypoints[index]) << "\n";
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
  result.diagnostics.branch_policy = options.branch_policy;
  result.diagnostics.endpoint_target = "eta=0";
  result.diagnostics.endpoint_local_model_kind = options.endpoint_local_model_kind;
  result.diagnostics.matrix_fingerprint = options.matrix_fingerprint;
  result.diagnostics.refinement_error_tolerance_abs =
      CompactFloat(options.refinement_error_tolerance, 40);
  result.diagnostics.failure_code = failure_code;
  result.diagnostics.summary = summary;
  return result;
}

bool AppliesRegularTaylorR0EndpointExtraction(
    const ComplexContourVector& endpoint_values,
    const ComplexContourPropagationOptions& options) {
  return endpoint_values.size() == 1 &&
         options.endpoint_local_model_kind == "regular-taylor-r0";
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
    ComplexContourVector previous =
        PropagateWaypointsWithRk4(initial_values,
                                  waypoints,
                                  matrix_evaluator,
                                  options.steps_per_segment);
    RequireFiniteVector(previous, "coarse");

    const std::size_t first_required_refinement =
        std::max<std::size_t>(1, options.refinement_doublings);
    ComplexContourVector refined = previous;
    ComplexContourFloat refinement_error =
        std::numeric_limits<ComplexContourFloat>::infinity();
    std::size_t refined_steps_per_segment = options.steps_per_segment;
    std::size_t refinement_doublings_used = 0;
    bool refinement_passed = false;
    for (std::size_t doubling = 1; doubling <= options.max_refinement_doublings;
         ++doubling) {
      refined_steps_per_segment = options.steps_per_segment;
      for (std::size_t level = 0; level < doubling; ++level) {
        refined_steps_per_segment *= 2;
      }
      refined = PropagateWaypointsWithRk4(initial_values,
                                          waypoints,
                                          matrix_evaluator,
                                          refined_steps_per_segment);
      RequireFiniteVector(refined, "refined");
      refinement_error = MaxVectorDifference(previous, refined);
      previous = refined;
      refinement_doublings_used = doubling;
      if (doubling >= first_required_refinement &&
          refinement_error <= options.refinement_error_tolerance) {
        refinement_passed = true;
        break;
      }
    }
    if (!refinement_passed) {
      return FailureResult(
          "refinement-tolerance-failed",
          "b61n complex contour propagation failed closed because RK4 refinement error " +
              CompactFloat(refinement_error, 40) + " exceeded tolerance " +
              CompactFloat(options.refinement_error_tolerance, 40),
          initial_values,
          waypoints,
          options);
    }

    ComplexContourPropagationResult result;
    result.success = true;
    result.endpoint_values = refined;
    const bool endpoint_extraction_applied =
        AppliesRegularTaylorR0EndpointExtraction(result.endpoint_values, options);
    result.diagnostics.success = true;
    result.diagnostics.ode_propagation_applied = true;
    result.diagnostics.coefficient_publication = false;
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
    result.diagnostics.working_precision_digits = options.working_precision_digits;
    result.diagnostics.half_plane = options.half_plane;
    result.diagnostics.eta_zero_endpoint_reached = true;
    result.diagnostics.runtime_application =
        "b61n-complex-contour-propagator-harness";
    result.diagnostics.transport_scope =
        "lower-half-plane-complex-ode-vector-propagation";
    result.diagnostics.branch_policy = options.branch_policy;
    result.diagnostics.endpoint_target = "eta=0";
    result.diagnostics.endpoint_local_model_kind = options.endpoint_local_model_kind;
    result.diagnostics.matrix_fingerprint = options.matrix_fingerprint;
    result.diagnostics.refinement_error_abs = CompactFloat(refinement_error, 40);
    result.diagnostics.refinement_error_tolerance_abs =
        CompactFloat(options.refinement_error_tolerance, 40);
    result.diagnostics.contour_fingerprint = ComputeArtifactFingerprint(
        SerializePropagationForFingerprint(initial_values,
                                           waypoints,
                                           options,
                                           refined_steps_per_segment,
                                           endpoint_extraction_applied));
    result.diagnostics.summary =
        "Propagated a b61n complex ODE vector over " +
        std::to_string(result.diagnostics.segment_count) +
        " lower-half-plane contour segment(s) with dimension " +
        std::to_string(result.diagnostics.dimension) +
        "; ode_propagation_applied=true; coefficient_publication=false; "
        "final_solution_samples_used_as_input=false; full_eta_zero_contour_applied=false; "
        "endpoint_target=eta=0; eta_zero_endpoint_reached=true; "
        "endpoint_local_model_kind=" + result.diagnostics.endpoint_local_model_kind +
        "; endpoint_extraction_applied=" +
        (result.diagnostics.endpoint_extraction_applied ? std::string("true")
                                                        : std::string("false")) +
        "; "
        "matrix_fingerprint=" + result.diagnostics.matrix_fingerprint +
        "; working_precision_digits=" +
        std::to_string(result.diagnostics.working_precision_digits) +
        "; refinement_doublings_used=" +
        std::to_string(result.diagnostics.refinement_doublings_used) +
        "; refinement_error_tolerance_abs=" +
        result.diagnostics.refinement_error_tolerance_abs +
        "; "
        "refinement_error_abs=" + result.diagnostics.refinement_error_abs +
        "; contour_fingerprint=" + result.diagnostics.contour_fingerprint + ".";
    return result;
  } catch (const std::exception& error) {
    return FailureResult("propagation-failed",
                         std::string("b61n complex contour propagation failed closed: ") +
                             error.what(),
                         initial_values,
                         waypoints,
                         options);
  }
}

}  // namespace amflow
