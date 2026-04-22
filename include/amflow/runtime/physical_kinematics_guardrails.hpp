#pragma once

#include <string>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

enum class PhysicalKinematicsGuardrailVerdict {
  NotApplicable,
  SupportedReviewedSubset,
  SingularSurface,
  NearSingularSurface,
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

PhysicalKinematicsGuardrailAssessment
AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
    const ProblemSpec& spec,
    const std::string& invariant_name,
    const std::string& start_location,
    const std::string& target_location,
    bool allow_unlabeled_reviewed_raw_expressions);

PhysicalKinematicsGuardrailAssessment
AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
    const ProblemSpec& spec,
    const std::string& start_location,
    const std::string& target_location,
    bool allow_unlabeled_reviewed_raw_expressions);

PhysicalKinematicsGuardrailAssessment
AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(
    const ProblemSpec& spec,
    const std::string& start_location,
    const std::string& target_location);

}  // namespace amflow
