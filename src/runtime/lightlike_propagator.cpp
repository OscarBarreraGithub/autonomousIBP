#include "amflow/runtime/lightlike_propagator.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace amflow {

namespace {

std::string RemoveAsciiSpaces(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](const unsigned char current) {
                               return std::isspace(current) != 0;
                             }),
              value.end());
  return value;
}

std::string IntegralLabel(const std::string& family, const std::vector<int>& indices) {
  std::ostringstream out;
  out << family << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index != 0) {
      out << ",";
    }
    out << indices[index];
  }
  out << "]";
  return out.str();
}

std::string MasterLabel(const MasterIntegral& master) {
  if (!master.label.empty()) {
    return master.label;
  }
  return IntegralLabel(master.family, master.indices);
}

std::string TargetLabel(const TargetIntegral& target) {
  return IntegralLabel(target.family, target.indices);
}

bool VectorEquals(const std::vector<std::string>& lhs,
                  const std::vector<std::string>& rhs) {
  return lhs == rhs;
}

bool HasBoundaryFile(const LightlikeGaugeLinkRuntimeState& state,
                     const std::string& name) {
  return std::find(state.boundary_file_names.begin(),
                   state.boundary_file_names.end(),
                   name) != state.boundary_file_names.end();
}

bool HasCanonicalSingularPoint(const LightlikeGaugeLinkRuntimeState& state,
                               const std::string& point) {
  const std::string canonical = RemoveAsciiSpaces(point);
  return std::any_of(state.singular_points.begin(),
                     state.singular_points.end(),
                     [&canonical](const std::string& candidate) {
                       return RemoveAsciiSpaces(candidate) == canonical;
                     });
}

std::vector<std::string> ExternalMomenta(const ProblemSpec& spec) {
  std::vector<std::string> momenta = spec.kinematics.incoming_momenta;
  momenta.insert(momenta.end(),
                 spec.kinematics.outgoing_momenta.begin(),
                 spec.kinematics.outgoing_momenta.end());
  return momenta;
}

bool HasGaugeLinkReplacement(const ProblemSpec& spec) {
  for (const ScalarProductRule& rule : spec.kinematics.scalar_product_rules) {
    const std::string lhs = RemoveAsciiSpaces(rule.left);
    const std::string rhs = RemoveAsciiSpaces(rule.right);
    if ((lhs == "n^2" || lhs == "n*n" || lhs == "n.n") && rhs == "-1") {
      return true;
    }
  }
  return false;
}

const std::vector<std::string>& ReviewedPropagators() {
  static const std::vector<std::string> propagators = {
      "l1^2",
      "l2^2",
      "l3^2",
      "1+l1*n",
      "1/2+l1*n+l2*n+l3*n",
      "l1^2+2*l1*l2+l2^2",
      "l1^2+2*l1*l3+l3^2",
      "l2^2+2*l2*l3+l3^2",
      "-1+l2^2+2*l2*n",
  };
  return propagators;
}

const std::vector<std::string>& GeneratedSquarePropagators() {
  static const std::vector<std::string> propagators = {
      "l1^2",
      "l2^2",
      "l3^2",
      "l1^2 + (1 + l1*n)/gaugex",
      "(l1 + l2 + l3)^2 + (1/2 + l1*n + l2*n + l3*n)/gaugex",
      "l1^2 + 2*l1*l2 + l2^2",
      "l1^2 + 2*l1*l3 + l3^2",
      "l2^2 + 2*l2*l3 + l3^2",
      "-1 + l2^2 + 2*l2*n",
  };
  return propagators;
}

const std::vector<std::size_t>& AffectedPropagatorIndices() {
  static const std::vector<std::size_t> indices = {3, 4};
  return indices;
}

const std::vector<std::string>& ReviewedTargetLabels() {
  static const std::vector<std::string> labels = {
      "gauge[1,1,1,1,1,0,-1,0,0]",
      "gauge[1,1,1,1,1,0,0,-1,0]",
      "gauge[1,1,1,1,1,0,0,0,-1]",
      "gauge[1,1,1,0,1,0,0,0,0]",
      "gauge[1,1,1,-1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,-1,0,0,0]",
      "gauge[1,1,1,1,1,0,0,0,0]",
      "gauge[1,1,1,1,1,-1,0,0,0]",
  };
  return labels;
}

const std::vector<std::string>& ReviewedReductionMasterLabels() {
  static const std::vector<std::string> labels = {
      "gauge[1,1,1,0,1,0,0,0,0]",
      "gauge[1,1,1,-1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,0,0,0,0]",
      "gauge[0,1,1,1,1,-1,0,0,0]",
      "gauge[1,1,1,1,1,0,0,0,0]",
      "gauge[1,1,1,1,1,-1,0,0,0]",
  };
  return labels;
}

bool LabelsExactlyMatch(const std::vector<TargetIntegral>& values,
                        const std::vector<std::string>& expected) {
  if (values.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (TargetLabel(values[index]) != expected[index]) {
      return false;
    }
  }
  return true;
}

bool LabelsExactlyMatch(const std::vector<MasterIntegral>& values,
                        const std::vector<std::string>& expected) {
  if (values.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (MasterLabel(values[index]) != expected[index]) {
      return false;
    }
  }
  return true;
}

bool LabelsContainAll(const std::vector<MasterIntegral>& haystack,
                      const std::vector<MasterIntegral>& needles) {
  std::set<std::string> labels;
  for (const MasterIntegral& master : haystack) {
    labels.insert(MasterLabel(master));
  }
  return std::all_of(needles.begin(),
                     needles.end(),
                     [&labels](const MasterIntegral& master) {
                       return labels.find(MasterLabel(master)) != labels.end();
                     });
}

void RequireReviewedGaugeLinkSourceSurface(const ProblemSpec& spec) {
  if (spec.family.name != "gauge") {
    throw std::runtime_error(
        "b64ag gauge-link scaffold is intentionally limited to family gauge");
  }
  if (!VectorEquals(spec.family.loop_momenta, {"l1", "l2", "l3"})) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires loop momenta {l1,l2,l3}");
  }
  if (!VectorEquals(ExternalMomenta(spec), {"n"})) {
    throw std::runtime_error("b64ag gauge-link scaffold requires external leg {n}");
  }
  if (!HasGaugeLinkReplacement(spec)) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires AMFlow replacement n^2 -> -1");
  }
  if (spec.family.propagators.size() != ReviewedPropagators().size()) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires the exact nine-propagator source surface");
  }
  for (std::size_t index = 0; index < ReviewedPropagators().size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (RemoveAsciiSpaces(propagator.expression) != ReviewedPropagators()[index]) {
      throw std::runtime_error(
          "b64ag gauge-link scaffold rejects non-reviewed denominator at propagator " +
          std::to_string(index));
    }
    if (RemoveAsciiSpaces(propagator.mass) != "0") {
      throw std::runtime_error(
          "b64ag gauge-link scaffold requires zero mass metadata on propagator " +
          std::to_string(index));
    }
    if (propagator.kind == PropagatorKind::Cut) {
      throw std::runtime_error(
          "b64ag gauge-link scaffold rejects cut propagators on the loop gauge-link surface");
    }
  }
  if (!spec.targets.empty() && !LabelsExactlyMatch(spec.targets, ReviewedTargetLabels())) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold requires the reviewed nine-target retained packet surface");
  }
}

std::string NormalizationFactor(const std::string& variable, const int affected_power_sum) {
  const int exponent = -affected_power_sum;
  if (exponent == 0) {
    return "1";
  }
  return variable + "^(" + std::to_string(exponent) + ")";
}

LightlikeGaugeLinkRuntimeState RequireRuntimeState(
    const LightlikeGaugeLinkRuntimeState& state) {
  if (!IsLightlikeGaugeLinkEtaZeroRuntimeState(state)) {
    throw std::runtime_error(
        "b64ag gauge-link scaffold was invoked for a non-linear_propagator gaugex=0 state");
  }
  return state;
}

}  // namespace

bool IsLightlikeGaugeLinkEtaZeroRuntimeState(
    const LightlikeGaugeLinkRuntimeState& state) {
  if (!state.amflow_state_input || state.benchmark_id != "linear_propagator" ||
      state.family != "gauge" || state.integral_kind != "loop" ||
      state.variable != "gaugex" ||
      RemoveAsciiSpaces(state.start_location) != "gaugex->1/40" ||
      RemoveAsciiSpaces(state.target_location) != "gaugex=0" ||
      state.boundary_state_kind != "amflow_finite_solution_samples" ||
      !HasCanonicalSingularPoint(state, "gaugex=0")) {
    return false;
  }
  for (const std::string& required_file : {"boundary", "diffeq", "reduction", "solve.wl"}) {
    if (!HasBoundaryFile(state, required_file)) {
      return false;
    }
  }
  if (!state.boundary_point.empty() &&
      RemoveAsciiSpaces(state.boundary_point) != "gaugex->1/40") {
    return false;
  }
  if (!state.diffeq_variables.empty() &&
      !VectorEquals(state.diffeq_variables, {"gaugex"})) {
    return false;
  }
  return true;
}

ProblemSpec MakeReviewedLightlikeGaugeLinkProblemSpec() {
  ProblemSpec spec;
  spec.family.name = "gauge";
  spec.family.loop_momenta = {"l1", "l2", "l3"};
  for (const std::string& expression : ReviewedPropagators()) {
    spec.family.propagators.push_back(Propagator(expression));
  }
  spec.kinematics.incoming_momenta = {"n"};
  spec.kinematics.scalar_product_rules = {{"n^2", "-1"}};
  spec.kinematics.invariants = {"n2"};
  for (const std::string& label : ReviewedTargetLabels()) {
    const std::size_t open = label.find('[');
    const std::size_t close = label.find(']');
    TargetIntegral target;
    target.family = "gauge";
    std::string number;
    for (std::size_t index = open + 1; index < close; ++index) {
      const char current = label[index];
      if (current == ',') {
        target.indices.push_back(std::stoi(number));
        number.clear();
      } else {
        number.push_back(current);
      }
    }
    if (!number.empty()) {
      target.indices.push_back(std::stoi(number));
    }
    spec.targets.push_back(std::move(target));
  }
  return spec;
}

LightlikeGaugeLinkSquareFamilyResult BuildLightlikeGaugeLinkSquareFamily(
    const ProblemSpec& spec,
    const std::string& variable) {
  if (RemoveAsciiSpaces(variable) != "gaugex") {
    throw std::runtime_error(
        "b64ag gauge-link scaffold is reviewed only for generated variable gaugex");
  }
  RequireReviewedGaugeLinkSourceSurface(spec);

  LightlikeGaugeLinkSquareFamilyResult result;
  result.transformed_spec = spec;
  result.variable = "gaugex";
  result.affected_propagator_indices = AffectedPropagatorIndices();
  result.generated_square_propagators = GeneratedSquarePropagators();
  for (std::size_t index = 0; index < result.generated_square_propagators.size(); ++index) {
    result.transformed_spec.family.propagators[index].expression =
        result.generated_square_propagators[index];
    result.transformed_spec.family.propagators[index].kind = PropagatorKind::Standard;
    result.transformed_spec.family.propagators[index].variant = PropagatorVariant::Quadratic;
  }
  return result;
}

std::vector<LightlikeGaugeLinkTargetNormalization>
ApplyLightlikeGaugeLinkPowerNormalization(
    const std::vector<TargetIntegral>& targets,
    const std::vector<std::size_t>& affected_propagator_indices,
    const std::string& variable) {
  std::vector<LightlikeGaugeLinkTargetNormalization> normalizations;
  normalizations.reserve(targets.size());
  for (const TargetIntegral& target : targets) {
    int affected_power_sum = 0;
    for (const std::size_t propagator_index : affected_propagator_indices) {
      if (propagator_index >= target.indices.size()) {
        throw std::runtime_error(
            "b64ag gauge-link power normalization found a target with too few indices");
      }
      affected_power_sum += target.indices[propagator_index];
    }
    normalizations.push_back({TargetLabel(target),
                              affected_power_sum,
                              NormalizationFactor(variable, affected_power_sum)});
  }
  return normalizations;
}

LightlikeGaugeLinkFinitePartResult ExtractLightlikeGaugeLinkEndpointFinitePart(
    const std::vector<LightlikeGaugeLinkFinitePartTerm>& terms) {
  LightlikeGaugeLinkFinitePartResult result;
  if (terms.empty()) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction has no endpoint terms; this partial scaffold does not "
        "publish an implicit zero coefficient";
    return result;
  }
  std::set<std::string> region_keys;
  for (const LightlikeGaugeLinkFinitePartTerm& term : terms) {
    region_keys.insert(term.region_key.empty() ? "integer" : term.region_key);
    if (term.log_power != 0) {
      result.failure_code = "continuation_budget_exhausted";
      result.summary =
          "b64ag finite-part extraction rejects unresolved logarithmic endpoint structure";
      return result;
    }
  }
  if (region_keys.size() > 1) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction rejects multiple integer endpoint regions";
    return result;
  }
  const auto min_power_it =
      std::min_element(terms.begin(),
                       terms.end(),
                       [](const LightlikeGaugeLinkFinitePartTerm& lhs,
                          const LightlikeGaugeLinkFinitePartTerm& rhs) {
                         return lhs.power < rhs.power;
                       });
  if (min_power_it != terms.end() && min_power_it->power > 0) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction found a positive starting power; this partial scaffold "
        "does not publish an implicit zero coefficient";
    return result;
  }

  const auto zero_it =
      std::find_if(terms.begin(),
                   terms.end(),
                   [](const LightlikeGaugeLinkFinitePartTerm& term) {
                     return term.power == 0;
                   });
  for (const LightlikeGaugeLinkFinitePartTerm& term : terms) {
    if (term.power < 0) {
      result.dropped_singular_terms.push_back(term.coefficient);
    }
  }
  if (zero_it == terms.end()) {
    result.failure_code = "continuation_budget_exhausted";
    result.summary =
        "b64ag finite-part extraction did not find a power-zero coefficient; this partial "
        "scaffold does not publish an implicit zero coefficient";
    return result;
  }
  result.success = true;
  result.ir_subtraction_applied = true;
  result.finite_part_coefficient = zero_it->coefficient;
  result.summary =
      result.dropped_singular_terms.empty()
          ? "b64ag finite-part extraction selected the endpoint power-zero coefficient"
          : "b64ag finite-part extraction dropped singular endpoint powers and selected the "
            "power-zero coefficient";
  return result;
}

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkRetainedStateScaffold(
    const LightlikeGaugeLinkRuntimeState& state) {
  const LightlikeGaugeLinkRuntimeState checked_state = RequireRuntimeState(state);
  if (!checked_state.targets.empty() &&
      !LabelsExactlyMatch(checked_state.targets, ReviewedTargetLabels())) {
    throw std::runtime_error(
        "b64ag gauge-link state target surface drifted from the reviewed nine-target packet");
  }
  if (checked_state.targets.empty()) {
    throw std::runtime_error(
        "boundary_unsolved: b64ag gauge-link retained state is missing reviewed targets");
  }
  if (checked_state.reduction_masters.empty() || checked_state.diffeq_masters.empty()) {
    throw std::runtime_error(
        "master_set_instability: b64ag gauge-link retained state is missing reduced or DE "
        "masters");
  }
  if (!checked_state.reduction_masters.empty() &&
      !LabelsExactlyMatch(checked_state.reduction_masters, ReviewedReductionMasterLabels())) {
    throw std::runtime_error(
        "master_set_instability: b64ag gauge-link reduced masters drifted from the reviewed "
        "surface");
  }

  LightlikeGaugeLinkTransportAudit audit;
  audit.reviewed_surface = true;
  audit.live_coefficients_available = false;
  audit.runtime_scaffold_consumes_retained_solution_samples = false;
  audit.retained_solution_samples_available =
      checked_state.solution_sample_cache_enabled && HasBoundaryFile(checked_state, "solution");
  audit.full_eta_zero_contour_applied = false;
  audit.ir_subtraction_applied = false;
  audit.boundary_data_available =
      HasBoundaryFile(checked_state, "boundary") && HasBoundaryFile(checked_state, "diffeq") &&
      HasBoundaryFile(checked_state, "reduction");
  audit.diffeq_masters_cover_reduction_masters =
      !checked_state.diffeq_masters.empty() && !checked_state.reduction_masters.empty() &&
      LabelsContainAll(checked_state.diffeq_masters, checked_state.reduction_masters);
  audit.family = checked_state.family;
  audit.variable = checked_state.variable;
  audit.desolver_local_variable = "eta";
  audit.boundary_point =
      checked_state.boundary_point.empty() ? checked_state.start_location
                                           : checked_state.boundary_point;
  audit.target_point = checked_state.target_location;
  audit.singular_endpoint = "gaugex=0";
  audit.runtime_application =
      "partial-gauge-link-gaugex-zero-contour-scaffold-no-coefficients";
  audit.provider_strategy = "builtin::b64ag-gauge-link-retained-state-scaffold";
  audit.endpoint_selection_rule =
      "PickZeroRuleS-compatible finite-part coefficient selection is validated only as a "
      "local rule in this scaffold";
  audit.coefficient_gap =
      "Live gauge-link endpoint coefficients are not implemented in this scaffold; retained "
      "final solution samples remain legacy evidence only, and full_eta_zero_contour_applied "
      "must remain false until gaugex=0 transport produces the coefficients.";
  audit.failure_code_contract =
      "boundary_unsolved, master_set_instability, continuation_budget_exhausted, "
      "insufficient_precision";
  audit.affected_propagator_indices = AffectedPropagatorIndices();
  audit.target_normalizations =
      ApplyLightlikeGaugeLinkPowerNormalization(checked_state.targets,
                                                audit.affected_propagator_indices,
                                                checked_state.variable);
  audit.pole_candidates = {"gaugex=0", "gaugex=-1/2", "gaugex=-1/4"};
  audit.boundary_file_names = checked_state.boundary_file_names;
  audit.epsilon_sample_count = checked_state.epsilon_samples.size();
  audit.master_count = checked_state.masters.size();
  audit.reduction_master_count = checked_state.reduction_masters.size();
  audit.target_count = checked_state.targets.size();
  audit.summary =
      "b64ag gauge-link scaffold recognized the retained linear_propagator gaugex=0 "
      "state metadata, audited the reviewed nine-target packet surface and D4,D5 power "
      "normalization, and kept live coefficient publication deferred.";
  if (audit.retained_solution_samples_available) {
    audit.summary +=
        " The retained state still carries final solution samples, but this scaffold does not "
        "consume them as runtime evidence.";
  }
  if (!audit.diffeq_masters_cover_reduction_masters) {
    throw std::runtime_error(
        "master_set_instability: b64ag gauge-link DE masters do not contain reduced masters");
  }
  return audit;
}

LightlikeGaugeLinkTransportAudit BuildLightlikeGaugeLinkTransportScaffold(
    const ProblemSpec& spec,
    const LightlikeGaugeLinkRuntimeState& state) {
  LightlikeGaugeLinkTransportAudit audit =
      BuildLightlikeGaugeLinkRetainedStateScaffold(state);
  const LightlikeGaugeLinkSquareFamilyResult square =
      BuildLightlikeGaugeLinkSquareFamily(spec, audit.variable);
  audit.generated_square_propagators = square.generated_square_propagators;
  audit.summary =
      "b64ag gauge-link scaffold recognized the reviewed linear_propagator gaugex=0 "
      "source surface, generated the AMFlow square family for affected propagators D4,D5, "
      "audited target-row gaugex power normalization, and kept live coefficient publication "
      "deferred.";
  if (audit.retained_solution_samples_available) {
    audit.summary +=
        " The retained state still carries final solution samples, but this scaffold does not "
        "consume them as runtime evidence.";
  }
  return audit;
}

}  // namespace amflow
