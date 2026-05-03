#include "amflow/runtime/boundary_generation.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "amflow/core/options.hpp"
#include "amflow/runtime/ending_scheme.hpp"

namespace amflow {

namespace {

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index != 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

constexpr char kBuiltinCutkoskyPhaseSpaceStrategy[] = "builtin::cutkosky-phase-space";

std::string JoinIndices(const std::vector<std::size_t>& indices) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << indices[index];
  }
  out << "]";
  return out.str();
}

std::string JoinSectors(const std::vector<int>& sectors) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < sectors.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << sectors[index];
  }
  out << "]";
  return out.str();
}

std::string JoinNames(const std::vector<std::string>& names) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << names[index];
  }
  out << "]";
  return out.str();
}

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(start, end - start);
}

bool ContainsWhitespace(const std::string& value) {
  return std::any_of(value.begin(), value.end(), [](const unsigned char current) {
    return std::isspace(current) != 0;
  });
}

void ValidateProblemSpecForBoundaryGeneration(const ProblemSpec& spec) {
  const std::vector<std::string> validation_messages = ValidateProblemSpec(spec);
  if (!validation_messages.empty()) {
    throw std::invalid_argument(JoinMessages(validation_messages));
  }
}

bool IsReviewedEtaInfinityBareZeroMassLiteral(const std::string& trimmed) {
  return trimmed == "0" || trimmed == "+0" || trimmed == "-0";
}

bool IsReviewedEtaInfinityZeroMassLiteral(const std::string& value) {
  std::string trimmed = Trim(value);
  while (trimmed.size() >= 3 && trimmed.front() == '(' && trimmed.back() == ')') {
    trimmed = Trim(trimmed.substr(1, trimmed.size() - 2));
  }
  return IsReviewedEtaInfinityBareZeroMassLiteral(trimmed);
}

std::string DescribeCutComponent(const CutkoskyPhaseSpaceCutComponent& component) {
  std::ostringstream out;
  out << "cuts=" << JoinIndices(component.cut_propagator_indices)
      << " loops=" << JoinNames(component.loop_momenta)
      << " active_top_level_sectors="
      << JoinSectors(component.active_top_level_sectors);
  return out.str();
}

std::string DescribeCutComponents(
    const std::vector<CutkoskyPhaseSpaceCutComponent>& components) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < components.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << DescribeCutComponent(components[index]);
  }
  out << "]";
  return out.str();
}

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

bool ShareLoopMomentum(const CutkoskyPhaseSpaceCutSupport& lhs,
                       const CutkoskyPhaseSpaceCutSupport& rhs) {
  for (const std::string& loop_momentum : lhs.loop_momenta) {
    if (std::find(rhs.loop_momenta.begin(), rhs.loop_momenta.end(), loop_momentum) !=
        rhs.loop_momenta.end()) {
      return true;
    }
  }
  return false;
}

bool IsPropagatorActiveInTopLevelSector(const std::size_t propagator_index,
                                        const int sector) {
  if (sector <= 0) {
    return false;
  }
  const unsigned long long sector_mask = static_cast<unsigned long long>(sector);
  const std::size_t max_supported_bits = sizeof(sector_mask) * 8;
  return propagator_index < max_supported_bits &&
         (sector_mask & (1ULL << propagator_index)) != 0ULL;
}

bool IsPropagatorActiveInTarget(const std::size_t propagator_index,
                                const TargetIntegral& target) {
  return propagator_index < target.indices.size() && target.indices[propagator_index] > 0;
}

std::vector<int> IntersectActiveTopLevelSectors(const std::vector<int>& lhs,
                                                const std::vector<int>& rhs) {
  std::vector<int> intersection;
  for (const int sector : lhs) {
    if (std::find(rhs.begin(), rhs.end(), sector) != rhs.end()) {
      intersection.push_back(sector);
    }
  }
  return intersection;
}

std::vector<std::string> IntersectActiveTargetLabels(const std::vector<std::string>& lhs,
                                                     const std::vector<std::string>& rhs) {
  std::vector<std::string> intersection;
  for (const std::string& label : lhs) {
    if (std::find(rhs.begin(), rhs.end(), label) != rhs.end()) {
      intersection.push_back(label);
    }
  }
  return intersection;
}

std::vector<CutkoskyPhaseSpaceCutComponent> BuildCutComponents(
    const std::vector<CutkoskyPhaseSpaceCutSupport>& cut_supports,
    const std::vector<std::string>& declared_loop_order) {
  std::vector<CutkoskyPhaseSpaceCutComponent> components;
  std::vector<bool> visited(cut_supports.size(), false);
  for (std::size_t seed = 0; seed < cut_supports.size(); ++seed) {
    if (visited[seed]) {
      continue;
    }

    CutkoskyPhaseSpaceCutComponent component;
    std::set<std::string> component_loops;
    std::vector<std::size_t> queue = {seed};
    visited[seed] = true;
    bool initialized_component_sectors = false;
    bool initialized_component_targets = false;
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
      const std::size_t current = queue[cursor];
      const CutkoskyPhaseSpaceCutSupport& support = cut_supports[current];
      component.cut_propagator_indices.push_back(support.propagator_index);
      component_loops.insert(support.loop_momenta.begin(), support.loop_momenta.end());
      if (!initialized_component_sectors) {
        component.active_top_level_sectors = support.active_top_level_sectors;
        initialized_component_sectors = true;
      } else {
        component.active_top_level_sectors = IntersectActiveTopLevelSectors(
            component.active_top_level_sectors, support.active_top_level_sectors);
      }
      if (!initialized_component_targets) {
        component.active_target_labels = support.active_target_labels;
        initialized_component_targets = true;
      } else {
        component.active_target_labels = IntersectActiveTargetLabels(
            component.active_target_labels, support.active_target_labels);
      }

      for (std::size_t candidate = 0; candidate < cut_supports.size(); ++candidate) {
        if (visited[candidate] || !ShareLoopMomentum(support, cut_supports[candidate])) {
          continue;
        }
        visited[candidate] = true;
        queue.push_back(candidate);
      }
    }

    std::sort(component.cut_propagator_indices.begin(),
              component.cut_propagator_indices.end());
    for (const std::string& loop_momentum : declared_loop_order) {
      if (component_loops.count(loop_momentum) != 0) {
        component.loop_momenta.push_back(loop_momentum);
      }
    }
    components.push_back(std::move(component));
  }
  return components;
}

CutkoskyPhaseSpaceTopology AnalyzeCutkoskyPhaseSpaceCutTopologyImpl(
    const FamilyDefinition& family,
    const std::vector<TargetIntegral>* targets) {
  CutkoskyPhaseSpaceTopology topology;
  for (std::size_t index = 0; index < family.propagators.size(); ++index) {
    const Propagator& propagator = family.propagators[index];
    if (propagator.kind != PropagatorKind::Cut) {
      continue;
    }

    CutkoskyPhaseSpaceCutSupport support;
    support.propagator_index = index;
    const std::set<std::string> identifiers = ExtractIdentifiers(propagator.expression);
    for (const std::string& loop_momentum : family.loop_momenta) {
      if (identifiers.find(loop_momentum) != identifiers.end()) {
        support.loop_momenta.push_back(loop_momentum);
      }
    }
    for (const int sector : family.top_level_sectors) {
      if (IsPropagatorActiveInTopLevelSector(index, sector)) {
        support.active_top_level_sectors.push_back(sector);
      }
    }
    if (targets != nullptr) {
      for (const TargetIntegral& target : *targets) {
        if (IsPropagatorActiveInTarget(index, target)) {
          support.active_target_labels.push_back(target.Label());
        }
      }
    }
    topology.cut_supports.push_back(std::move(support));
  }
  topology.cut_components =
      BuildCutComponents(topology.cut_supports, family.loop_momenta);
  return topology;
}

void ValidateBuiltinEtaInfinitySubset(const ProblemSpec& spec) {
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Standard) {
      throw BoundaryUnsolvedError(
          "builtin eta->infinity boundary request generation only supports standard "
          "propagators; propagator " +
          std::to_string(index) + " has kind " + ToString(propagator.kind));
    }
    if (!IsReviewedEtaInfinityZeroMassLiteral(propagator.mass)) {
      throw BoundaryUnsolvedError(
          "builtin eta->infinity boundary request generation only supports propagators with "
          "zero mass literal \"0\", \"+0\", or \"-0\" after trimming outer whitespace and at "
          "any number of redundant outer parenthesis pairs; "
          "propagator " +
          std::to_string(index) + " has mass \"" + propagator.mass + "\"");
    }
  }
}

void ValidateBuiltinCutkoskyPhaseSpaceSubset(const ProblemSpec& spec) {
  bool saw_cut_propagator = false;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind == PropagatorKind::Cut) {
      saw_cut_propagator = true;
      continue;
    }
    if (propagator.kind == PropagatorKind::Standard) {
      continue;
    }
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary request generation only supports standard/cut "
        "propagators; propagator " +
        std::to_string(index) + " has kind " + ToString(propagator.kind));
  }

  if (!saw_cut_propagator) {
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary request generation requires at least one cut "
        "propagator on the current reviewed phase-space subset");
  }

  const CutkoskyPhaseSpaceTopology topology =
      AnalyzeCutkoskyPhaseSpaceCutTopology(spec);
  for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
    if (!support.loop_momenta.empty()) {
      continue;
    }
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary request generation requires cut propagator " +
        std::to_string(support.propagator_index) +
        " to carry declared loop-momentum support; no declared loop momentum support found "
        "in expression \"" + spec.family.propagators[support.propagator_index].expression +
        "\"");
  }

  if (topology.cut_components.size() > 1) {
    throw BoundaryUnsolvedError(
        "builtin Cutkosky phase-space boundary request generation requires a connected cut "
        "surface on the current reviewed phase-space subset; disconnected cut components: " +
        DescribeCutComponents(topology.cut_components));
  }

  if (spec.family.top_level_sectors.size() == 1) {
    const int sector = spec.family.top_level_sectors.front();
    for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
      if (!support.active_top_level_sectors.empty()) {
        continue;
      }
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires cut propagator " +
          std::to_string(support.propagator_index) +
          " to be active in the single declared top-level sector " +
          std::to_string(sector) +
          " on the current reviewed top-sector support subset");
    }
  } else if (spec.family.top_level_sectors.size() > 1) {
    for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
      if (!support.active_top_level_sectors.empty()) {
        continue;
      }
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires cut propagator " +
          std::to_string(support.propagator_index) +
          " to be active in at least one declared top-level sector " +
          JoinSectors(spec.family.top_level_sectors) +
          " on the current reviewed multi-top-sector support subset");
    }
    for (const CutkoskyPhaseSpaceCutComponent& component : topology.cut_components) {
      if (!component.active_top_level_sectors.empty()) {
        continue;
      }
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires connected cut "
          "component " +
          DescribeCutComponent(component) +
          " to share at least one declared top-level sector " +
          JoinSectors(spec.family.top_level_sectors) +
          " on the current reviewed component top-sector support subset");
    }
  }

  for (const TargetIntegral& target : spec.targets) {
    const std::string target_label = target.Label();
    for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
      if (std::find(support.active_target_labels.begin(),
                    support.active_target_labels.end(),
                    target_label) != support.active_target_labels.end()) {
        continue;
      }
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires target " +
          target_label + " to keep cut propagator " +
          std::to_string(support.propagator_index) +
          " active on the current reviewed target-support subset");
    }
  }
}

std::string CutkoskyPhaseSpaceProviderStrategyForPrescription(
    const FeynmanPrescription prescription) {
  switch (prescription) {
    case FeynmanPrescription::PlusI0:
      return std::string(kBuiltinCutkoskyPhaseSpaceStrategy) + "::plus_i0";
    case FeynmanPrescription::MinusI0:
      return std::string(kBuiltinCutkoskyPhaseSpaceStrategy) + "::minus_i0";
    case FeynmanPrescription::None:
      return std::string(kBuiltinCutkoskyPhaseSpaceStrategy) + "::none";
  }
  return kBuiltinCutkoskyPhaseSpaceStrategy;
}

std::string ResolveRawCutkoskyPhaseSpaceStrategy(const ProblemSpec& spec) {
  std::optional<FeynmanPrescription> selected_cut_prescription;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Cut) {
      continue;
    }

    const std::optional<FeynmanPrescription> raw_prescription =
        ParseFeynmanPrescription(propagator.prescription);
    if (!raw_prescription.has_value()) {
      throw std::invalid_argument("family.propagators[" + std::to_string(index) +
                                  "].prescription must be one of -1 (-i0), 0 (none), or 1 "
                                  "(+i0)");
    }

    if (!selected_cut_prescription.has_value()) {
      selected_cut_prescription = *raw_prescription;
      continue;
    }

    if (*selected_cut_prescription != *raw_prescription) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires all cut "
          "propagators to carry the same raw provider strategy on the current reviewed "
          "raw-prescription provider-selection subset");
    }
  }

  if (!selected_cut_prescription.has_value()) {
    return kBuiltinCutkoskyPhaseSpaceStrategy;
  }

  return CutkoskyPhaseSpaceProviderStrategyForPrescription(*selected_cut_prescription);
}

std::string ResolveBuiltinCutkoskyPhaseSpaceStrategy(const ProblemSpec& spec) {
  if (spec.family.loop_prescriptions.empty()) {
    return ResolveRawCutkoskyPhaseSpaceStrategy(spec);
  }

  std::optional<FeynmanPrescription> selected_cut_prescription;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Cut) {
      continue;
    }

    const std::optional<FeynmanPrescription> derived_prescription =
        DerivePropagatorPrescriptionFromLoopPrescriptions(spec.family, propagator);
    if (!derived_prescription.has_value()) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation could not derive a "
          "loop-prescription-backed provider strategy for cut propagator " +
          std::to_string(index) + " from family.loop_prescriptions");
    }

    const std::optional<FeynmanPrescription> raw_prescription =
        ParseFeynmanPrescription(propagator.prescription);
    if (!raw_prescription.has_value()) {
      throw std::invalid_argument("family.propagators[" + std::to_string(index) +
                                  "].prescription must be one of -1 (-i0), 0 (none), or 1 "
                                  "(+i0)");
    }

    if (*derived_prescription != *raw_prescription) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires cut propagator " +
          std::to_string(index) +
          " raw prescription to match family.loop_prescriptions on the current reviewed "
          "provider-selection subset");
    }

    if (!selected_cut_prescription.has_value()) {
      selected_cut_prescription = *derived_prescription;
      continue;
    }

    if (*selected_cut_prescription != *derived_prescription) {
      throw BoundaryUnsolvedError(
          "builtin Cutkosky phase-space boundary request generation requires all cut "
          "propagators to resolve to the same loop-prescription-backed provider strategy on "
          "the current reviewed provider-selection subset");
    }
  }

  if (!selected_cut_prescription.has_value()) {
    return kBuiltinCutkoskyPhaseSpaceStrategy;
  }

  return CutkoskyPhaseSpaceProviderStrategyForPrescription(*selected_cut_prescription);
}

std::string SupportedEtaInfinityTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::eta->infinity";
}

std::string SupportedCutkoskyPhaseSpaceTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::cutkosky-phase-space";
}

void ValidateEtaInfinityBoundaryEtaSymbol(const std::string& eta_symbol) {
  const std::string trimmed_eta_symbol = Trim(eta_symbol);
  if (trimmed_eta_symbol.empty()) {
    throw std::invalid_argument(
        "builtin eta->infinity boundary request eta_symbol must not be empty");
  }
  if (trimmed_eta_symbol != eta_symbol) {
    throw std::invalid_argument(
        "builtin eta->infinity boundary request eta_symbol must not contain leading or "
        "trailing whitespace");
  }
}

void ValidateCutkoskyPhaseSpaceBoundaryEtaSymbol(const std::string& eta_symbol) {
  const std::string trimmed_eta_symbol = Trim(eta_symbol);
  if (trimmed_eta_symbol.empty()) {
    throw std::invalid_argument(
        "builtin Cutkosky phase-space boundary request eta_symbol must not be empty");
  }
  if (trimmed_eta_symbol != eta_symbol) {
    throw std::invalid_argument(
        "builtin Cutkosky phase-space boundary request eta_symbol must not contain leading "
        "or trailing whitespace");
  }
  if (ContainsWhitespace(eta_symbol)) {
    throw std::invalid_argument(
        "builtin Cutkosky phase-space boundary request eta_symbol must not contain internal "
        "whitespace");
  }
}

void ValidatePlannedEtaInfinityTerminalNodes(const ProblemSpec& spec,
                                             const EndingDecision& decision) {
  const std::string supported_terminal_node = SupportedEtaInfinityTerminalNode(spec);
  std::size_t trimmed_supported_count = 0;
  for (const std::string& terminal_node : decision.terminal_nodes) {
    if (Trim(terminal_node) == supported_terminal_node) {
      ++trimmed_supported_count;
    }
  }
  if (trimmed_supported_count > 1) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; duplicate supported terminal node");
  }

  for (const std::string& terminal_node : decision.terminal_nodes) {
    if (Trim(terminal_node) != supported_terminal_node) {
      throw BoundaryUnsolvedError(
          "planned eta->infinity boundary request requires exactly one supported terminal "
          "node " +
          supported_terminal_node + "; unsupported extra terminal node " + terminal_node);
    }
  }

  if (trimmed_supported_count == 0) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; missing supported terminal node");
  }
}

void ValidatePlannedEtaInfinityDecisionMetadata(const EndingDecision& decision) {
  const std::string trimmed_terminal_strategy = Trim(decision.terminal_strategy);
  if (trimmed_terminal_strategy.empty()) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires selected ending decision "
        "terminal_strategy must not be empty");
  }
  if (trimmed_terminal_strategy != decision.terminal_strategy) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires selected ending decision "
        "terminal_strategy must not contain leading or trailing whitespace");
  }
  if (ContainsWhitespace(decision.terminal_strategy)) {
    throw BoundaryUnsolvedError(
        "planned eta->infinity boundary request requires selected ending decision "
        "terminal_strategy must not contain internal whitespace");
  }
}

void ValidatePlannedCutkoskyPhaseSpaceTerminalNodes(const ProblemSpec& spec,
                                                    const EndingDecision& decision) {
  const std::string supported_terminal_node = SupportedCutkoskyPhaseSpaceTerminalNode(spec);
  std::size_t trimmed_supported_count = 0;
  for (const std::string& terminal_node : decision.terminal_nodes) {
    if (Trim(terminal_node) == supported_terminal_node) {
      ++trimmed_supported_count;
    }
  }
  if (trimmed_supported_count > 1) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; duplicate supported terminal node");
  }

  for (const std::string& terminal_node : decision.terminal_nodes) {
    if (Trim(terminal_node) != supported_terminal_node) {
      throw BoundaryUnsolvedError(
          "planned Cutkosky phase-space boundary request requires exactly one supported "
          "terminal node " +
          supported_terminal_node + "; unsupported extra terminal node " + terminal_node);
    }
  }

  if (trimmed_supported_count == 0) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires exactly one supported terminal "
        "node " +
        supported_terminal_node + "; missing supported terminal node");
  }
}

void ValidatePlannedCutkoskyPhaseSpaceDecisionMetadata(const EndingDecision& decision) {
  const std::string trimmed_terminal_strategy = Trim(decision.terminal_strategy);
  if (trimmed_terminal_strategy.empty()) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires selected ending decision "
        "terminal_strategy must not be empty");
  }
  if (trimmed_terminal_strategy != decision.terminal_strategy) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires selected ending decision "
        "terminal_strategy must not contain leading or trailing whitespace");
  }
  if (ContainsWhitespace(decision.terminal_strategy)) {
    throw BoundaryUnsolvedError(
        "planned Cutkosky phase-space boundary request requires selected ending decision "
        "terminal_strategy must not contain internal whitespace");
  }
}

}  // namespace

CutkoskyPhaseSpaceTopology AnalyzeCutkoskyPhaseSpaceCutTopology(
    const FamilyDefinition& family) {
  return AnalyzeCutkoskyPhaseSpaceCutTopologyImpl(family, nullptr);
}

CutkoskyPhaseSpaceTopology AnalyzeCutkoskyPhaseSpaceCutTopology(
    const ProblemSpec& spec) {
  return AnalyzeCutkoskyPhaseSpaceCutTopologyImpl(spec.family, &spec.targets);
}

BoundaryRequest GenerateBuiltinEtaInfinityBoundaryRequest(const ProblemSpec& spec,
                                                         const std::string& eta_symbol) {
  ValidateProblemSpecForBoundaryGeneration(spec);
  ValidateEtaInfinityBoundaryEtaSymbol(eta_symbol);

  ValidateBuiltinEtaInfinitySubset(spec);

  BoundaryRequest request;
  request.variable = eta_symbol;
  request.location = "infinity";
  request.strategy = "builtin::eta->infinity";
  return request;
}

BoundaryRequest GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(const ProblemSpec& spec,
                                                                 const std::string& eta_symbol) {
  ValidateProblemSpecForBoundaryGeneration(spec);
  ValidateCutkoskyPhaseSpaceBoundaryEtaSymbol(eta_symbol);

  ValidateBuiltinCutkoskyPhaseSpaceSubset(spec);

  BoundaryRequest request;
  request.variable = eta_symbol;
  request.location = "cutkosky-phase-space";
  request.strategy = ResolveBuiltinCutkoskyPhaseSpaceStrategy(spec);
  return request;
}

BoundaryRequest GeneratePlannedEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const EndingDecision& decision,
    const std::string& eta_symbol) {
  ValidateProblemSpecForBoundaryGeneration(spec);
  ValidateEtaInfinityBoundaryEtaSymbol(eta_symbol);
  ValidatePlannedEtaInfinityDecisionMetadata(decision);
  ValidatePlannedEtaInfinityTerminalNodes(spec, decision);
  return GenerateBuiltinEtaInfinityBoundaryRequest(spec, eta_symbol);
}

BoundaryRequest GeneratePlannedEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  ValidateEtaInfinityBoundaryEtaSymbol(eta_symbol);
  ValidateProblemSpecForBoundaryGeneration(spec);
  const EndingDecision decision =
      PlanEndingScheme(spec, ending_scheme_name, user_defined_schemes);
  return GeneratePlannedEtaInfinityBoundaryRequest(spec, decision, eta_symbol);
}

BoundaryRequest GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const EndingDecision& decision,
    const std::string& eta_symbol) {
  ValidateProblemSpecForBoundaryGeneration(spec);
  ValidateCutkoskyPhaseSpaceBoundaryEtaSymbol(eta_symbol);
  ValidatePlannedCutkoskyPhaseSpaceDecisionMetadata(decision);
  ValidatePlannedCutkoskyPhaseSpaceTerminalNodes(spec, decision);
  return GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(spec, eta_symbol);
}

BoundaryRequest GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  ValidateCutkoskyPhaseSpaceBoundaryEtaSymbol(eta_symbol);
  ValidateProblemSpecForBoundaryGeneration(spec);
  const EndingDecision decision =
      PlanEndingScheme(spec, ending_scheme_name, user_defined_schemes);
  return GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(spec, decision, eta_symbol);
}

BoundaryRequest GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  ValidateEtaInfinityBoundaryEtaSymbol(eta_symbol);
  ValidateProblemSpecForBoundaryGeneration(spec);
  const EndingDecision decision =
      PlanAmfOptionsEndingScheme(spec, amf_options, user_defined_schemes);
  return GeneratePlannedEtaInfinityBoundaryRequest(spec, decision, eta_symbol);
}

BoundaryRequest GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol) {
  ValidateCutkoskyPhaseSpaceBoundaryEtaSymbol(eta_symbol);
  ValidateProblemSpecForBoundaryGeneration(spec);
  const EndingDecision decision =
      PlanAmfOptionsEndingScheme(spec, amf_options, user_defined_schemes);
  return GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(spec, decision, eta_symbol);
}

}  // namespace amflow
