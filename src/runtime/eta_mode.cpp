#include "amflow/runtime/eta_mode.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "amflow/core/options.hpp"

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

std::string RemoveWhitespace(const std::string& value) {
  std::string compact;
  compact.reserve(value.size());
  for (const unsigned char current : value) {
    if (std::isspace(current) != 0) {
      continue;
    }
    compact.push_back(static_cast<char>(current));
  }
  return compact;
}

bool IsIdentifierStart(const char value) {
  const unsigned char current = static_cast<unsigned char>(value);
  return std::isalpha(current) != 0 || current == static_cast<unsigned char>('_');
}

bool IsIdentifierContinue(const char value) {
  const unsigned char current = static_cast<unsigned char>(value);
  return std::isalnum(current) != 0 || current == static_cast<unsigned char>('_');
}

std::string ParseIdentifier(const std::string& expression, std::size_t& index) {
  if (index >= expression.size() || !IsIdentifierStart(expression[index])) {
    throw std::runtime_error("expected momentum identifier in supported squared-linear "
                             "propagator grammar: " +
                             expression);
  }
  const std::size_t begin = index;
  ++index;
  while (index < expression.size() && IsIdentifierContinue(expression[index])) {
    ++index;
  }
  return expression.substr(begin, index - begin);
}

std::vector<std::string> CollectKnownMomenta(const ProblemSpec& spec) {
  std::vector<std::string> known;
  known.reserve(spec.family.loop_momenta.size() + spec.kinematics.incoming_momenta.size() +
                spec.kinematics.outgoing_momenta.size());
  known.insert(known.end(), spec.family.loop_momenta.begin(), spec.family.loop_momenta.end());
  known.insert(known.end(),
               spec.kinematics.incoming_momenta.begin(),
               spec.kinematics.incoming_momenta.end());
  known.insert(known.end(),
               spec.kinematics.outgoing_momenta.begin(),
               spec.kinematics.outgoing_momenta.end());
  std::sort(known.begin(), known.end());
  known.erase(std::unique(known.begin(), known.end()), known.end());
  return known;
}

std::vector<int> ParseSupportedSquaredLinearMomentumCombination(
    const std::string& expression,
    const std::vector<std::string>& loop_momenta,
    const std::vector<std::string>& known_momenta) {
  const std::string compact = RemoveWhitespace(expression);
  if (compact.size() < 4 || compact.front() != '(' || compact[compact.size() - 3] != ')' ||
      compact.substr(compact.size() - 2) != "^2") {
    throw std::runtime_error("expected propagator expression of the form "
                             "\"(linear_momentum_combination)^2\", found: " +
                             expression);
  }

  const std::string content = compact.substr(1, compact.size() - 4);
  if (content.empty()) {
    throw std::runtime_error("encountered empty linear momentum combination in propagator "
                             "expression: " +
                             expression);
  }

  std::vector<int> loop_coefficients(loop_momenta.size(), 0);
  std::size_t index = 0;
  int sign = 1;
  bool expect_term = true;
  bool consumed_unary_sign = false;

  while (index < content.size()) {
    const char current = content[index];
    if (current == '+') {
      if (expect_term) {
        if (consumed_unary_sign) {
          throw std::runtime_error("encountered repeated sign operator in supported squared-"
                                   "linear propagator grammar: " +
                                   expression);
        }
        consumed_unary_sign = true;
        ++index;
        continue;
      }
      sign = 1;
      expect_term = true;
      consumed_unary_sign = true;
      ++index;
      continue;
    }
    if (current == '-') {
      if (expect_term) {
        if (consumed_unary_sign) {
          throw std::runtime_error("encountered repeated sign operator in supported squared-"
                                   "linear propagator grammar: " +
                                   expression);
        }
        consumed_unary_sign = true;
        sign = -1;
        ++index;
        continue;
      }
      sign = -1;
      expect_term = true;
      consumed_unary_sign = true;
      ++index;
      continue;
    }

    const std::string identifier = ParseIdentifier(content, index);
    if (std::find(known_momenta.begin(), known_momenta.end(), identifier) == known_momenta.end()) {
      throw std::runtime_error("encountered unknown momentum identifier \"" + identifier +
                               "\" in supported squared-linear propagator grammar");
    }
    const auto loop_it = std::find(loop_momenta.begin(), loop_momenta.end(), identifier);
    if (loop_it != loop_momenta.end()) {
      loop_coefficients[static_cast<std::size_t>(std::distance(loop_momenta.begin(), loop_it))] +=
          sign;
    }
    sign = 1;
    expect_term = false;
    consumed_unary_sign = false;
  }

  if (expect_term) {
    throw std::runtime_error("encountered malformed linear momentum combination in propagator "
                             "expression: " +
                             expression);
  }

  return loop_coefficients;
}

std::vector<std::size_t> CollectActiveTopSectorPropagatorIndices(const ProblemSpec& spec) {
  if (spec.family.top_level_sectors.empty()) {
    throw std::runtime_error("current family definition does not provide a top-level sector");
  }
  if (spec.family.top_level_sectors.size() != 1) {
    throw std::runtime_error("multiple top-level sectors remain deferred to the later "
                             "multi-top-sector orchestration lane");
  }

  const int sector = spec.family.top_level_sectors.front();
  if (sector <= 0) {
    throw std::runtime_error("top-level sector must be a positive bitmask");
  }
  if (spec.family.propagators.size() > static_cast<std::size_t>(std::numeric_limits<int>::digits)) {
    throw std::runtime_error("top-level sector bitmask support is limited to " +
                             std::to_string(std::numeric_limits<int>::digits) +
                             " propagators in the current Branch/Loop selector bootstrap");
  }

  const unsigned long long mask = static_cast<unsigned long long>(sector);
  std::vector<std::size_t> active;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    if ((mask & (1ULL << index)) == 0) {
      continue;
    }
    active.push_back(index);
  }
  if (active.empty()) {
    throw std::runtime_error("top-level sector bitmask selects no active propagators");
  }
  return active;
}

using UMonomial = std::vector<std::size_t>;
using UPolynomial = std::map<UMonomial, long long>;
using LinearForm = std::vector<std::pair<std::size_t, long long>>;

struct BranchLoopCandidateAnalysis {
  std::size_t loopnum = 0;
  std::vector<std::size_t> active_candidate_indices;
  std::vector<std::size_t> uncut_candidate_indices;
  UPolynomial u_polynomial;
};

int PermutationSign(const std::vector<std::size_t>& permutation) {
  int inversions = 0;
  for (std::size_t left = 0; left < permutation.size(); ++left) {
    for (std::size_t right = left + 1; right < permutation.size(); ++right) {
      if (permutation[left] > permutation[right]) {
        ++inversions;
      }
    }
  }
  return (inversions % 2 == 0) ? 1 : -1;
}

UPolynomial MultiplyByLinearForm(const UPolynomial& polynomial, const LinearForm& linear_form) {
  UPolynomial product;
  for (const auto& term : polynomial) {
    for (const auto& factor : linear_form) {
      UMonomial monomial = term.first;
      monomial.push_back(factor.first);
      std::sort(monomial.begin(), monomial.end());
      product[monomial] += term.second * factor.second;
    }
  }
  for (auto it = product.begin(); it != product.end();) {
    if (it->second == 0) {
      it = product.erase(it);
    } else {
      ++it;
    }
  }
  return product;
}

void AccumulatePolynomial(UPolynomial& destination,
                          const UPolynomial& source,
                          const int sign) {
  for (const auto& term : source) {
    destination[term.first] += sign * term.second;
  }
  for (auto it = destination.begin(); it != destination.end();) {
    if (it->second == 0) {
      it = destination.erase(it);
    } else {
      ++it;
    }
  }
}

UPolynomial BuildFirstSymanzikPolynomial(
    const std::vector<std::size_t>& candidate_indices,
    const std::vector<std::vector<int>>& loop_coefficients,
    const std::size_t loopnum) {
  if (loopnum == 0) {
    throw std::runtime_error("loop-momentum list is empty");
  }

  std::vector<std::vector<LinearForm>> matrix(
      loopnum, std::vector<LinearForm>(loopnum));
  for (std::size_t row = 0; row < loopnum; ++row) {
    for (std::size_t column = 0; column < loopnum; ++column) {
      LinearForm form;
      for (std::size_t index = 0; index < candidate_indices.size(); ++index) {
        const long long coefficient = static_cast<long long>(loop_coefficients[index][row]) *
                                      static_cast<long long>(loop_coefficients[index][column]);
        if (coefficient == 0) {
          continue;
        }
        form.push_back({candidate_indices[index], coefficient});
      }
      matrix[row][column] = std::move(form);
    }
  }

  std::vector<std::size_t> permutation(loopnum);
  for (std::size_t index = 0; index < loopnum; ++index) {
    permutation[index] = index;
  }

  UPolynomial determinant;
  do {
    UPolynomial term;
    term[{}] = 1;
    bool vanished = false;
    for (std::size_t row = 0; row < loopnum; ++row) {
      const LinearForm& linear_form = matrix[row][permutation[row]];
      if (linear_form.empty()) {
        vanished = true;
        break;
      }
      term = MultiplyByLinearForm(term, linear_form);
      if (term.empty()) {
        vanished = true;
        break;
      }
    }
    if (!vanished) {
      AccumulatePolynomial(determinant, term, PermutationSign(permutation));
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));

  return determinant;
}

bool IsSubsetOf(const std::vector<std::size_t>& subset, const std::vector<std::size_t>& superset) {
  return std::includes(superset.begin(), superset.end(), subset.begin(), subset.end());
}

std::vector<std::size_t> IntersectWithSortedIndices(const std::vector<std::size_t>& values,
                                                    const std::vector<std::size_t>& keep) {
  std::vector<std::size_t> intersection;
  std::set_intersection(values.begin(),
                        values.end(),
                        keep.begin(),
                        keep.end(),
                        std::back_inserter(intersection));
  return intersection;
}

bool CompareIndexGroups(const std::vector<std::size_t>& left,
                        const std::vector<std::size_t>& right) {
  if (left.size() != right.size()) {
    return left.size() < right.size();
  }
  return left < right;
}

std::vector<std::vector<std::size_t>> SortAndDeduplicateGroups(
    std::vector<std::vector<std::size_t>> groups) {
  std::sort(groups.begin(), groups.end(), CompareIndexGroups);
  groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
  return groups;
}

std::vector<std::size_t> DeduplicateSelectionOrder(const std::vector<std::size_t>& values) {
  std::vector<std::size_t> unique_values;
  std::set<std::size_t> seen;
  for (const std::size_t value : values) {
    if (!seen.insert(value).second) {
      continue;
    }
    unique_values.push_back(value);
  }
  return unique_values;
}

[[noreturn]] void ThrowBranchLoopBootstrapBlocker(const ProblemSpec& spec,
                                                  const std::string& mode_name,
                                                  const std::string& reason) {
  const EtaTopologyPrereqSnapshot topology_snapshot = BuildEtaTopologyPrereqSnapshot(spec);
  throw std::runtime_error("eta mode " + mode_name +
                           " is blocked in bootstrap: internal eta-topology prereq snapshot "
                           "collected the current family/kinematics surface, but Branch/Loop "
                           "candidate analysis is unavailable for this input (" +
                           reason + "; " + DescribeEtaTopologySummary(topology_snapshot.surface) +
                           "; " + DescribeEtaTopologyPrereqSnapshot(topology_snapshot) + ")");
}

BranchLoopCandidateAnalysis AnalyzeBranchLoopCandidateSurface(const ProblemSpec& spec,
                                                              const std::string& mode_name) {
  BranchLoopCandidateAnalysis analysis;
  analysis.loopnum = spec.family.loop_momenta.size();
  if (analysis.loopnum == 0) {
    ThrowBranchLoopBootstrapBlocker(spec,
                                    mode_name,
                                    "loop-momentum list is empty on the current family surface");
  }

  std::vector<std::size_t> active_indices;
  try {
    active_indices = CollectActiveTopSectorPropagatorIndices(spec);
  } catch (const std::runtime_error& error) {
    ThrowBranchLoopBootstrapBlocker(spec, mode_name, error.what());
  }
  const std::vector<std::string> known_momenta = CollectKnownMomenta(spec);
  std::vector<std::vector<int>> loop_coefficients;

  for (const std::size_t index : active_indices) {
    const auto& propagator = spec.family.propagators[index];
    if (propagator.kind == PropagatorKind::Auxiliary) {
      continue;
    }
    if (propagator.kind == PropagatorKind::Linear) {
      ThrowBranchLoopBootstrapBlocker(
          spec,
          mode_name,
          "linear propagators remain outside the current Branch/Loop selector subset");
    }

    try {
      loop_coefficients.push_back(ParseSupportedSquaredLinearMomentumCombination(
          propagator.expression, spec.family.loop_momenta, known_momenta));
    } catch (const std::runtime_error& error) {
      ThrowBranchLoopBootstrapBlocker(spec, mode_name, error.what());
    }
    analysis.active_candidate_indices.push_back(index);
    if (propagator.kind != PropagatorKind::Cut) {
      analysis.uncut_candidate_indices.push_back(index);
    }
  }

  if (analysis.active_candidate_indices.empty()) {
    ThrowBranchLoopBootstrapBlocker(
        spec,
        mode_name,
        "no active non-auxiliary propagators remain inside the current top-level sector");
  }
  if (analysis.uncut_candidate_indices.empty()) {
    ThrowBranchLoopBootstrapBlocker(
        spec,
        mode_name,
        "all active non-auxiliary propagators are cut-like, leaving no uncut Branch/Loop "
        "candidates");
  }

  analysis.u_polynomial = BuildFirstSymanzikPolynomial(
      analysis.active_candidate_indices, loop_coefficients, analysis.loopnum);
  if (analysis.u_polynomial.empty()) {
    ThrowBranchLoopBootstrapBlocker(spec,
                                    mode_name,
                                    "the derived first Symanzik support is empty on the current "
                                    "supported subset");
  }

  for (const auto& term : analysis.u_polynomial) {
    if (term.first.size() != analysis.loopnum) {
      ThrowBranchLoopBootstrapBlocker(
          spec,
          mode_name,
          "the derived first Symanzik support is not homogeneous of degree loopnum");
    }
    if (std::adjacent_find(term.first.begin(), term.first.end()) != term.first.end()) {
      ThrowBranchLoopBootstrapBlocker(
          spec,
          mode_name,
          "the derived first Symanzik support is not squarefree on the current subset");
    }
  }

  std::sort(analysis.active_candidate_indices.begin(), analysis.active_candidate_indices.end());
  std::sort(analysis.uncut_candidate_indices.begin(), analysis.uncut_candidate_indices.end());
  return analysis;
}

std::vector<std::vector<std::size_t>> BuildBranchGroups(
    const BranchLoopCandidateAnalysis& analysis) {
  std::vector<std::vector<std::size_t>> groups;
  for (const std::size_t variable : analysis.active_candidate_indices) {
    std::vector<std::size_t> coefficient_variables;
    for (const auto& term : analysis.u_polynomial) {
      if (!std::binary_search(term.first.begin(), term.first.end(), variable)) {
        continue;
      }
      for (const std::size_t factor : term.first) {
        if (factor == variable) {
          continue;
        }
        coefficient_variables.push_back(factor);
      }
    }
    std::sort(coefficient_variables.begin(), coefficient_variables.end());
    coefficient_variables.erase(
        std::unique(coefficient_variables.begin(), coefficient_variables.end()),
        coefficient_variables.end());

    std::vector<std::size_t> branch;
    std::set_difference(analysis.active_candidate_indices.begin(),
                        analysis.active_candidate_indices.end(),
                        coefficient_variables.begin(),
                        coefficient_variables.end(),
                        std::back_inserter(branch));
    branch = IntersectWithSortedIndices(branch, analysis.uncut_candidate_indices);
    if (!branch.empty()) {
      groups.push_back(std::move(branch));
    }
  }
  return SortAndDeduplicateGroups(std::move(groups));
}

void BuildSubsetCombinations(const std::vector<std::size_t>& values,
                             const std::size_t subset_size,
                             const std::size_t next_index,
                             std::vector<std::size_t>& current,
                             std::vector<std::vector<std::size_t>>& all) {
  if (current.size() == subset_size) {
    all.push_back(current);
    return;
  }
  for (std::size_t index = next_index; index < values.size(); ++index) {
    current.push_back(values[index]);
    BuildSubsetCombinations(values, subset_size, index + 1, current, all);
    current.pop_back();
  }
}

std::vector<std::vector<std::size_t>> BuildLoopGroups(
    const BranchLoopCandidateAnalysis& analysis) {
  const std::size_t subset_size = analysis.loopnum - 1;
  std::vector<std::vector<std::size_t>> subsets;
  if (subset_size == 0) {
    subsets.push_back({});
  } else {
    std::vector<std::size_t> current;
    BuildSubsetCombinations(analysis.active_candidate_indices,
                            subset_size,
                            0,
                            current,
                            subsets);
  }

  std::vector<std::vector<std::size_t>> groups;
  for (const auto& subset : subsets) {
    std::vector<std::size_t> group;
    for (const auto& term : analysis.u_polynomial) {
      if (!IsSubsetOf(subset, term.first)) {
        continue;
      }
      for (const std::size_t factor : term.first) {
        if (std::binary_search(subset.begin(), subset.end(), factor)) {
          continue;
        }
        group.push_back(factor);
      }
    }
    std::sort(group.begin(), group.end());
    group.erase(std::unique(group.begin(), group.end()), group.end());
    group = IntersectWithSortedIndices(group, analysis.uncut_candidate_indices);
    if (!group.empty()) {
      groups.push_back(std::move(group));
    }
  }
  return SortAndDeduplicateGroups(std::move(groups));
}

EtaInsertionDecision PlanBranchOrLoopEtaMode(const ProblemSpec& spec,
                                             const std::string& mode_name) {
  const BranchLoopCandidateAnalysis analysis = AnalyzeBranchLoopCandidateSurface(spec, mode_name);
  std::vector<std::vector<std::size_t>> groups =
      (mode_name == "Branch") ? BuildBranchGroups(analysis) : BuildLoopGroups(analysis);
  if (groups.empty()) {
    ThrowBranchLoopBootstrapBlocker(
        spec,
        mode_name,
        "the derived first Symanzik support produced no non-empty uncut " + mode_name +
            " groups");
  }

  std::vector<std::size_t> selected_indices;
  selected_indices.reserve(groups.size());
  for (const auto& group : groups) {
    selected_indices.push_back(group.front());
  }
  const std::size_t raw_selection_count = selected_indices.size();
  selected_indices = DeduplicateSelectionOrder(selected_indices);
  if (selected_indices.empty()) {
    ThrowBranchLoopBootstrapBlocker(
        spec,
        mode_name,
        "the derived " + mode_name +
            " groups collapsed to an empty unique propagator selection");
  }

  EtaInsertionDecision decision;
  decision.mode_name = mode_name;
  decision.selected_propagator_indices = selected_indices;
  for (const std::size_t index : selected_indices) {
    decision.selected_propagators.push_back(spec.family.propagators[index].expression);
  }

  std::ostringstream explanation;
  explanation << "Supported " << mode_name << " selector chose "
              << decision.selected_propagator_indices.size()
              << " unique uncut propagators from " << groups.size()
              << " topology groups on the current single-top-sector squared-linear-momentum "
                 "subset";
  if (raw_selection_count != decision.selected_propagator_indices.size()) {
    explanation << " after deduplicating repeated first-choice candidates";
  }
  decision.explanation = explanation.str();
  return decision;
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
      return PlanBranchOrLoopEtaMode(spec, name_);
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

EtaInsertionDecision PlanAmfOptionsEtaMode(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes) {
  if (amf_options.amf_modes.empty()) {
    throw std::invalid_argument("eta-mode list must not be empty");
  }

  std::exception_ptr last_failure;
  for (const std::string& eta_mode_name : amf_options.amf_modes) {
    const std::shared_ptr<EtaMode> eta_mode =
        ResolveEtaMode(eta_mode_name, user_defined_modes);
    try {
      return eta_mode->Plan(spec);
    } catch (const std::exception&) {
      last_failure = std::current_exception();
      if (eta_mode_name == "Branch" || eta_mode_name == "Loop") {
        std::rethrow_exception(last_failure);
      }
    }
  }

  if (!last_failure) {
    throw std::runtime_error(
        "AmfOptions eta-mode selection exhausted without planning failure");
  }
  std::rethrow_exception(last_failure);
}

}  // namespace amflow
