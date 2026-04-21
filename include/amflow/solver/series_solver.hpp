#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "amflow/core/de_system.hpp"
#include "amflow/solver/boundary_request.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"
#include "amflow/solver/precision_policy.hpp"

namespace amflow {

struct AmfOptions;
struct ArtifactLayout;
class EtaMode;
struct EtaInsertionDecision;
struct ParsedMasterList;
struct ProblemSpec;
struct ReductionOptions;

struct SeriesPatch {
  std::string center;
  int order = 0;
  std::vector<std::string> basis_functions;
  std::vector<std::string> coefficients;
};

struct ScalarFrobeniusSeriesPatch {
  std::string center;
  std::string indicial_exponent;
  int order = 0;
  std::vector<std::string> basis_functions;
  std::vector<std::string> coefficients;
};

struct UpperTriangularMatrixSeriesPatch {
  std::string center;
  int order = 0;
  std::vector<std::string> basis_functions;
  std::vector<ExactRationalMatrix> coefficient_matrices;
};

struct UpperTriangularMatrixFrobeniusSeriesPatch {
  std::string center;
  std::vector<std::string> indicial_exponents;
  int order = 0;
  std::vector<std::string> basis_functions;
  std::vector<ExactRationalMatrix> coefficient_matrices;
};

struct ScalarSeriesPatchOverlapDiagnostics {
  ExactRational lambda;
  ExactRational mismatch;
};

struct UpperTriangularMatrixSeriesPatchOverlapDiagnostics {
  ExactRationalMatrix match_matrix;
  ExactRationalMatrix mismatch;
};

struct SolverDiagnostics {
  bool success = false;
  double residual_norm = 1.0;
  double overlap_mismatch = 1.0;
  std::string failure_code;
  std::string summary;
};

struct SolveRequest {
  DESystem system;
  std::vector<BoundaryRequest> boundary_requests;
  std::vector<BoundaryCondition> boundary_conditions;
  std::string start_location;
  std::string target_location;
  PrecisionPolicy precision_policy;
  std::optional<AmfSolveRuntimePolicy> amf_runtime_policy;
  // Wrapper-owned AMFlow D0 intent; current DE construction and the bootstrap
  // solver do not interpret these fields yet.
  std::optional<std::string> amf_requested_d0;
  std::optional<std::string> amf_requested_dimension_expression;
  int requested_digits = 50;
};

class SeriesSolver {
 public:
  virtual ~SeriesSolver() = default;

  virtual SolverDiagnostics Solve(const SolveRequest& request) const = 0;
};

class BootstrapSeriesSolver final : public SeriesSolver {
 public:
  SolverDiagnostics Solve(const SolveRequest& request) const override;
};

SeriesPatch GenerateScalarRegularPointSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    int order,
    const NumericEvaluationPoint& passive_bindings);

ScalarFrobeniusSeriesPatch GenerateScalarFrobeniusSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    int order,
    const NumericEvaluationPoint& passive_bindings);

UpperTriangularMatrixSeriesPatch GenerateUpperTriangularRegularPointSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    int order,
    const NumericEvaluationPoint& passive_bindings);

UpperTriangularMatrixFrobeniusSeriesPatch
GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    int order,
    const NumericEvaluationPoint& passive_bindings);

ExactRational EvaluateScalarSeriesPatchResidual(
    const DESystem& system,
    const std::string& variable_name,
    const SeriesPatch& patch,
    const std::string& point_expression,
    const NumericEvaluationPoint& passive_bindings);

ScalarSeriesPatchOverlapDiagnostics MatchScalarSeriesPatches(
    const std::string& variable_name,
    const SeriesPatch& left_patch,
    const SeriesPatch& right_patch,
    const std::string& match_point_expression,
    const std::string& check_point_expression,
    const NumericEvaluationPoint& passive_bindings);

ExactRationalMatrix EvaluateUpperTriangularMatrixSeriesPatchResidual(
    const DESystem& system,
    const std::string& variable_name,
    const UpperTriangularMatrixSeriesPatch& patch,
    const std::string& point_expression,
    const NumericEvaluationPoint& passive_bindings);

UpperTriangularMatrixSeriesPatchOverlapDiagnostics MatchUpperTriangularMatrixSeriesPatches(
    const std::string& variable_name,
    const UpperTriangularMatrixSeriesPatch& left_patch,
    const UpperTriangularMatrixSeriesPatch& right_patch,
    const std::string& match_point_expression,
    const std::string& check_point_expression,
    const NumericEvaluationPoint& passive_bindings);

SolverDiagnostics SolveInvariantGeneratedSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& invariant_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits);

SolverDiagnostics SolveInvariantGeneratedSeriesList(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& invariant_names,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits);

SolverDiagnostics SolveEtaGeneratedSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveEtaModePlannedSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaMode& eta_mode,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveBuiltinEtaModeSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& eta_mode_name,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveBuiltinEtaModeListSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& eta_mode_names,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveAmfOptionsEtaModeSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const AmfOptions& amf_options,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveResolvedEtaModeSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::string& eta_mode_name,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveResolvedEtaModeListSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const std::vector<std::string>& eta_mode_names,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveAmfOptionsEtaModeSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    int requested_digits,
    const std::string& eta_symbol = "eta");

SolverDiagnostics SolveDifferentialEquation(const SolveRequest& request);

std::unique_ptr<SeriesSolver> MakeBootstrapSeriesSolver();

}  // namespace amflow
