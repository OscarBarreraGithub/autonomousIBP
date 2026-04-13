#include "amflow/runtime/eta_mode.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace amflow {

namespace {

std::string Trim(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(begin, end - begin);
}

bool IsBuiltinEtaModeName(const std::string& name) {
  for (const auto& candidate : BuiltinEtaModes()) {
    if (candidate == name) {
      return true;
    }
  }
  return false;
}

void SelectNonAuxiliaryPropagators(const ProblemSpec& spec, EtaInsertionDecision& decision) {
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const auto& propagator = spec.family.propagators[index];
    if (propagator.kind == PropagatorKind::Auxiliary) {
      continue;
    }
    decision.selected_propagator_indices.push_back(index);
    decision.selected_propagators.push_back(propagator.expression);
  }
}

struct EtaTopologyPropagatorSummary {
  std::size_t declaration_order_index = 0;
  std::string expression;
  std::string mass;
  PropagatorKind kind = PropagatorKind::Standard;
  int prescription = -1;
  bool is_non_auxiliary = false;
  bool is_cut_like = false;
  std::vector<std::string> loop_momentum_identifiers;
};

struct EtaTopologySummary {
  std::string family_name;
  std::vector<std::string> loop_momenta;
  std::vector<int> top_level_sectors;
  std::vector<std::string> incoming_momenta;
  std::vector<std::string> outgoing_momenta;
  std::string momentum_conservation;
  std::vector<EtaTopologyPropagatorSummary> propagators;
  std::vector<std::string> scalar_product_rule_rights;
  std::vector<std::string> scalar_product_rule_identifiers;
  std::size_t non_auxiliary_candidate_count = 0;
  std::size_t cut_like_count = 0;
};

struct TopologyFieldStatus {
  bool available = false;
  std::string reason;
};

struct EtaTopologyPrereqSnapshot {
  EtaTopologySummary surface;
  TopologyFieldStatus u0;
  TopologyFieldStatus vacQ;
  TopologyFieldStatus var;
  std::size_t loopnum = 0;
  std::vector<std::size_t> var_proxy_declaration_indices;
  std::vector<std::string> mass;
  std::vector<std::size_t> cutvar;
  std::vector<int> pres;
  std::vector<std::string> missing_fields;
};

std::set<std::string> ExtractIdentifiers(const std::string& expression) {
  std::set<std::string> identifiers;
  std::size_t index = 0;
  while (index < expression.size()) {
    const unsigned char current = static_cast<unsigned char>(expression[index]);
    if (std::isalpha(current) == 0 && current != static_cast<unsigned char>('_')) {
      ++index;
      continue;
    }

    const std::size_t begin = index;
    ++index;
    while (index < expression.size()) {
      const unsigned char next = static_cast<unsigned char>(expression[index]);
      if (std::isalnum(next) == 0 && next != static_cast<unsigned char>('_')) {
        break;
      }
      ++index;
    }
    identifiers.insert(expression.substr(begin, index - begin));
  }
  return identifiers;
}

std::vector<std::string> CollectExpressionLoopMomentumIdentifiers(
    const std::string& expression,
    const std::vector<std::string>& loop_momenta) {
  const std::set<std::string> identifiers = ExtractIdentifiers(expression);
  std::vector<std::string> present;
  for (const auto& loop_momentum : loop_momenta) {
    if (identifiers.find(loop_momentum) != identifiers.end()) {
      present.push_back(loop_momentum);
    }
  }
  return present;
}

std::vector<std::size_t> CollectCutPropagatorIndices(const ProblemSpec& spec) {
  std::vector<std::size_t> cut_indices;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    if (spec.family.propagators[index].kind == PropagatorKind::Cut) {
      cut_indices.push_back(index);
    }
  }
  return cut_indices;
}

std::vector<int> CollectPrescriptions(const ProblemSpec& spec) {
  std::vector<int> prescriptions;
  prescriptions.reserve(spec.family.propagators.size());
  for (const auto& propagator : spec.family.propagators) {
    prescriptions.push_back(propagator.prescription);
  }
  return prescriptions;
}

std::vector<std::string> CollectMassStrings(const ProblemSpec& spec) {
  std::vector<std::string> masses;
  masses.reserve(spec.family.propagators.size());
  for (const auto& propagator : spec.family.propagators) {
    masses.push_back(propagator.mass);
  }
  return masses;
}

EtaTopologySummary BuildEtaTopologySummary(const ProblemSpec& spec) {
  EtaTopologySummary summary;
  summary.family_name = spec.family.name;
  summary.loop_momenta = spec.family.loop_momenta;
  summary.top_level_sectors = spec.family.top_level_sectors;
  summary.incoming_momenta = spec.kinematics.incoming_momenta;
  summary.outgoing_momenta = spec.kinematics.outgoing_momenta;
  summary.momentum_conservation = spec.kinematics.momentum_conservation;
  summary.propagators.reserve(spec.family.propagators.size());
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const auto& propagator = spec.family.propagators[index];
    EtaTopologyPropagatorSummary propagator_summary;
    propagator_summary.declaration_order_index = index;
    propagator_summary.expression = propagator.expression;
    propagator_summary.mass = propagator.mass;
    propagator_summary.kind = propagator.kind;
    propagator_summary.prescription = propagator.prescription;
    propagator_summary.is_non_auxiliary = propagator.kind != PropagatorKind::Auxiliary;
    propagator_summary.is_cut_like = propagator.kind == PropagatorKind::Cut;
    propagator_summary.loop_momentum_identifiers =
        CollectExpressionLoopMomentumIdentifiers(propagator.expression, summary.loop_momenta);
    if (propagator_summary.is_non_auxiliary) {
      ++summary.non_auxiliary_candidate_count;
    }
    if (propagator_summary.is_cut_like) {
      ++summary.cut_like_count;
    }
    summary.propagators.push_back(std::move(propagator_summary));
  }
  summary.scalar_product_rule_rights.reserve(spec.kinematics.scalar_product_rules.size());
  std::set<std::string> scalar_product_rule_identifiers;
  for (const auto& rule : spec.kinematics.scalar_product_rules) {
    summary.scalar_product_rule_rights.push_back(rule.right);
    const std::set<std::string> identifiers = ExtractIdentifiers(rule.right);
    scalar_product_rule_identifiers.insert(identifiers.begin(), identifiers.end());
  }
  summary.scalar_product_rule_identifiers.assign(scalar_product_rule_identifiers.begin(),
                                                 scalar_product_rule_identifiers.end());
  return summary;
}

std::string DescribeEtaTopologySummary(const EtaTopologySummary& summary) {
  std::ostringstream description;
  description << "family="
              << (summary.family_name.empty() ? std::string("<unnamed>") : summary.family_name)
              << ", loop_momenta=" << summary.loop_momenta.size()
              << ", top_level_sectors=" << summary.top_level_sectors.size()
              << ", propagators=" << summary.propagators.size()
              << ", candidate_non_auxiliary_variables=" << summary.non_auxiliary_candidate_count
              << ", cut_like_variables=" << summary.cut_like_count
              << ", scalar_product_rules=" << summary.scalar_product_rule_rights.size()
              << ", scalar_product_rule_identifiers="
              << summary.scalar_product_rule_identifiers.size();
  return description.str();
}

EtaTopologyPrereqSnapshot BuildEtaTopologyPrereqSnapshot(const ProblemSpec& spec) {
  EtaTopologyPrereqSnapshot snapshot;
  snapshot.surface = BuildEtaTopologySummary(spec);
  snapshot.u0 = {false, "not derivable from the current family/kinematics surface"};
  snapshot.vacQ = {
      false, "conservative vacuum classification is not available on the current surface"};
  snapshot.loopnum = snapshot.surface.loop_momenta.size();
  snapshot.mass = CollectMassStrings(spec);
  snapshot.cutvar = CollectCutPropagatorIndices(spec);
  snapshot.pres = CollectPrescriptions(spec);
  snapshot.var_proxy_declaration_indices.reserve(snapshot.surface.non_auxiliary_candidate_count);
  for (const auto& propagator : snapshot.surface.propagators) {
    if (propagator.is_non_auxiliary) {
      snapshot.var_proxy_declaration_indices.push_back(propagator.declaration_order_index);
    }
  }
  if (snapshot.var_proxy_declaration_indices.empty()) {
    snapshot.var = {
        false,
        "no declaration-order non-auxiliary propagators are available for the current proxy "
        "list"};
  } else {
    snapshot.var = {
        true,
        "current proxy from declaration-order non-auxiliary propagators; not upstream-fidelity "
        "Feynman-parameter candidate analysis"};
  }
  snapshot.missing_fields = {
      "u0",
      "vacuum_classification",
      "component_factorization",
      "graph_polynomials_u_f_massterm",
      "true_feynman_parameter_candidate_analysis",
  };
  return snapshot;
}

template <typename T>
std::string DescribeList(const std::vector<T>& values) {
  std::ostringstream description;
  description << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      description << "|";
    }
    description << values[index];
  }
  description << "]";
  return description.str();
}

std::string DescribeFieldStatus(const std::string& label, const TopologyFieldStatus& status) {
  std::ostringstream description;
  description << label;
  if (status.available) {
    description << "=<available>";
  } else {
    description << ":<missing>";
  }
  if (!status.reason.empty()) {
    description << "(" << status.reason << ")";
  }
  return description.str();
}

std::string DescribeEtaTopologyPrereqSnapshot(const EtaTopologyPrereqSnapshot& snapshot) {
  std::ostringstream description;
  description << "topology_prereq_bridge={"
              << DescribeFieldStatus("u0", snapshot.u0)
              << ", "
              << DescribeFieldStatus("vacQ", snapshot.vacQ)
              << ", "
              << DescribeFieldStatus("var_proxy", snapshot.var)
              << ", loopnum=" << snapshot.loopnum
              << ", mass=" << DescribeList(snapshot.mass)
              << ", cutvar=" << DescribeList(snapshot.cutvar)
              << ", pres=" << DescribeList(snapshot.pres)
              << ", candidate_non_auxiliary_variables="
              << snapshot.surface.non_auxiliary_candidate_count
              << ", cut_like_variables=" << snapshot.surface.cut_like_count
              << ", scalar_product_rule_identifiers="
              << snapshot.surface.scalar_product_rule_identifiers.size()
              << ", missing_fields=" << DescribeList(snapshot.missing_fields)
              << "}";
  return description.str();
}

bool IsSingleIdentifier(const std::string& expression) {
  if (expression.empty()) {
    return false;
  }
  const unsigned char first = static_cast<unsigned char>(expression.front());
  if (std::isalpha(first) == 0 && first != static_cast<unsigned char>('_')) {
    return false;
  }
  for (const unsigned char current : expression) {
    if (std::isalnum(current) == 0 && current != static_cast<unsigned char>('_')) {
      return false;
    }
  }
  return true;
}

std::set<std::string> CollectScalarProductRuleRhsVariables(const ProblemSpec& spec) {
  std::set<std::string> variables;
  for (const auto& rule : spec.kinematics.scalar_product_rules) {
    const std::set<std::string> identifiers = ExtractIdentifiers(rule.right);
    variables.insert(identifiers.begin(), identifiers.end());
  }
  return variables;
}

std::set<std::string> CollectExactMassLabelIdentifiers(const ProblemSpec& spec) {
  std::set<std::string> mass_labels;
  for (const auto& propagator : spec.family.propagators) {
    const std::string trimmed_mass = Trim(propagator.mass);
    if (trimmed_mass == "0" || !IsSingleIdentifier(trimmed_mass)) {
      continue;
    }
    mass_labels.insert(trimmed_mass);
  }
  return mass_labels;
}

bool IsScalarProductRuleIndependent(const std::string& expression,
                                    const std::set<std::string>& rhs_variables,
                                    const std::set<std::string>& exact_mass_labels) {
  const std::set<std::string> identifiers = ExtractIdentifiers(expression);
  for (const auto& identifier : identifiers) {
    if (exact_mass_labels.find(identifier) != exact_mass_labels.end()) {
      continue;
    }
    if (rhs_variables.find(identifier) != rhs_variables.end()) {
      return false;
    }
  }
  return true;
}

struct MassGroup {
  std::string mass;
  bool is_independent = false;
  std::vector<std::size_t> propagator_indices;
  std::vector<std::string> propagator_expressions;
};

EtaInsertionDecision PlanMassEtaMode(const ProblemSpec& spec) {
  EtaInsertionDecision decision;
  decision.mode_name = "Mass";

  const std::set<std::string> rhs_variables = CollectScalarProductRuleRhsVariables(spec);
  const std::set<std::string> exact_mass_labels = CollectExactMassLabelIdentifiers(spec);
  std::vector<MassGroup> groups;

  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const auto& propagator = spec.family.propagators[index];
    if (propagator.kind == PropagatorKind::Auxiliary) {
      continue;
    }

    const std::string trimmed_mass = Trim(propagator.mass);
    if (trimmed_mass == "0") {
      continue;
    }

    auto group = std::find_if(groups.begin(), groups.end(), [&trimmed_mass](const MassGroup& item) {
      return item.mass == trimmed_mass;
    });
    if (group == groups.end()) {
      MassGroup created;
      created.mass = trimmed_mass;
      created.is_independent =
          IsScalarProductRuleIndependent(trimmed_mass, rhs_variables, exact_mass_labels);
      groups.push_back(std::move(created));
      group = std::prev(groups.end());
    }

    group->propagator_indices.push_back(index);
    group->propagator_expressions.push_back(propagator.expression);
  }

  if (groups.empty()) {
    throw std::runtime_error(
        "eta mode Mass found no non-auxiliary non-zero-mass propagator group in bootstrap");
  }

  auto selected_group = std::find_if(groups.begin(), groups.end(), [](const MassGroup& group) {
    return group.is_independent;
  });
  const bool used_independent_group = selected_group != groups.end();
  if (!used_independent_group) {
    selected_group = groups.begin();
  }

  decision.selected_propagator_indices = selected_group->propagator_indices;
  decision.selected_propagators = selected_group->propagator_expressions;

  std::ostringstream explanation;
  if (used_independent_group) {
    explanation << "Bootstrap Mass selector chose the first scalar-product-rule-independent "
                   "equal non-zero mass group \""
                << selected_group->mass << "\" with "
                << selected_group->propagator_indices.size()
                << " non-auxiliary propagators on the current local declaration-order "
                   "candidate surface";
  } else {
    explanation << "Bootstrap Mass selector chose the first equal non-zero mass group \""
                << selected_group->mass << "\" with "
                << selected_group->propagator_indices.size()
                << " non-auxiliary propagators on the current local declaration-order "
                   "candidate surface because no scalar-product-rule-independent group was "
                   "available";
  }
  decision.explanation = explanation.str();
  return decision;
}

void ValidateUserDefinedEtaModeRegistry(
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes) {
  std::vector<std::string> seen_names;
  seen_names.reserve(user_defined_modes.size());

  for (const auto& user_defined_mode : user_defined_modes) {
    if (!user_defined_mode) {
      throw std::invalid_argument("user-defined eta mode registry contains null entry");
    }

    const std::string mode_name = user_defined_mode->Name();
    if (IsBuiltinEtaModeName(mode_name)) {
      throw std::invalid_argument("user-defined eta mode conflicts with builtin eta mode: " +
                                  mode_name);
    }

    if (std::find(seen_names.begin(), seen_names.end(), mode_name) != seen_names.end()) {
      throw std::invalid_argument("duplicate user-defined eta mode: " + mode_name);
    }
    seen_names.push_back(mode_name);
  }
}

class BuiltinEtaMode final : public EtaMode {
 public:
  explicit BuiltinEtaMode(std::string name) : name_(std::move(name)) {}

  std::string Name() const override { return name_; }

  EtaInsertionDecision Plan(const ProblemSpec& spec) const override {
    EtaInsertionDecision decision;
    decision.mode_name = name_;

    if (name_ == "Prescription") {
      SelectNonAuxiliaryPropagators(spec, decision);
      if (decision.selected_propagator_indices.empty()) {
        throw std::runtime_error(
            "eta mode Prescription found no non-auxiliary propagators in bootstrap");
      }
      std::ostringstream explanation;
      explanation << "Bootstrap alias selected "
                  << decision.selected_propagator_indices.size()
                  << " non-auxiliary propagators in declaration order for mode Prescription";
      decision.explanation = explanation.str();
      return decision;
    }

    if (name_ == "Propagator") {
      SelectNonAuxiliaryPropagators(spec, decision);
      if (decision.selected_propagator_indices.empty()) {
        throw std::runtime_error(
            "eta mode Propagator found no non-auxiliary propagators in bootstrap");
      }
      std::ostringstream explanation;
      explanation << "Bootstrap structural selector selected "
                  << decision.selected_propagator_indices.size()
                  << " non-auxiliary propagators in declaration order for mode Propagator";
      decision.explanation = explanation.str();
      return decision;
    }

    if (name_ == "Branch" || name_ == "Loop") {
      const EtaTopologyPrereqSnapshot topology_snapshot = BuildEtaTopologyPrereqSnapshot(spec);
      throw std::runtime_error("eta mode " + name_ +
                               " is blocked in bootstrap: internal eta-topology preflight "
                               "snapshot collected the current family/kinematics surface, but "
                               "topology-analysis/candidate-analysis for Branch/Loop selectors "
                               "is not implemented yet (" +
                               DescribeEtaTopologySummary(topology_snapshot.surface) + "; " +
                               DescribeEtaTopologyPrereqSnapshot(topology_snapshot) + ")");
    }

    if (name_ == "Mass") {
      return PlanMassEtaMode(spec);
    }

    if (name_ == "All") {
      SelectNonAuxiliaryPropagators(spec, decision);
      if (decision.selected_propagator_indices.empty()) {
        throw std::runtime_error("eta mode All found no non-auxiliary propagators in bootstrap");
      }
      std::ostringstream explanation;
      explanation << "Bootstrap planner selected "
                  << decision.selected_propagator_indices.size()
                  << " non-auxiliary propagators for mode All";
      decision.explanation = explanation.str();
      return decision;
    }

    throw std::runtime_error("eta mode " + name_ + " is not implemented in bootstrap");
  }

 private:
  std::string name_;
};

}  // namespace

std::vector<std::string> BuiltinEtaModes() {
  return {"Prescription", "Mass", "Propagator", "Branch", "Loop", "All"};
}

std::shared_ptr<EtaMode> MakeBuiltinEtaMode(const std::string& name) {
  for (const auto& candidate : BuiltinEtaModes()) {
    if (candidate == name) {
      return std::make_shared<BuiltinEtaMode>(name);
    }
  }
  throw std::invalid_argument("unknown eta mode: " + name);
}

std::shared_ptr<EtaMode> ResolveEtaMode(
    const std::string& name,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes) {
  ValidateUserDefinedEtaModeRegistry(user_defined_modes);

  for (const auto& user_defined_mode : user_defined_modes) {
    if (user_defined_mode->Name() == name) {
      return user_defined_mode;
    }
  }

  return MakeBuiltinEtaMode(name);
}

}  // namespace amflow
