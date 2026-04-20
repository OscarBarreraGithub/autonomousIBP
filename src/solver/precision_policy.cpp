#include "amflow/solver/precision_policy.hpp"

#include <algorithm>
#include <sstream>

namespace amflow {

PrecisionDecision EvaluatePrecision(const PrecisionPolicy& policy,
                                    const PrecisionObservation& observation) {
  PrecisionDecision decision;
  decision.suggested_working_precision = policy.working_precision + policy.escalation_step;
  decision.suggested_x_order = policy.x_order + policy.x_order_step;

  const bool stable = observation.stable_digits >= observation.requested_digits &&
                      observation.residual_estimate <= 1e-8 &&
                      observation.overlap_mismatch <= 1e-8 &&
                      observation.alternative_path_difference <= 1e-8;

  if (stable) {
    decision.status = PrecisionStatus::Accepted;
    decision.suggested_working_precision = policy.working_precision;
    decision.suggested_x_order = policy.x_order;
    decision.reason = "observed stability satisfies the bootstrap policy";
    return decision;
  }

  if (policy.working_precision >= policy.max_working_precision) {
    decision.status = PrecisionStatus::Rejected;
    decision.reason =
        "insufficient_precision: precision ceiling reached before satisfying the stability "
        "checks";
    return decision;
  }

  decision.status = PrecisionStatus::Escalate;
  decision.reason = "raise working precision and truncation order";
  return decision;
}

PrecisionDecision EvaluatePrecisionBudget(const PrecisionPolicy& policy,
                                          const int requested_digits) {
  const int effective_working_precision =
      std::min(policy.working_precision, policy.max_working_precision);
  PrecisionDecision decision;
  decision.suggested_working_precision = effective_working_precision + policy.escalation_step;
  decision.suggested_x_order = policy.x_order + policy.x_order_step;

  if (requested_digits <= effective_working_precision) {
    decision.status = PrecisionStatus::Accepted;
    decision.suggested_working_precision = effective_working_precision;
    decision.suggested_x_order = policy.x_order;
    decision.reason = "requested digits are already covered by the current working precision";
    return decision;
  }

  if (requested_digits > policy.max_working_precision) {
    decision.status = PrecisionStatus::Rejected;
    decision.reason =
        "insufficient_precision: requested digits exceed the configured precision ceiling";
    return decision;
  }

  decision.status = PrecisionStatus::Escalate;
  decision.reason = "raise working precision and truncation order";
  return decision;
}

std::string DescribePrecisionPolicy(const PrecisionPolicy& policy) {
  std::ostringstream out;
  out << "working_precision=" << policy.working_precision
      << ", chop_precision=" << policy.chop_precision
      << ", rationalize_precision=" << policy.rationalize_precision
      << ", x_order=" << policy.x_order;
  return out.str();
}

std::string DescribeAmfSolveRuntimePolicy(const AmfSolveRuntimePolicy& policy) {
  std::ostringstream out;
  out << "extra_x_order=" << policy.extra_x_order
      << ", learn_x_order=" << policy.learn_x_order
      << ", test_x_order=" << policy.test_x_order
      << ", run_length=" << policy.run_length;
  return out.str();
}

}  // namespace amflow
