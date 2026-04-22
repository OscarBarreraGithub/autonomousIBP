#pragma once

#include <string>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

enum class PhysicalKinematicsGuardrailVerdict {
  NotApplicable,
  SupportedReviewedSubset,
  UnsupportedSurface
};

struct PhysicalKinematicsGuardrailAssessment {
  PhysicalKinematicsGuardrailVerdict verdict =
      PhysicalKinematicsGuardrailVerdict::NotApplicable;
  std::string reviewed_subset;
};

std::string DescribeReviewedPhysicalKinematicsSubset();

PhysicalKinematicsGuardrailAssessment AssessPhysicalKinematicsForBatch62(
    const ProblemSpec& spec);

}  // namespace amflow
