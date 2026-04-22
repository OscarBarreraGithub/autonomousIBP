#pragma once

#include <memory>
#include <string>
#include <vector>

#include "amflow/core/boundary_data.hpp"
#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct AmfOptions;
class EndingScheme;

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
