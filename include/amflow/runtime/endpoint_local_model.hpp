#pragma once

#include <optional>
#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/continuation_path.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

struct EtaEndpointLocalModel {
  std::string eta_symbol;
  std::string endpoint_expression;
  ExactComplexRational endpoint_value;
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Upper;
  std::string contour_fingerprint;
  std::string local_model_kind;
  int extraction_order = 0;
  bool branch_sensitive = true;
  bool live_endpoint_extraction_ready = false;
  std::vector<std::string> indicial_exponents;
  ExactRationalMatrix residue_matrix;
  std::vector<std::string> basis_functions;
};

struct EtaEndpointLocalModelAnalysis {
  bool success = false;
  std::string failure_code;
  std::string summary;
  std::optional<EtaEndpointLocalModel> model;
};

EtaEndpointLocalModelAnalysis AnalyzeEtaEndpointLocalModel(
    const DESystem& system,
    const ProblemSpec& spec,
    const EtaContinuationPlan& plan,
    int extraction_order = 4);

}  // namespace amflow
