#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "amflow/core/boundary_data.hpp"
#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct AmfOptions;
class EndingScheme;

struct CutkoskyPhaseSpaceCutSupport {
  std::size_t propagator_index = 0;
  std::vector<std::string> loop_momenta;
};

struct CutkoskyPhaseSpaceCutComponent {
  std::vector<std::size_t> cut_propagator_indices;
  std::vector<std::string> loop_momenta;
};

struct CutkoskyPhaseSpaceTopology {
  std::vector<CutkoskyPhaseSpaceCutSupport> cut_supports;
  std::vector<CutkoskyPhaseSpaceCutComponent> cut_components;
};

CutkoskyPhaseSpaceTopology AnalyzeCutkoskyPhaseSpaceCutTopology(
    const FamilyDefinition& family);

BoundaryRequest GenerateBuiltinEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& eta_symbol = "eta");
BoundaryRequest GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& eta_symbol = "eta");
BoundaryRequest GeneratePlannedEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol = "eta");
BoundaryRequest GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const std::string& ending_scheme_name,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol = "eta");
BoundaryRequest GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol = "eta");
BoundaryRequest GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const std::string& eta_symbol = "eta");

}  // namespace amflow
