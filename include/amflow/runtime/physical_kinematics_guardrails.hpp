#pragma once

#include <string>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

enum class PhysicalKinematicsGuardrailVerdict {
  NotApplicable,
  SupportedReviewedSubset,
  SingularSurface,
  UnsupportedSurface
};

struct PhysicalKinematicsGuardrailAssessment {
  PhysicalKinematicsGuardrailVerdict verdict =
      PhysicalKinematicsGuardrailVerdict::NotApplicable;
  std::string reviewed_subset;
  std::string detail;
};

std::string DescribeReviewedPhysicalKinematicsSubset();

PhysicalKinematicsGuardrailAssessment AssessPhysicalKinematicsForBatch62(
    const ProblemSpec& spec);

}  // namespace amflow
