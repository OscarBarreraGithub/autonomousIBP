#include "amflow/io/sample_data.hpp"

#include "amflow/core/options.hpp"

namespace amflow {

ProblemSpec MakeSampleProblemSpec() {
  ProblemSpec spec;
  spec.family.name = "planar_double_box";
  spec.family.loop_momenta = {"k1", "k2"};
  spec.family.top_level_sectors = {127};
  spec.family.preferred_masters = {"planar_double_box[1,1,1,1,1,1,1]"};
  spec.family.propagators = {
      {"(k1)^2", "0", PropagatorKind::Standard, -1},
      {"(k1-p1)^2", "0", PropagatorKind::Standard, -1},
      {"(k1-p1-p2)^2", "0", PropagatorKind::Standard, -1},
      {"(k2)^2", "0", PropagatorKind::Standard, -1},
      {"(k2-p3)^2", "0", PropagatorKind::Standard, -1},
      {"(k1-k2)^2", "0", PropagatorKind::Standard, -1},
      {"(k2-p1-p2)^2", "0", PropagatorKind::Standard, -1},
  };

  spec.kinematics.incoming_momenta = {"p1", "p2"};
  spec.kinematics.outgoing_momenta = {"p3", "p4"};
  spec.kinematics.momentum_conservation = "p1 + p2 + p3 + p4 = 0";
  spec.kinematics.invariants = {"s", "t", "msq"};
  spec.kinematics.scalar_product_rules = {
      {"p1*p1", "0"},
      {"p2*p2", "0"},
      {"p1*p2", "s/2"},
  };
  spec.kinematics.numeric_substitutions = {
      {"s", "30"},
      {"t", "-10/3"},
      {"msq", "1"},
  };

  spec.targets = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 1}},
  };
  spec.dimension = "4 - 2*eps";
  spec.complex_mode = false;
  spec.notes = "Bootstrap example aligned with the package-paper double-box family.";
  return spec;
}

DESystem MakeSampleDESystem() {
  DESystem system;
  system.masters = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 1}, "I1"},
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 0}, "I2"},
  };
  system.variables = {
      {"eta", DifferentiationVariableKind::Eta},
      {"s", DifferentiationVariableKind::Invariant},
  };
  system.coefficient_matrices["eta"] = {
      {"1/(eta-s)", "0"},
      {"0", "1/(eta-s)"},
  };
  system.coefficient_matrices["s"] = {
      {"0", "1/s"},
      {"1/s", "0"},
  };
  system.singular_points = {"eta=0", "eta=s"};
  return system;
}

BoundaryRequest MakeSampleBoundaryRequest() {
  BoundaryRequest request;
  request.variable = "eta";
  request.location = "infinity";
  request.strategy = "manual";
  return request;
}

BoundaryCondition MakeSampleBoundaryCondition() {
  BoundaryCondition condition;
  condition.variable = "eta";
  condition.location = "infinity";
  condition.values = {"B1", "B2"};
  condition.strategy = "manual";
  return condition;
}

ArtifactManifest MakeBootstrapManifest() {
  ArtifactManifest manifest;
  manifest.manifest_kind = "sample-demo";
  manifest.run_id = "sample-demo-manifest";
  manifest.spec_provenance = "builtin sample/demo ProblemSpec";
  manifest.spec_fingerprint = "sample-planar-double-box";
  manifest.family = "planar_double_box";
  manifest.target_count = 1;
  manifest.execution_status = "sample-demo-not-run";
  manifest.exit_code = 0;
  manifest.amflow_commit = "sample-demo";
  manifest.git_status_short = "sample-demo";
  manifest.threads = 0;
  return manifest;
}

}  // namespace amflow
