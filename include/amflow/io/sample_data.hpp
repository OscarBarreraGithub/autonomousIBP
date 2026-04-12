#pragma once

#include "amflow/core/de_system.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/solver/boundary_request.hpp"

namespace amflow {

ProblemSpec MakeSampleProblemSpec();
DESystem MakeSampleDESystem();
BoundaryRequest MakeSampleBoundaryRequest();
BoundaryCondition MakeSampleBoundaryCondition();
ArtifactManifest MakeBootstrapManifest();

}  // namespace amflow
