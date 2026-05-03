#include "amflow/runtime/ending_scheme.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "amflow/core/options.hpp"
#include "amflow/runtime/boundary_generation.hpp"

namespace amflow {

namespace {

bool IsBuiltinEndingSchemeName(const std::string& name) {
  for (const auto& candidate : BuiltinEndingSchemes()) {
    if (candidate == name) {
      return true;
    }
  }
  return false;
}

bool IsCutkoskyPrescriptionVocabularyFailure(const std::string& message) {
  return message.find("family.propagators[") != std::string::npos &&
         message.find("].prescription must be one of -1 (-i0), 0 (none), or 1 (+i0)") !=
             std::string::npos;
}

bool IsMalformedCutkoskyPlanningFailure(const std::exception& error) {
  const std::string message = error.what();
  return IsCutkoskyPrescriptionVocabularyFailure(message) ||
         message.find("ending scheme Cutkosky could not derive a loop-prescription-backed "
                      "provider strategy") != std::string::npos ||
         message.find("ending scheme Cutkosky requires all cut propagators to resolve to the "
                      "same loop-prescription-backed provider strategy") != std::string::npos ||
         message.find("ending scheme Cutkosky requires all cut propagators to carry the "
                      "same raw provider strategy") != std::string::npos ||
         message.find("ending scheme Cutkosky only supports standard/cut propagators") !=
             std::string::npos ||
         message.find("ending scheme Cutkosky requires cut propagator ") !=
             std::string::npos ||
         message.find("ending scheme Cutkosky requires target ") !=
             std::string::npos ||
         message.find("ending scheme Cutkosky requires connected cut component") !=
             std::string::npos ||
         message.find("ending scheme Cutkosky requires a connected cut surface") !=
             std::string::npos;
}

std::string JoinIndices(const std::vector<std::size_t>& indices) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << indices[index];
  }
  stream << "]";
  return stream.str();
}

std::string JoinSectors(const std::vector<int>& sectors) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < sectors.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << sectors[index];
  }
  stream << "]";
  return stream.str();
}

std::string JoinNames(const std::vector<std::string>& names) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << names[index];
  }
  stream << "]";
  return stream.str();
}

std::string DescribeCutComponent(const CutkoskyPhaseSpaceCutComponent& component) {
  std::ostringstream stream;
  stream << "cuts=" << JoinIndices(component.cut_propagator_indices)
         << " loops=" << JoinNames(component.loop_momenta)
         << " active_top_level_sectors="
         << JoinSectors(component.active_top_level_sectors);
  return stream.str();
}

std::string DescribeCutComponentWithTargetLabels(
    const CutkoskyPhaseSpaceCutComponent& component) {
  std::ostringstream stream;
  stream << DescribeCutComponent(component)
         << " active_target_labels=" << JoinNames(component.active_target_labels);
  return stream.str();
}

std::string DescribeCutSupportActivation(const CutkoskyPhaseSpaceCutSupport& support) {
  std::ostringstream stream;
  stream << "active_top_level_sectors="
         << JoinSectors(support.active_top_level_sectors)
         << " active_target_labels=" << JoinNames(support.active_target_labels);
  return stream.str();
}

std::string DescribeCutComponents(
    const std::vector<CutkoskyPhaseSpaceCutComponent>& components) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < components.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << DescribeCutComponent(components[index]);
  }
  stream << "]";
  return stream.str();
}

std::string DescribeCutComponentsWithTargetLabels(
    const std::vector<CutkoskyPhaseSpaceCutComponent>& components) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t index = 0; index < components.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << DescribeCutComponentWithTargetLabels(components[index]);
  }
  stream << "]";
  return stream.str();
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

bool IsReviewedCutkoskyPhaseSpaceBareZeroMassLiteral(const std::string& trimmed) {
  return trimmed == "0" || trimmed == "+0" || trimmed == "-0";
}

bool IsReviewedCutkoskyPhaseSpaceMasslessLiteral(const std::string& value) {
  std::string trimmed = Trim(value);
  while (trimmed.size() >= 3 && trimmed.front() == '(' && trimmed.back() == ')') {
    trimmed = Trim(trimmed.substr(1, trimmed.size() - 2));
  }
  return IsReviewedCutkoskyPhaseSpaceBareZeroMassLiteral(trimmed);
}

void ValidateCutkoskyLoopPrescriptionProviderSurface(const ProblemSpec& spec) {
  if (spec.family.loop_prescriptions.empty()) {
    return;
  }

  std::optional<FeynmanPrescription> selected_cut_prescription;
  std::size_t selected_cut_index = 0;
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const Propagator& propagator = spec.family.propagators[index];
    if (propagator.kind != PropagatorKind::Cut) {
      continue;
    }

    const std::optional<FeynmanPrescription> derived_prescription =
        DerivePropagatorPrescriptionFromLoopPrescriptions(spec.family, propagator);
    if (!derived_prescription.has_value()) {
      throw std::runtime_error(
          "ending scheme Cutkosky could not derive a loop-prescription-backed provider "
          "strategy for cut propagator " +
          std::to_string(index) +
          " from family.loop_prescriptions before emitting the reviewed phase-space terminal "
          "node");
    }

    const std::optional<FeynmanPrescription> raw_prescription =
        ParseFeynmanPrescription(propagator.prescription);
    if (!raw_prescription.has_value()) {
      throw std::invalid_argument("family.propagators[" + std::to_string(index) +
                                  "].prescription must be one of -1 (-i0), 0 (none), or 1 "
                                  "(+i0)");
    }

    if (*derived_prescription != *raw_prescription) {
      throw std::runtime_error(
          "ending scheme Cutkosky requires cut propagator " +
          std::to_string(index) +
          " raw prescription to match family.loop_prescriptions before emitting the reviewed "
          "phase-space terminal node on the current reviewed provider-selection subset");
    }

    if (!selected_cut_prescription.has_value()) {
      selected_cut_prescription = *derived_prescription;
      selected_cut_index = index;
      continue;
    }

    if (*selected_cut_prescription != *derived_prescription) {
      throw std::runtime_error(
          "ending scheme Cutkosky requires all cut propagators to resolve to the same "
          "loop-prescription-backed provider strategy before emitting the reviewed "
          "phase-space terminal node on the current reviewed provider-selection subset; cut "
          "propagator " +
          std::to_string(index) + " disagrees with cut propagator " +
          std::to_string(selected_cut_index));
    }
  }
}

void ValidateCutkoskyRawPrescriptionProviderSurface(const ProblemSpec& spec) {
  if (!spec.family.loop_prescriptions.empty()) {
    return;
  }

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
      throw std::runtime_error(
          "ending scheme Cutkosky requires all cut propagators to carry the same raw provider "
          "strategy before emitting the reviewed phase-space terminal node on the current "
          "reviewed raw-prescription provider-selection subset");
    }
  }
}

std::string EtaInfinityTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::eta->infinity";
}

std::string CutkoskyPhaseSpaceTerminalNode(const ProblemSpec& spec) {
  return spec.family.name + "::cutkosky-phase-space";
}

void ValidateTraditionEndingSurface(const ProblemSpec& spec) {
  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    return;
  }

  throw std::runtime_error("ending scheme Tradition only supports the current loop-only subset; "
                           "cut propagators are present at indices " +
                           JoinIndices(cut_indices));
}

void ValidateCutkoskyEndingSurface(const ProblemSpec& spec) {
  const std::vector<std::size_t> cut_indices = CollectCutPropagatorIndices(spec);
  if (cut_indices.empty()) {
    throw std::runtime_error("ending scheme Cutkosky requires at least one cut propagator on "
                             "the current reviewed phase-space subset");
  }

  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    const PropagatorKind kind = spec.family.propagators[index].kind;
    if (kind != PropagatorKind::Standard && kind != PropagatorKind::Cut) {
      throw std::runtime_error("ending scheme Cutkosky only supports standard/cut propagators on "
                               "the current reviewed phase-space subset; propagator " +
                               std::to_string(index) + " has kind " + ToString(kind));
    }
    if (kind == PropagatorKind::Cut &&
        !IsReviewedCutkoskyPhaseSpaceMasslessLiteral(spec.family.propagators[index].mass)) {
      throw std::runtime_error(
          "ending scheme Cutkosky requires cut propagator " + std::to_string(index) +
          " to have reviewed massless phase-space support before emitting the reviewed "
          "phase-space terminal node on the current boundary-value subset; mass must be zero "
          "literal \"0\", \"+0\", or \"-0\" after trimming outer whitespace and any number "
          "of redundant outer parenthesis pairs; propagator " +
          std::to_string(index) + " has mass \"" + spec.family.propagators[index].mass + "\"");
    }
    if (!ParseFeynmanPrescription(spec.family.propagators[index].prescription).has_value()) {
      throw std::invalid_argument("family.propagators[" + std::to_string(index) +
                                  "].prescription must be one of -1 (-i0), 0 (none), or 1 "
                                  "(+i0)");
    }
  }

  const CutkoskyPhaseSpaceTopology topology =
      AnalyzeCutkoskyPhaseSpaceCutTopology(spec);
  for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
    if (!support.loop_momenta.empty()) {
      continue;
    }
    throw std::runtime_error(
        "ending scheme Cutkosky requires cut propagator " +
        std::to_string(support.propagator_index) +
        " to carry declared loop-momentum support; " +
        DescribeCutSupportActivation(support) +
        " before emitting the reviewed phase-space terminal node; no declared loop momentum "
        "support found in expression \"" +
        spec.family.propagators[support.propagator_index].expression + "\"");
  }

  if (topology.cut_components.size() > 1) {
    throw std::runtime_error(
        "ending scheme Cutkosky requires a connected cut surface before emitting the reviewed "
        "phase-space terminal node; disconnected cut components: " +
        DescribeCutComponentsWithTargetLabels(topology.cut_components));
  }

  if (spec.family.top_level_sectors.size() == 1) {
    const int sector = spec.family.top_level_sectors.front();
    for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
      if (!support.active_top_level_sectors.empty()) {
        continue;
      }
      throw std::runtime_error(
          "ending scheme Cutkosky requires cut propagator " +
          std::to_string(support.propagator_index) +
          " to be active in the single declared top-level sector " +
          std::to_string(sector) +
          " before emitting the reviewed phase-space terminal node on the current reviewed "
          "top-sector support subset");
    }
  } else if (spec.family.top_level_sectors.size() > 1) {
    for (const CutkoskyPhaseSpaceCutSupport& support : topology.cut_supports) {
      if (!support.active_top_level_sectors.empty()) {
        continue;
      }
      throw std::runtime_error(
          "ending scheme Cutkosky requires cut propagator " +
          std::to_string(support.propagator_index) +
          " to be active in at least one declared top-level sector " +
          JoinSectors(spec.family.top_level_sectors) +
          " before emitting the reviewed phase-space terminal node on the current reviewed "
          "multi-top-sector support subset");
    }
    for (const CutkoskyPhaseSpaceCutComponent& component : topology.cut_components) {
      if (!component.active_top_level_sectors.empty()) {
        continue;
      }
      throw std::runtime_error(
          "ending scheme Cutkosky requires connected cut component " +
          DescribeCutComponent(component) +
          " to share at least one declared top-level sector " +
          JoinSectors(spec.family.top_level_sectors) +
          " before emitting the reviewed phase-space terminal node on the current reviewed "
          "component top-sector support subset");
    }
  }

  for (const TargetIntegral& target : spec.targets) {
    const std::string target_label = target.Label();
    for (const CutkoskyPhaseSpaceCutComponent& component : topology.cut_components) {
      if (std::find(component.active_target_labels.begin(),
                    component.active_target_labels.end(),
                    target_label) != component.active_target_labels.end()) {
        continue;
      }
      throw std::runtime_error(
          "ending scheme Cutkosky requires target " + target_label +
          " to keep connected cut component " + DescribeCutComponent(component) +
          " with active_target_labels=" + JoinNames(component.active_target_labels) +
          " active before emitting the reviewed phase-space terminal node on the current "
          "reviewed component target-support subset");
    }
  }

  ValidateCutkoskyRawPrescriptionProviderSurface(spec);
  ValidateCutkoskyLoopPrescriptionProviderSurface(spec);
}

void ValidateUserDefinedEndingSchemeRegistry(
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  std::vector<std::string> seen_names;
  seen_names.reserve(user_defined_schemes.size());

  for (const auto& user_defined_scheme : user_defined_schemes) {
    if (!user_defined_scheme) {
      throw std::invalid_argument("user-defined ending scheme registry contains null entry");
    }

    const std::string scheme_name = user_defined_scheme->Name();
    if (IsBuiltinEndingSchemeName(scheme_name)) {
      throw std::invalid_argument("user-defined ending scheme conflicts with builtin ending "
                                  "scheme: " +
                                  scheme_name);
    }

    if (std::find(seen_names.begin(), seen_names.end(), scheme_name) != seen_names.end()) {
      throw std::invalid_argument("duplicate user-defined ending scheme: " + scheme_name);
    }
    seen_names.push_back(scheme_name);
  }
}

EndingDecision SelectEndingSchemeDecision(
    const ProblemSpec& spec,
    const std::vector<std::string>& ending_scheme_names,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  if (ending_scheme_names.empty()) {
    throw std::invalid_argument("ending-scheme list must not be empty");
  }

  for (std::size_t index = 0; index < ending_scheme_names.size(); ++index) {
    const std::shared_ptr<EndingScheme> ending_scheme =
        ResolveEndingScheme(ending_scheme_names[index], user_defined_schemes);
    try {
      return ending_scheme->Plan(spec);
    } catch (const std::exception& error) {
      if (ending_scheme_names[index] == "Cutkosky" &&
          IsMalformedCutkoskyPlanningFailure(error)) {
        throw;
      }
      if (index + 1 == ending_scheme_names.size()) {
        throw;
      }
    }
  }

  throw std::runtime_error("failed to select an ending scheme");
}

class BuiltinEndingScheme final : public EndingScheme {
 public:
  explicit BuiltinEndingScheme(std::string name) : name_(std::move(name)) {}

  std::string Name() const override { return name_; }

  EndingDecision Plan(const ProblemSpec& spec) const override {
    EndingDecision decision;
    decision.terminal_strategy = name_;
    if (name_ == "Tradition") {
      ValidateTraditionEndingSurface(spec);
      decision.terminal_nodes.push_back(EtaInfinityTerminalNode(spec));
      return decision;
    }
    if (name_ == "Cutkosky") {
      ValidateCutkoskyEndingSurface(spec);
      decision.terminal_nodes.push_back(CutkoskyPhaseSpaceTerminalNode(spec));
      return decision;
    }

    decision.terminal_nodes.push_back(EtaInfinityTerminalNode(spec));
    if (name_ == "Trivial") {
      decision.terminal_nodes.push_back(spec.family.name + "::trivial-region");
    }
    return decision;
  }

 private:
  std::string name_;
};

}  // namespace

std::vector<std::string> BuiltinEndingSchemes() {
  return {"Tradition", "Cutkosky", "SingleMass", "Trivial"};
}

std::shared_ptr<EndingScheme> MakeBuiltinEndingScheme(const std::string& name) {
  for (const auto& candidate : BuiltinEndingSchemes()) {
    if (candidate == name) {
      return std::make_shared<BuiltinEndingScheme>(name);
    }
  }
  throw std::invalid_argument("unknown ending scheme: " + name);
}

std::shared_ptr<EndingScheme> ResolveEndingScheme(
    const std::string& name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  ValidateUserDefinedEndingSchemeRegistry(user_defined_schemes);

  for (const auto& user_defined_scheme : user_defined_schemes) {
    if (user_defined_scheme->Name() == name) {
      return user_defined_scheme;
    }
  }

  return MakeBuiltinEndingScheme(name);
}

EndingDecision PlanEndingScheme(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  const std::shared_ptr<EndingScheme> ending_scheme =
      ResolveEndingScheme(ending_scheme_name, user_defined_schemes);
  return ending_scheme->Plan(spec);
}

EndingDecision PlanEndingSchemeList(
    const ProblemSpec& spec,
    const std::vector<std::string>& ending_scheme_names,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  return SelectEndingSchemeDecision(spec, ending_scheme_names, user_defined_schemes);
}

EndingDecision PlanAmfOptionsEndingScheme(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes) {
  return PlanEndingSchemeList(spec,
                              amf_options.ending_schemes,
                              user_defined_schemes);
}

}  // namespace amflow
