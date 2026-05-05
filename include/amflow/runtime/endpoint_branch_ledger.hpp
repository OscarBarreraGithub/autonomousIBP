#pragma once

#include <optional>
#include <string>

#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/continuation_path.hpp"
#include "amflow/runtime/endpoint_local_model.hpp"

namespace amflow {

struct EtaEndpointBranchLedger {
  std::string eta_symbol;
  std::string endpoint_expression;
  ExactComplexRational endpoint_value;
  EtaContourHalfPlane half_plane = EtaContourHalfPlane::Upper;
  FeynmanPrescription prescription = FeynmanPrescription::None;
  std::string prescription_source;
  std::string approach_direction;
  std::string log_branch_argument;
  int log_sheet_index = 0;
  int endpoint_branch_winding = 0;
  std::string contour_fingerprint;
  std::string local_model_kind;
  int extraction_order = 0;
  bool live_endpoint_extraction_ready = false;
  std::string ledger_fingerprint;
};

struct EtaEndpointBranchLedgerAnalysis {
  bool success = false;
  std::string failure_code;
  std::string summary;
  std::optional<EtaEndpointBranchLedger> ledger;
};

std::string ToString(FeynmanPrescription prescription);

EtaEndpointBranchLedgerAnalysis AnalyzeEtaEndpointBranchLedger(
    const ProblemSpec& spec,
    const EtaContinuationPlan& plan,
    const EtaEndpointLocalModel& local_model);

}  // namespace amflow
