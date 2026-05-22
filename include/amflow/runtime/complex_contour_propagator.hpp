#pragma once

#include <cstddef>
#include <complex>
#include <functional>
#include <string>
#include <vector>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include "amflow/runtime/continuation_path.hpp"

namespace amflow {

using ComplexContourFloat = boost::multiprecision::cpp_dec_float_100;
using ComplexContourNumber = std::complex<ComplexContourFloat>;
using ComplexContourVector = std::vector<ComplexContourNumber>;
using ComplexContourMatrix = std::vector<std::vector<ComplexContourNumber>>;
using ComplexContourMatrixEvaluator =
    std::function<ComplexContourMatrix(const ComplexContourNumber& eta)>;

struct ComplexContourPropagationOptions {
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower;
  std::size_t steps_per_segment = 32;
  std::size_t refinement_doublings = 1;
  std::size_t max_refinement_doublings = 4;
  std::size_t working_precision_digits = 100;
  ComplexContourFloat refinement_error_tolerance = ComplexContourFloat("1e-60");
  ComplexContourFloat refinement_relative_error_tolerance = 0;
  std::string matrix_fingerprint;
  std::string endpoint_integral_id;
  std::string endpoint_local_model_kind = "regular-taylor-r0-pending-laurent-fit";
  std::string branch_policy = "NegIm lower-half-plane contour supplied by caller";
};

struct ComplexContourPropagationDiagnostics {
  bool success = false;
  bool ode_propagation_applied = false;
  bool coefficient_publication = false;
  bool retained_solution_samples_used = false;
  bool full_eta_zero_contour_applied = false;
  bool endpoint_extraction_applied = false;
  std::size_t dimension = 0;
  std::size_t waypoint_count = 0;
  std::size_t segment_count = 0;
  std::size_t coarse_step_count = 0;
  std::size_t refined_step_count = 0;
  std::size_t refinement_doublings_used = 0;
  std::size_t working_precision_digits = 0;
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Lower;
  bool eta_zero_endpoint_reached = false;
  std::string runtime_application;
  std::string transport_scope;
  std::string branch_policy;
  std::string endpoint_target;
  std::string endpoint_integral_id;
  std::string endpoint_local_model_kind;
  std::string matrix_fingerprint;
  std::string contour_fingerprint;
  std::string endpoint_vector_norm_abs;
  std::string refinement_error_abs;
  std::string refinement_error_tolerance_abs;
  std::string refinement_error_tolerance_rel;
  std::string refinement_effective_tolerance_abs;
  std::string failure_code;
  std::string summary;
};

struct ComplexContourPropagationResult {
  bool success = false;
  ComplexContourVector endpoint_values;
  ComplexContourPropagationDiagnostics diagnostics;
};

ComplexContourPropagationResult PropagateComplexContourVector(
    const ComplexContourVector& initial_values,
    const std::vector<ComplexContourNumber>& waypoints,
    const ComplexContourMatrixEvaluator& matrix_evaluator,
    const ComplexContourPropagationOptions& options = {});

}  // namespace amflow
