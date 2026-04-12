#pragma once

#include <string>

namespace amflow {

enum class PrecisionStatus {
  Accepted,
  Escalate,
  Rejected
};

struct PrecisionObservation {
  int requested_digits = 50;
  int stable_digits = 0;
  double residual_estimate = 1.0;
  double overlap_mismatch = 1.0;
  double alternative_path_difference = 1.0;
};

struct PrecisionDecision {
  PrecisionStatus status = PrecisionStatus::Escalate;
  int suggested_working_precision = 120;
  int suggested_x_order = 120;
  std::string reason;
};

struct PrecisionPolicy {
  int working_precision = 100;
  int chop_precision = 20;
  int rationalize_precision = 100;
  int escalation_step = 20;
  int max_working_precision = 400;
  int x_order = 100;
  int x_order_step = 20;
};

PrecisionDecision EvaluatePrecision(const PrecisionPolicy& policy,
                                    const PrecisionObservation& observation);
std::string DescribePrecisionPolicy(const PrecisionPolicy& policy);

}  // namespace amflow
