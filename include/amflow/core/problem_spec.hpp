#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace amflow {

enum class PropagatorKind {
  Standard,
  Cut,
  Linear,
  Auxiliary
};

std::string ToString(PropagatorKind kind);

enum class PropagatorVariant {
  Quadratic,
  Linear,
};

std::string ToString(PropagatorVariant variant);
std::optional<PropagatorVariant> ParsePropagatorVariantKeyword(const std::string& keyword);

enum class FeynmanPrescription {
  MinusI0 = -1,
  None = 0,
  PlusI0 = 1,
};

std::optional<FeynmanPrescription> ParseFeynmanPrescription(int raw_value);

struct Propagator {
  Propagator() = default;
  Propagator(std::string expression_in,
             std::string mass_in = "0",
             PropagatorKind kind_in = PropagatorKind::Standard,
             int prescription_in = -1,
             std::optional<PropagatorVariant> variant_in = std::nullopt)
      : expression(std::move(expression_in)),
        mass(std::move(mass_in)),
        kind(kind_in),
        prescription(prescription_in),
        variant(std::move(variant_in)) {}

  std::string expression;
  std::string mass = "0";
  PropagatorKind kind = PropagatorKind::Standard;
  int prescription = -1;
  std::optional<PropagatorVariant> variant;
};

PropagatorVariant EffectivePropagatorVariant(const Propagator& propagator);

struct ScalarProductRule {
  std::string left;
  std::string right;
};

struct TargetIntegral {
  std::string family;
  std::vector<int> indices;

  std::string Label() const;
};

struct FamilyDefinition {
  std::string name;
  std::vector<std::string> loop_momenta;
  std::vector<FeynmanPrescription> loop_prescriptions;
  std::vector<int> top_level_sectors;
  std::vector<Propagator> propagators;
  std::vector<std::string> preferred_masters;
};

std::optional<FeynmanPrescription> DerivePropagatorPrescriptionFromLoopPrescriptions(
    const FamilyDefinition& family,
    const Propagator& propagator);

struct Kinematics {
  std::vector<std::string> incoming_momenta;
  std::vector<std::string> outgoing_momenta;
  std::string momentum_conservation;
  std::vector<std::string> invariants;
  std::vector<ScalarProductRule> scalar_product_rules;
  std::map<std::string, std::string> numeric_substitutions;
  std::map<std::string, std::string> complex_numeric_substitutions;
};

struct ProblemSpec {
  FamilyDefinition family;
  Kinematics kinematics;
  std::vector<TargetIntegral> targets;
  std::string dimension = "4 - 2*eps";
  bool complex_mode = false;
  std::string notes;
};

enum class AmflowLoopPrefactorSign {
  PlusI0,
  MinusI0
};

std::string ToString(AmflowLoopPrefactorSign sign);

// Default literals are frozen by the Batch 58c evidence packet:
// specs/amflow-prefactor-reference.yaml is the structured lock, and
// references/snapshots/amflow/prefactor_convention_lock.md is the
// human-readable mirror. The retained phase-0 README backs the +i0 loop and
// cut factors; the explicit -i0 loop factor remains backed only by the
// repo-local AMFlow snapshot README.
struct AmflowPrefactorConvention {
  AmflowPrefactorConvention()
      : plus_i0_loop_prefactor("1/(I*pi^(D/2))"),
        minus_i0_loop_prefactor("-1/(I*pi^(D/2))"),
        cut_prefactor("delta_+(p^2-m^2)/(2*pi)^(D-1)"),
        loop_sign(AmflowLoopPrefactorSign::PlusI0) {}

  std::string plus_i0_loop_prefactor;
  std::string minus_i0_loop_prefactor;
  std::string cut_prefactor;
  AmflowLoopPrefactorSign loop_sign;
};

std::vector<std::string> ValidateProblemSpec(const ProblemSpec& spec);
std::string SerializeProblemSpecYaml(const ProblemSpec& spec);
std::string BuildOverallAmflowPrefactor(
    const ProblemSpec& spec,
    const AmflowPrefactorConvention& convention = AmflowPrefactorConvention{});

}  // namespace amflow
