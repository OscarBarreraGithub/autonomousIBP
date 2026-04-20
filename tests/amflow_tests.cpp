#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "amflow/core/options.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/de/eta_reduction_execution.hpp"
#include "amflow/de/eta_reduction_preparation.hpp"
#include "amflow/de/invariant_derivative_generation.hpp"
#include "amflow/de/invariant_reduction_execution.hpp"
#include "amflow/de/invariant_reduction_preparation.hpp"
#include "amflow/de/reduction_assembly.hpp"
#include "amflow/io/problem_spec_io.hpp"
#include "amflow/io/sample_data.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/runtime/auxiliary_family.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/boundary_generation.hpp"
#include "amflow/runtime/ending_scheme.hpp"
#include "amflow/runtime/eta_mode.hpp"
#include "amflow/solver/boundary_provider.hpp"
#include "amflow/solver/coefficient_evaluator.hpp"
#include "amflow/solver/singular_point_analysis.hpp"
#include "amflow/solver/series_solver.hpp"

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

bool ContainsSubstring(const std::vector<std::string>& values, const std::string& needle) {
  for (const auto& value : values) {
    if (value.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void ReplaceFirst(std::string& value,
                  const std::string& needle,
                  const std::string& replacement) {
  const std::size_t position = value.find(needle);
  if (position == std::string::npos) {
    throw std::runtime_error("failed to find expected YAML fragment: " + needle);
  }
  value.replace(position, needle.size(), replacement);
}

void InsertBefore(std::string& value,
                  const std::string& needle,
                  const std::string& insertion) {
  const std::size_t position = value.find(needle);
  if (position == std::string::npos) {
    throw std::runtime_error("failed to find insertion point: " + needle);
  }
  value.insert(position, insertion);
}

void ExpectParseFailure(const std::string& yaml,
                        const std::string& expected_substring,
                        const std::string& message) {
  try {
    static_cast<void>(amflow::ParseProblemSpecYaml(yaml));
  } catch (const std::runtime_error& error) {
    Expect(std::string(error.what()).find(expected_substring) != std::string::npos, message);
    return;
  }
  throw std::runtime_error(message);
}

template <typename Callable>
void ExpectRuntimeError(Callable&& callable,
                        const std::string& expected_substring,
                        const std::string& message) {
  try {
    callable();
  } catch (const std::runtime_error& error) {
    Expect(std::string(error.what()).find(expected_substring) != std::string::npos, message);
    return;
  }
  throw std::runtime_error(message);
}

template <typename Callable>
void ExpectInvalidArgument(Callable&& callable,
                           const std::string& expected_substring,
                           const std::string& message) {
  try {
    callable();
  } catch (const std::invalid_argument& error) {
    Expect(std::string(error.what()).find(expected_substring) != std::string::npos, message);
    return;
  }
  throw std::runtime_error(message);
}

template <typename Callable>
void ExpectBoundaryUnsolved(Callable&& callable,
                            const std::string& expected_substring,
                            const std::string& message) {
  try {
    callable();
  } catch (const amflow::BoundaryUnsolvedError& error) {
    Expect(std::string(error.what()).find(expected_substring) != std::string::npos, message);
    Expect(std::string(error.failure_code()) == "boundary_unsolved", message);
    return;
  }
  throw std::runtime_error(message);
}

template <typename Callable>
std::string CaptureInvalidArgumentMessage(Callable&& callable,
                                          const std::string& message) {
  try {
    static_cast<void>(callable());
  } catch (const std::invalid_argument& error) {
    return error.what();
  }
  throw std::runtime_error(message);
}

template <typename Callable>
std::string CaptureRuntimeErrorMessage(Callable&& callable,
                                       const std::string& message) {
  try {
    static_cast<void>(callable());
  } catch (const std::runtime_error& error) {
    return error.what();
  }
  throw std::runtime_error(message);
}

template <typename Callable>
std::string CaptureBoundaryUnsolvedMessage(Callable&& callable,
                                           const std::string& message) {
  try {
    static_cast<void>(callable());
  } catch (const amflow::BoundaryUnsolvedError& error) {
    return error.what();
  }
  throw std::runtime_error(message);
}

void ExpectBranchLoopBootstrapBlockerMessage(const std::string& message,
                                             const std::string& mode_name,
                                             const std::string& context) {
  const std::vector<std::string> expected_substrings = {
      "eta mode " + mode_name + " is blocked in bootstrap",
      "internal eta-topology prereq snapshot collected the current family/kinematics surface",
      "Branch/Loop candidate analysis is unavailable for this input",
      "topology_prereq_bridge={",
      "loopnum=",
      "cutvar=",
      "pres=",
      "missing_fields=[",
  };
  for (const auto& expected_substring : expected_substrings) {
    Expect(message.find(expected_substring) != std::string::npos,
           context + ": missing blocker substring: " + expected_substring);
  }
  Expect(message.find("nontrivial_prescriptions=") == std::string::npos,
         context + ": blocker should not overclaim nontrivial prescription classification");
  Expect(message.find("nontrivial_prescriptions_present=") == std::string::npos,
         context + ": blocker should not advertise interpreted prescription availability");
}

void ExpectBranchLoopBootstrapReason(const amflow::ProblemSpec& spec,
                                     const std::string& reason,
                                     const std::string& context) {
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const auto mode = amflow::MakeBuiltinEtaMode(mode_name);
    const std::string message = CaptureRuntimeErrorMessage(
        [&mode, &spec]() {
          static_cast<void>(mode->Plan(spec));
        },
        context);
    ExpectBranchLoopBootstrapBlockerMessage(message, mode_name, context);
    Expect(message.find(reason) != std::string::npos,
           context + ": missing blocker reason: " + reason);
  }
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("failed to open file: " + path.string());
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

std::filesystem::path FreshTempDir(const std::string& label) {
  const std::filesystem::path path = std::filesystem::temp_directory_path() / label;
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}

std::string ShellSingleQuote(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

std::filesystem::path CurrentBuildBinaryPath(const std::string& name) {
  const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe");
  return self.parent_path() / name;
}

int RunShellCommand(const std::string& command) {
  return std::system(command.c_str());
}

std::filesystem::path TestDataRoot() {
  return std::filesystem::path(AMFLOW_SOURCE_DIR) / "tests/data";
}

amflow::ReductionOptions MakeKiraReductionOptions() {
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;
  return options;
}

std::size_t CountString(const std::vector<std::string>& values, const std::string& needle) {
  return static_cast<std::size_t>(std::count(values.begin(), values.end(), needle));
}

bool SameMasterIntegral(const amflow::MasterIntegral& lhs, const amflow::MasterIntegral& rhs) {
  return lhs.family == rhs.family && lhs.indices == rhs.indices && lhs.label == rhs.label;
}

bool SameDifferentiationVariable(const amflow::DifferentiationVariable& lhs,
                                 const amflow::DifferentiationVariable& rhs) {
  return lhs.name == rhs.name && lhs.kind == rhs.kind;
}

bool SameBoundaryCondition(const amflow::BoundaryCondition& lhs,
                           const amflow::BoundaryCondition& rhs) {
  return lhs.variable == rhs.variable && lhs.location == rhs.location &&
         lhs.values == rhs.values && lhs.strategy == rhs.strategy;
}

bool SameBoundaryRequest(const amflow::BoundaryRequest& lhs,
                         const amflow::BoundaryRequest& rhs) {
  return lhs.variable == rhs.variable && lhs.location == rhs.location &&
         lhs.strategy == rhs.strategy;
}

bool SameDESystem(const amflow::DESystem& lhs, const amflow::DESystem& rhs) {
  if (lhs.masters.size() != rhs.masters.size() || lhs.variables.size() != rhs.variables.size() ||
      lhs.coefficient_matrices != rhs.coefficient_matrices ||
      lhs.singular_points != rhs.singular_points) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.masters.size(); ++index) {
    if (!SameMasterIntegral(lhs.masters[index], rhs.masters[index])) {
      return false;
    }
  }
  for (std::size_t index = 0; index < lhs.variables.size(); ++index) {
    if (!SameDifferentiationVariable(lhs.variables[index], rhs.variables[index])) {
      return false;
    }
  }
  return true;
}

bool SamePrecisionPolicy(const amflow::PrecisionPolicy& lhs, const amflow::PrecisionPolicy& rhs) {
  return lhs.working_precision == rhs.working_precision &&
         lhs.chop_precision == rhs.chop_precision &&
         lhs.rationalize_precision == rhs.rationalize_precision &&
         lhs.escalation_step == rhs.escalation_step &&
         lhs.max_working_precision == rhs.max_working_precision &&
         lhs.x_order == rhs.x_order && lhs.x_order_step == rhs.x_order_step;
}

bool SameSolveRequest(const amflow::SolveRequest& lhs, const amflow::SolveRequest& rhs) {
  if (!SameDESystem(lhs.system, rhs.system) ||
      lhs.boundary_requests.size() != rhs.boundary_requests.size() ||
      lhs.boundary_conditions.size() != rhs.boundary_conditions.size() ||
      lhs.start_location != rhs.start_location ||
      lhs.target_location != rhs.target_location ||
      !SamePrecisionPolicy(lhs.precision_policy, rhs.precision_policy) ||
      lhs.requested_digits != rhs.requested_digits) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.boundary_requests.size(); ++index) {
    if (!SameBoundaryRequest(lhs.boundary_requests[index], rhs.boundary_requests[index])) {
      return false;
    }
  }
  for (std::size_t index = 0; index < lhs.boundary_conditions.size(); ++index) {
    if (!SameBoundaryCondition(lhs.boundary_conditions[index], rhs.boundary_conditions[index])) {
      return false;
    }
  }
  return true;
}

bool SameSolverDiagnostics(const amflow::SolverDiagnostics& lhs,
                           const amflow::SolverDiagnostics& rhs) {
  return lhs.success == rhs.success && lhs.residual_norm == rhs.residual_norm &&
         lhs.overlap_mismatch == rhs.overlap_mismatch &&
         lhs.failure_code == rhs.failure_code && lhs.summary == rhs.summary;
}

bool SameExactRationalMatrix(const amflow::ExactRationalMatrix& lhs,
                             const amflow::ExactRationalMatrix& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    if (lhs[row].size() != rhs[row].size()) {
      return false;
    }
    for (std::size_t column = 0; column < lhs[row].size(); ++column) {
      if (lhs[row][column] != rhs[row][column]) {
        return false;
      }
    }
  }
  return true;
}

bool SameFiniteSingularPoints(const std::vector<amflow::FiniteSingularPoint>& lhs,
                              const std::vector<amflow::FiniteSingularPoint>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    if (lhs[index].location != rhs[index].location) {
      return false;
    }
  }
  return true;
}

bool SameSeriesPatch(const amflow::SeriesPatch& lhs, const amflow::SeriesPatch& rhs) {
  return lhs.center == rhs.center && lhs.order == rhs.order &&
         lhs.basis_functions == rhs.basis_functions && lhs.coefficients == rhs.coefficients;
}

bool SameScalarFrobeniusSeriesPatch(const amflow::ScalarFrobeniusSeriesPatch& lhs,
                                    const amflow::ScalarFrobeniusSeriesPatch& rhs) {
  return lhs.center == rhs.center &&
         lhs.indicial_exponent == rhs.indicial_exponent &&
         lhs.order == rhs.order &&
         lhs.basis_functions == rhs.basis_functions &&
         lhs.coefficients == rhs.coefficients;
}

bool SameExactRationalMatrixVector(const std::vector<amflow::ExactRationalMatrix>& lhs,
                                   const std::vector<amflow::ExactRationalMatrix>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    if (!SameExactRationalMatrix(lhs[index], rhs[index])) {
      return false;
    }
  }
  return true;
}

bool SameUpperTriangularMatrixSeriesPatch(const amflow::UpperTriangularMatrixSeriesPatch& lhs,
                                          const amflow::UpperTriangularMatrixSeriesPatch& rhs) {
  return lhs.center == rhs.center && lhs.order == rhs.order &&
         lhs.basis_functions == rhs.basis_functions &&
         SameExactRationalMatrixVector(lhs.coefficient_matrices, rhs.coefficient_matrices);
}

bool SameUpperTriangularMatrixFrobeniusSeriesPatch(
    const amflow::UpperTriangularMatrixFrobeniusSeriesPatch& lhs,
    const amflow::UpperTriangularMatrixFrobeniusSeriesPatch& rhs) {
  return lhs.center == rhs.center &&
         lhs.indicial_exponents == rhs.indicial_exponents &&
         lhs.order == rhs.order &&
         lhs.basis_functions == rhs.basis_functions &&
         SameExactRationalMatrixVector(lhs.coefficient_matrices, rhs.coefficient_matrices);
}

bool ContainsFiniteSingularPoint(const std::vector<amflow::FiniteSingularPoint>& points,
                                 const std::string& expected_location) {
  return std::any_of(points.begin(),
                     points.end(),
                     [&expected_location](const amflow::FiniteSingularPoint& point) {
                       return point.location.ToString() == expected_location;
                     });
}

void ExpectRationalString(const amflow::ExactRational& value,
                          const std::string& expected,
                          const std::string& message) {
  Expect(value.ToString() == expected,
         message + "; expected " + expected + ", got " + value.ToString());
}

void ExpectStringVector(const std::vector<std::string>& values,
                        const std::vector<std::string>& expected,
                        const std::string& message) {
  Expect(values == expected, message);
}

amflow::ExactRational ParseExactRational(const std::string& expression) {
  return amflow::EvaluateCoefficientExpression(expression, {});
}

amflow::ExactRational AddExactRational(const amflow::ExactRational& lhs,
                                       const amflow::ExactRational& rhs) {
  return ParseExactRational("(" + lhs.ToString() + ")+(" + rhs.ToString() + ")");
}

amflow::ExactRational SubtractExactRational(const amflow::ExactRational& lhs,
                                            const amflow::ExactRational& rhs) {
  return ParseExactRational("(" + lhs.ToString() + ")-(" + rhs.ToString() + ")");
}

amflow::ExactRational MultiplyExactRational(const amflow::ExactRational& lhs,
                                            const amflow::ExactRational& rhs) {
  return ParseExactRational("(" + lhs.ToString() + ")*(" + rhs.ToString() + ")");
}

amflow::ExactRationalMatrix MakeExactRationalMatrix(
    const std::initializer_list<std::initializer_list<const char*>>& rows) {
  amflow::ExactRationalMatrix matrix;
  matrix.reserve(rows.size());
  for (const auto& row : rows) {
    std::vector<amflow::ExactRational> parsed_row;
    parsed_row.reserve(row.size());
    for (const char* cell : row) {
      parsed_row.push_back(ParseExactRational(cell));
    }
    matrix.push_back(std::move(parsed_row));
  }
  return matrix;
}

amflow::ExactRationalMatrix MakeZeroExactRationalMatrix(const std::size_t dimension) {
  return amflow::ExactRationalMatrix(
      dimension, std::vector<amflow::ExactRational>(dimension, {"0", "1"}));
}

amflow::ExactRationalMatrix AddExactRationalMatrices(const amflow::ExactRationalMatrix& lhs,
                                                     const amflow::ExactRationalMatrix& rhs) {
  amflow::ExactRationalMatrix result = MakeZeroExactRationalMatrix(lhs.size());
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    for (std::size_t column = 0; column < lhs[row].size(); ++column) {
      result[row][column] = AddExactRational(lhs[row][column], rhs[row][column]);
    }
  }
  return result;
}

amflow::ExactRationalMatrix MultiplyExactRationalMatrices(const amflow::ExactRationalMatrix& lhs,
                                                          const amflow::ExactRationalMatrix& rhs) {
  amflow::ExactRationalMatrix result = MakeZeroExactRationalMatrix(lhs.size());
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    for (std::size_t inner = 0; inner < lhs[row].size(); ++inner) {
      if (lhs[row][inner].IsZero()) {
        continue;
      }
      for (std::size_t column = 0; column < rhs[inner].size(); ++column) {
        if (rhs[inner][column].IsZero()) {
          continue;
        }
        result[row][column] = AddExactRational(
            result[row][column],
            MultiplyExactRational(lhs[row][inner], rhs[inner][column]));
      }
    }
  }
  return result;
}

amflow::ExactRationalMatrix ScaleExactRationalMatrix(const amflow::ExactRationalMatrix& matrix,
                                                     const std::size_t factor) {
  amflow::ExactRationalMatrix result = MakeZeroExactRationalMatrix(matrix.size());
  const amflow::ExactRational scale = {std::to_string(factor), "1"};
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      result[row][column] = MultiplyExactRational(matrix[row][column], scale);
    }
  }
  return result;
}

void ExpectExactRationalMatrix(const amflow::ExactRationalMatrix& value,
                               const amflow::ExactRationalMatrix& expected,
                               const std::string& message) {
  Expect(SameExactRationalMatrix(value, expected), message);
}

void ExpectExactRationalMatrixVector(const std::vector<amflow::ExactRationalMatrix>& values,
                                     const std::vector<amflow::ExactRationalMatrix>& expected,
                                     const std::string& message) {
  Expect(SameExactRationalMatrixVector(values, expected), message);
}

void ExpectMatrixPatchRecurrence(
    const amflow::UpperTriangularMatrixSeriesPatch& patch,
    const std::vector<amflow::ExactRationalMatrix>& local_degree_matrices,
    const std::string& message) {
  Expect(patch.coefficient_matrices.size() == local_degree_matrices.size(), message);
  const std::size_t dimension =
      patch.coefficient_matrices.empty() ? 0 : patch.coefficient_matrices.front().size();
  for (std::size_t degree = 0; degree + 1 < patch.coefficient_matrices.size(); ++degree) {
    amflow::ExactRationalMatrix recurrence_rhs = MakeZeroExactRationalMatrix(dimension);
    for (std::size_t coefficient_degree = 0; coefficient_degree <= degree; ++coefficient_degree) {
      recurrence_rhs = AddExactRationalMatrices(
          recurrence_rhs,
          MultiplyExactRationalMatrices(local_degree_matrices[coefficient_degree],
                                        patch.coefficient_matrices[degree - coefficient_degree]));
    }
    const amflow::ExactRationalMatrix recurrence_lhs =
        ScaleExactRationalMatrix(patch.coefficient_matrices[degree + 1], degree + 1);
    ExpectExactRationalMatrix(recurrence_lhs,
                              recurrence_rhs,
                              message + " at degree " + std::to_string(degree));
  }
}

void ExpectMatrixFrobeniusPatchRecurrence(
    const amflow::UpperTriangularMatrixFrobeniusSeriesPatch& patch,
    const std::vector<amflow::ExactRationalMatrix>& regular_tail_degree_matrices,
    const std::string& message) {
  Expect(patch.coefficient_matrices.size() == regular_tail_degree_matrices.size(), message);
  const std::size_t dimension =
      patch.coefficient_matrices.empty() ? 0 : patch.coefficient_matrices.front().size();
  Expect(patch.indicial_exponents.size() == dimension, message);

  std::vector<amflow::ExactRational> indicial_exponents;
  indicial_exponents.reserve(patch.indicial_exponents.size());
  for (const std::string& exponent : patch.indicial_exponents) {
    indicial_exponents.push_back(ParseExactRational(exponent));
  }

  for (std::size_t degree = 0; degree + 1 < patch.coefficient_matrices.size(); ++degree) {
    amflow::ExactRationalMatrix recurrence_rhs = MakeZeroExactRationalMatrix(dimension);
    for (std::size_t coefficient_degree = 0; coefficient_degree <= degree; ++coefficient_degree) {
      recurrence_rhs = AddExactRationalMatrices(
          recurrence_rhs,
          MultiplyExactRationalMatrices(regular_tail_degree_matrices[coefficient_degree],
                                        patch.coefficient_matrices[degree - coefficient_degree]));
    }

    amflow::ExactRationalMatrix recurrence_lhs = MakeZeroExactRationalMatrix(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
      for (std::size_t column = 0; column < dimension; ++column) {
        const amflow::ExactRational denominator = AddExactRational(
            {std::to_string(degree + 1), "1"},
            SubtractExactRational(indicial_exponents[column], indicial_exponents[row]));
        recurrence_lhs[row][column] =
            MultiplyExactRational(denominator, patch.coefficient_matrices[degree + 1][row][column]);
      }
    }

    ExpectExactRationalMatrix(recurrence_lhs,
                              recurrence_rhs,
                              message + " at degree " + std::to_string(degree));
  }
}

bool SameEtaInsertionDecision(const amflow::EtaInsertionDecision& lhs,
                              const amflow::EtaInsertionDecision& rhs) {
  return lhs.mode_name == rhs.mode_name &&
         lhs.selected_propagator_indices == rhs.selected_propagator_indices &&
         lhs.selected_propagators == rhs.selected_propagators &&
         lhs.explanation == rhs.explanation;
}

bool SameEndingDecision(const amflow::EndingDecision& lhs, const amflow::EndingDecision& rhs) {
  return lhs.terminal_strategy == rhs.terminal_strategy &&
         lhs.terminal_nodes == rhs.terminal_nodes;
}

enum class PlanningFailureKind {
  RuntimeError,
  InvalidArgument,
};

amflow::PrecisionPolicy MakeDistinctPrecisionPolicy() {
  amflow::PrecisionPolicy policy;
  policy.working_precision = 130;
  policy.chop_precision = 27;
  policy.rationalize_precision = 111;
  policy.escalation_step = 14;
  policy.max_working_precision = 290;
  policy.x_order = 77;
  policy.x_order_step = 9;
  return policy;
}

amflow::SolverDiagnostics MakeRequestDrivenEquivalenceHarnessDiagnostics(
    const amflow::SolveRequest& request);

class RecordingSeriesSolver final : public amflow::SeriesSolver {
 public:
  bool use_request_driven_diagnostics = false;
  amflow::SolverDiagnostics returned_diagnostics;

  amflow::SolverDiagnostics Solve(const amflow::SolveRequest& request) const override {
    ++call_count_;
    has_request_ = true;
    last_request_ = request;
    if (use_request_driven_diagnostics) {
      return MakeRequestDrivenEquivalenceHarnessDiagnostics(request);
    }
    return returned_diagnostics;
  }

  int call_count() const { return call_count_; }

  const amflow::SolveRequest& last_request() const {
    if (!has_request_) {
      throw std::runtime_error("recording series solver has no recorded request");
    }
    return last_request_;
  }

 private:
  mutable int call_count_ = 0;
  mutable bool has_request_ = false;
  mutable amflow::SolveRequest last_request_;
};

class RecordingSeriesSolverSequence final : public amflow::SeriesSolver {
 public:
  std::vector<amflow::SolverDiagnostics> returned_diagnostics;

  amflow::SolverDiagnostics Solve(const amflow::SolveRequest& request) const override {
    ++call_count_;
    seen_requests_.push_back(request);
    if (static_cast<std::size_t>(call_count_) > returned_diagnostics.size()) {
      throw std::runtime_error("recording series solver sequence has no diagnostic for call " +
                               std::to_string(call_count_));
    }
    return returned_diagnostics[static_cast<std::size_t>(call_count_ - 1)];
  }

  int call_count() const { return call_count_; }

  const std::vector<amflow::SolveRequest>& seen_requests() const { return seen_requests_; }

 private:
 mutable int call_count_ = 0;
  mutable std::vector<amflow::SolveRequest> seen_requests_;
};

class RecordingEtaMode final : public amflow::EtaMode {
 public:
  explicit RecordingEtaMode(const amflow::EtaInsertionDecision& returned_decision,
                            const std::string& name = "Recording",
                            const std::string& failure_message = "",
                            const PlanningFailureKind failure_kind =
                                PlanningFailureKind::RuntimeError)
      : returned_decision_(returned_decision),
        name_(name),
        failure_message_(failure_message),
        failure_kind_(failure_kind) {}

  std::string Name() const override { return name_; }

  amflow::EtaInsertionDecision Plan(const amflow::ProblemSpec& spec) const override {
    ++call_count_;
    last_planned_spec_yaml_ = amflow::SerializeProblemSpecYaml(spec);
    if (!failure_message_.empty()) {
      if (failure_kind_ == PlanningFailureKind::InvalidArgument) {
        throw std::invalid_argument(failure_message_);
      }
      throw std::runtime_error(failure_message_);
    }
    return returned_decision_;
  }

  int call_count() const { return call_count_; }

  const std::string& last_planned_spec_yaml() const {
    if (call_count_ == 0) {
      throw std::runtime_error("recording eta mode has no recorded plan input");
    }
    return last_planned_spec_yaml_;
  }

 private:
  amflow::EtaInsertionDecision returned_decision_;
  std::string name_;
  std::string failure_message_;
  PlanningFailureKind failure_kind_;
  mutable int call_count_ = 0;
  mutable std::string last_planned_spec_yaml_;
};

class RecordingEndingScheme final : public amflow::EndingScheme {
 public:
  explicit RecordingEndingScheme(const amflow::EndingDecision& returned_decision,
                                 const std::string& name = "Recording",
                                 const std::string& failure_message = "",
                                 const PlanningFailureKind failure_kind =
                                     PlanningFailureKind::RuntimeError)
      : returned_decision_(returned_decision),
        name_(name),
        failure_message_(failure_message),
        failure_kind_(failure_kind) {}

  std::string Name() const override { return name_; }

  amflow::EndingDecision Plan(const amflow::ProblemSpec& spec) const override {
    ++call_count_;
    last_planned_spec_yaml_ = amflow::SerializeProblemSpecYaml(spec);
    if (!failure_message_.empty()) {
      if (failure_kind_ == PlanningFailureKind::InvalidArgument) {
        throw std::invalid_argument(failure_message_);
      }
      throw std::runtime_error(failure_message_);
    }
    return returned_decision_;
  }

  int call_count() const { return call_count_; }

  const std::string& last_planned_spec_yaml() const {
    if (call_count_ == 0) {
      throw std::runtime_error("recording ending scheme has no recorded plan input");
    }
    return last_planned_spec_yaml_;
  }

 private:
  amflow::EndingDecision returned_decision_;
  std::string name_;
  std::string failure_message_;
  PlanningFailureKind failure_kind_;
  mutable int call_count_ = 0;
  mutable std::string last_planned_spec_yaml_;
};

amflow::AuxiliaryFamilyTransformResult MakeEtaGeneratedHappyTransform() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {0, 6};
  decision.selected_propagators = {
      spec.family.propagators[0].expression,
      spec.family.propagators[6].expression,
  };
  return amflow::ApplyEtaInsertion(spec, decision);
}

amflow::EtaInsertionDecision MakeEtaGeneratedHappyDecision() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {0, 6};
  decision.selected_propagators = {
      spec.family.propagators[0].expression,
      spec.family.propagators[6].expression,
  };
  return decision;
}

amflow::ProblemSpec MakeBuiltinAllEtaHappySpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  for (std::size_t index = 1; index <= 5; ++index) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Auxiliary;
  }
  return spec;
}

amflow::ProblemSpec MakeBuiltinPropagatorMixedSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[1].kind = amflow::PropagatorKind::Auxiliary;
  spec.family.propagators[4].kind = amflow::PropagatorKind::Auxiliary;
  return spec;
}

amflow::ProblemSpec MakeBuiltinPropagatorMassBoundarySpec() {
  amflow::ProblemSpec spec = MakeBuiltinPropagatorMixedSpec();
  spec.family.propagators[2].mass = "msq";
  return spec;
}

amflow::ProblemSpec MakeBuiltinMassHappySpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[0].mass = "msq";
  spec.family.propagators[6].mass = "msq";
  spec.family.propagators[2].mass = "t-msq";
  return spec;
}

amflow::ProblemSpec MakeBuiltinMassTrimEqualitySpec() {
  amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  spec.family.propagators[0].mass = " msq ";
  return spec;
}

void UseRecoveredMassPreferenceScalarProductRules(amflow::ProblemSpec& spec) {
  spec.kinematics.scalar_product_rules = {
      {"p1*p1", "0"},
      {"p2*p2", "0"},
      {"p3*p3", "msq"},
      {"p1*p2", "s/2"},
      {"p1*p3", "(t-msq)/2"},
      {"p2*p3", "(msq-s-t)/2"},
  };
}

amflow::ProblemSpec MakeBuiltinMassPreferenceSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  UseRecoveredMassPreferenceScalarProductRules(spec);
  spec.family.propagators[0].mass = "t-msq";
  spec.family.propagators[6].mass = "t-msq";
  spec.family.propagators[2].mass = "msq";
  spec.family.propagators[5].mass = "msq";
  return spec;
}

amflow::ProblemSpec MakeBuiltinMassDependentOnlySpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  UseRecoveredMassPreferenceScalarProductRules(spec);
  spec.family.propagators[0].mass = "t-msq";
  spec.family.propagators[6].mass = "t-msq";
  spec.family.propagators[2].mass = "s+t";
  spec.family.propagators[5].mass = "s+t";
  return spec;
}

amflow::ProblemSpec MakeBuiltinMassMixedAuxiliarySharedMassSpec() {
  amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  spec.family.propagators[1].kind = amflow::PropagatorKind::Auxiliary;
  spec.family.propagators[1].mass = "msq";
  return spec;
}

amflow::ProblemSpec MakeBuiltinMassAuxiliaryOnlyNonzeroSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[1].kind = amflow::PropagatorKind::Auxiliary;
  spec.family.propagators[1].mass = "msq";
  spec.family.propagators[4].kind = amflow::PropagatorKind::Auxiliary;
  spec.family.propagators[4].mass = "msq";
  return spec;
}

amflow::ProblemSpec MakeBuiltinMassOverSelectionSpec() {
  amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  spec.family.propagators[2].mass = "m2";
  spec.family.propagators[3].mass = "m2";
  return spec;
}

amflow::ProblemSpec MakeBuiltinAllAuxiliarySpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  for (auto& propagator : spec.family.propagators) {
    propagator.kind = amflow::PropagatorKind::Auxiliary;
  }
  return spec;
}

amflow::ProblemSpec MakeUnsupportedBranchLoopGrammarSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[0].expression = "s*((k1)^2)";
  return spec;
}

amflow::ProblemSpec MakeRepeatedSignBranchLoopGrammarSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[5].expression = "(k1--k2)^2";
  return spec;
}

amflow::ProblemSpec MakeTooManyPropagatorsForTopSectorMaskSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  while (spec.family.propagators.size() <=
         static_cast<std::size_t>(std::numeric_limits<int>::digits)) {
    spec.family.propagators.push_back(spec.family.propagators.front());
  }
  spec.family.top_level_sectors = {std::numeric_limits<int>::max()};
  return spec;
}

amflow::ProblemSpec MakeBranchLoopMissingTopLevelSectorSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.top_level_sectors.clear();
  return spec;
}

amflow::ProblemSpec MakeBranchLoopMultipleTopLevelSectorsSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.top_level_sectors = {127, 3};
  return spec;
}

amflow::ProblemSpec MakeBranchLoopInactiveTopLevelSectorSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.top_level_sectors = {1 << static_cast<int>(spec.family.propagators.size())};
  return spec;
}

amflow::ProblemSpec MakeBranchLoopAllCutActiveSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.top_level_sectors = {7};
  for (std::size_t index = 0; index < 3; ++index) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Cut;
  }
  return spec;
}

amflow::ProblemSpec MakeBranchLoopEmptyFirstSymanzikSupportSpec() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.top_level_sectors = {1};
  return spec;
}

amflow::AmfOptions MakePoisonedAmfOptions(
    const std::vector<std::string>& amf_modes,
    const std::vector<std::string>& ending_schemes = {"EdgeCase", "SingleMass"}) {
  amflow::AmfOptions options;
  options.amf_modes = amf_modes;
  options.ending_schemes = ending_schemes;
  options.d0 = "11/3";
  options.working_precision = 211;
  options.chop_precision = 19;
  options.x_order = 7;
  options.extra_x_order = 9;
  options.learn_x_order = 13;
  options.test_x_order = 17;
  options.rationalize_precision = 29;
  options.run_length = 31;
  options.use_cache = true;
  options.skip_reduction = true;
  return options;
}

amflow::DESystem MakeBoundaryAttachmentBaselineDESystem() {
  amflow::DESystem system;
  system.masters = {
      {"toy_family", {1, 0}, "I1"},
      {"toy_family", {0, 1}, "I2"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices["eta"] = {
      {"1", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=0"};
  return system;
}

amflow::BoundaryRequest MakeValidBoundaryRequest() {
  return {"eta", "eta=1", "manual"};
}

amflow::BoundaryCondition MakeValidBoundaryCondition() {
  return {"eta", "eta=1", {"1/3", "2/5"}, "manual"};
}

amflow::BoundaryCondition MakeEtaInfinityBoundaryCondition(
    const std::string& strategy = "builtin::eta->infinity") {
  return {"eta", "infinity", {"B1", "B2"}, strategy};
}

amflow::SolveRequest MakeBoundarySolveRequest() {
  amflow::SolveRequest request;
  request.system = MakeBoundaryAttachmentBaselineDESystem();
  request.boundary_requests = {MakeValidBoundaryRequest()};
  request.start_location = "eta=1";
  request.target_location = "eta=2";
  request.precision_policy = MakeDistinctPrecisionPolicy();
  request.requested_digits = 73;
  return request;
}

amflow::SolveRequest MakeEtaInfinitySolveRequest(const amflow::BoundaryRequest& request) {
  amflow::SolveRequest solve_request;
  solve_request.system = amflow::MakeSampleDESystem();
  solve_request.boundary_requests = {request};
  solve_request.start_location = "infinity";
  solve_request.target_location = "eta=0";
  solve_request.precision_policy = MakeDistinctPrecisionPolicy();
  solve_request.requested_digits = 61;
  return solve_request;
}

amflow::SolverDiagnostics MakeRequestDrivenEquivalenceHarnessDiagnostics(
    const amflow::SolveRequest& request) {
  amflow::SolverDiagnostics diagnostics;
  diagnostics.success = true;
  const std::size_t boundary_value_count =
      request.boundary_conditions.empty() ? 0 : request.boundary_conditions.front().values.size();
  const std::size_t summary_size = request.start_location.size() + request.target_location.size() +
                                   request.boundary_conditions.size() + boundary_value_count;
  diagnostics.residual_norm = static_cast<double>(
                                  request.system.masters.size() * 16 +
                                  request.boundary_requests.size() * 8 +
                                  request.boundary_conditions.size() * 4 +
                                  request.system.variables.size() * 2 +
                                  (request.requested_digits % 16)) /
                              1024.0;
  diagnostics.overlap_mismatch =
      static_cast<double>(summary_size + request.precision_policy.x_order_step) / 512.0;
  diagnostics.failure_code.clear();
  diagnostics.summary = "recorded deterministic eta-infinity boundary solve from " +
                        request.start_location + " to " + request.target_location + " with " +
                        std::to_string(request.boundary_conditions.size()) +
                        " attached conditions";
  return diagnostics;
}

amflow::SolveRequest MakeManualStartBoundarySolveRequest(
    const amflow::DESystem& system,
    const std::string& variable_name,
    const std::string& start_location,
    const std::string& target_location,
    const std::vector<std::string>& boundary_values) {
  amflow::SolveRequest request;
  request.system = system;
  request.boundary_requests = {{variable_name, start_location, "manual"}};
  request.boundary_conditions = {{variable_name, start_location, boundary_values, "manual"}};
  request.start_location = start_location;
  request.target_location = target_location;
  request.precision_policy = MakeDistinctPrecisionPolicy();
  request.requested_digits = 73;
  return request;
}

class RecordingStaticBoundaryProvider final : public amflow::BoundaryProvider {
 public:
  RecordingStaticBoundaryProvider(
      const std::string& strategy,
      const std::vector<amflow::BoundaryCondition>& provided_conditions)
      : strategy_(strategy), provided_conditions_(provided_conditions) {}

  std::string Strategy() const override {
    ++strategy_call_count_;
    return strategy_;
  }

  amflow::BoundaryCondition Provide(const amflow::DESystem&,
                                    const amflow::BoundaryRequest& request) const override {
    ++provide_call_count_;
    seen_requests_.push_back(request);
    if (next_condition_index_ >= provided_conditions_.size()) {
      throw std::runtime_error("unexpected boundary-provider call");
    }
    return provided_conditions_[next_condition_index_++];
  }

  int strategy_call_count() const {
    return strategy_call_count_;
  }

  int provide_call_count() const {
    return provide_call_count_;
  }

  const std::vector<amflow::BoundaryRequest>& seen_requests() const {
    return seen_requests_;
  }

 private:
  std::string strategy_;
  std::vector<amflow::BoundaryCondition> provided_conditions_;
  mutable int strategy_call_count_ = 0;
  mutable int provide_call_count_ = 0;
  mutable std::size_t next_condition_index_ = 0;
  mutable std::vector<amflow::BoundaryRequest> seen_requests_;
};

class ThrowingBoundaryProvider final : public amflow::BoundaryProvider {
 public:
  ThrowingBoundaryProvider(const std::string& strategy, const std::string& message)
      : strategy_(strategy), message_(message) {}

  std::string Strategy() const override {
    ++strategy_call_count_;
    return strategy_;
  }

  amflow::BoundaryCondition Provide(const amflow::DESystem&,
                                    const amflow::BoundaryRequest&) const override {
    ++provide_call_count_;
    throw amflow::BoundaryUnsolvedError(message_);
  }

  int strategy_call_count() const {
    return strategy_call_count_;
  }

  int provide_call_count() const {
    return provide_call_count_;
  }

 private:
  std::string strategy_;
  std::string message_;
  mutable int strategy_call_count_ = 0;
  mutable int provide_call_count_ = 0;
};

amflow::DESystem MakeGeneratedEtaCoefficientEvaluationSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"(-1)*(2)", "(-1)*(1) + (-1)*(s)"},
      {"(-1)*(t)", "(-1)*(3)"},
  };
  system.singular_points = {"eta=0", "eta=s"};
  return system;
}

amflow::DESystem MakeAutomaticInvariantCoefficientEvaluationSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.variables = {
      {"s", amflow::DifferentiationVariableKind::Invariant},
  };
  system.coefficient_matrices.clear();
  system.coefficient_matrices["s"] = {
      {"(-1)*(2) + ((-1)*(-1))*(5)", "(-1)*(3) + ((-1)*(-1))*(7)"},
      {"(1)*(19) + ((-2)*(-1))*(13)", "(1)*(11) + ((-2)*(-1))*(17)"},
  };
  system.singular_points = {"s=0"};
  return system;
}

amflow::DESystem MakeCancellationSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"((eta-s)*(eta-t))/(eta-s)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeDistinctFinitePoleSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-s) + 1/(eta-t)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeZeroMaskedSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"0*(1/((eta-s)*(eta-s))) + 1/(eta-t)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeDivisionShortCircuitSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"0/(1/(eta-s)+1/(eta-t))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeGroupedSameDenominatorCancellationSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"eta/(eta-s) - s/(eta-s)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeZeroDivisorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"0/0", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=0"};
  return system;
}

amflow::DESystem MakeDirectSimpleDifferenceSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-s)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeCancelledUnsupportedHigherOrderSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/((eta-s)*(eta-s)) - 1/((eta-s)*(eta-s))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeUnsupportedNegatedConstantDifferenceSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-(-s))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=0"};
  return system;
}

amflow::DESystem MakeUnsupportedGroupedConstantDifferenceSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-(s-s))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=0"};
  return system;
}

amflow::DESystem MakeClassificationMatrixAuthoritativeSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-s)", "1/((eta-t)*(eta-u))"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t", "eta=u"};
  return system;
}

amflow::DESystem MakeCancelledUnsupportedMultiFactorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/((eta-s)*(eta-t)) - 1/((eta-s)*(eta-t))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeUnsupportedNormalizedMultiTermDivisorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta/(eta-s) - s/(eta-s))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeUnsupportedRegularMultiTermDivisorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/((eta-s)+s)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeUnsupportedDirectLinearMultiTermDivisorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/(eta+s)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=0"};
  return system;
}

amflow::DESystem MakeZeroNormalizedMultiTermDivisorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/((eta/(eta-s) - s/(eta-s)) - 1)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeZeroNumeratorZeroNormalizedDivisorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"0/((eta/(eta-s) - s/(eta-s)) - 1)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakePolynomialNumeratorRegularSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"(eta-s)*(eta-t)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakePolynomialNumeratorSimplePoleSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"((eta-s)*(eta-t))/(eta-u)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t", "eta=u"};
  return system;
}

amflow::DESystem MakeGroupedNonlinearNumeratorSharedDenominatorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"((eta-s)*(eta-t))/(eta-u) + 1/(eta-u)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=u"};
  return system;
}

amflow::DESystem MakeDuplicateNonlinearNumeratorSharedDenominatorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"((eta-s)*(eta-t))/(eta-u) + ((eta-s)*(eta-t))/(eta-u)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=u"};
  return system;
}

amflow::DESystem MakeUnsupportedSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/((eta-s)*(eta-s))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeIdenticalMultiTermQuotientRegularSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"(1/(eta-s)+1/(eta-t))/(1/(eta-s)+1/(eta-t))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeIdenticalMultiTermQuotientZeroSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"(((eta/(eta-s))-(s/(eta-s)))-1)/(((eta/(eta-s))-(s/(eta-s)))-1)", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeIdenticalHigherOrderQuotientUnsupportedSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"(1/((eta-s)*(eta-s)))/(1/((eta-s)*(eta-s)))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s"};
  return system;
}

amflow::DESystem MakeIdenticalMultiFactorQuotientUnsupportedSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"(1/((eta-s)*(eta-t)))/(1/((eta-s)*(eta-t)))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeUnsupportedMultiFactorSingularAnalysisSystem() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices.clear();
  system.coefficient_matrices["eta"] = {
      {"1/((eta-s)*(eta-t))", "0"},
      {"0", "1"},
  };
  system.singular_points = {"eta=s", "eta=t"};
  return system;
}

amflow::DESystem MakeScalarRegularPointSeriesSystem(const std::string& coefficient_expression) {
  amflow::DESystem system;
  system.masters = {
      {"toy_scalar_family", {1}, "I"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices["eta"] = {
      {coefficient_expression},
  };
  return system;
}

amflow::DESystem MakeUnsupportedScalarSeriesMatrixShapeSystem() {
  amflow::DESystem system = MakeScalarRegularPointSeriesSystem("1");
  system.coefficient_matrices["eta"] = {
      {"1", "0"},
  };
  return system;
}

amflow::DESystem MakeMatrixRegularPointSeriesSystem(
    const std::vector<std::vector<std::string>>& coefficient_matrix,
    const std::size_t master_count = 0,
    const std::string& variable_name = "eta") {
  amflow::DESystem system;
  const std::size_t resolved_master_count =
      master_count == 0 ? coefficient_matrix.size() : master_count;
  system.masters.reserve(resolved_master_count);
  for (std::size_t index = 0; index < resolved_master_count; ++index) {
    system.masters.push_back({"toy_matrix_family",
                              {static_cast<int>(index + 1)},
                              "I" + std::to_string(index + 1)});
  }
  system.variables = {
      {variable_name, amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices[variable_name] = coefficient_matrix;
  return system;
}

amflow::ParsedMasterList MakeInvariantGenerationHappyMasterBasis() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_family";
  master_basis.masters = {
      {"toy_family", {1, 1, 0}},
      {"toy_family", {0, 2, 1}},
  };
  return master_basis;
}

amflow::InvariantDerivativeSeed MakeInvariantGenerationHappySeed() {
  amflow::InvariantDerivativeSeed seed;
  seed.family = "toy_family";
  seed.variable = {"s", amflow::DifferentiationVariableKind::Invariant};
  seed.propagator_derivatives = {
      {{{"a", {-1, 0, 0}}, {"b", {0, 0, 0}}}},
      {{{"c", {0, -1, 0}}}},
      {{{"d", {0, 0, -1}}, {"e", {2, -1, -2}}}},
  };
  return seed;
}

amflow::InvariantDerivativeSeed MakeInvariantPreparationHappySeed() {
  amflow::InvariantDerivativeSeed seed;
  seed.family = "planar_double_box";
  seed.variable = {"s", amflow::DifferentiationVariableKind::Invariant};
  seed.propagator_derivatives = {
      {{{"s", {-1, 0, 0, 0, 0, 0, 0}}, {"1", {0, 0, 0, 0, 0, 0, 0}}}},
      {},
      {},
      {},
      {},
      {},
      {{{"t", {0, 0, 0, 0, 0, 0, -1}}}},
  };
  return seed;
}

amflow::InvariantDerivativeSeed MakeInvariantExecutionHappySeed() {
  amflow::InvariantDerivativeSeed seed;
  seed.family = "planar_double_box";
  seed.variable = {"s", amflow::DifferentiationVariableKind::Invariant};
  seed.propagator_derivatives = {
      {{{"1", {0, 0, 0, 0, 0, 0, 0}}}},
      {},
      {},
      {},
      {},
      {},
      {{{"1", {0, 0, 0, 0, 0, 0, 0}}}},
  };
  return seed;
}

amflow::ProblemSpec MakeAutoInvariantHappyProblemSpec() {
  amflow::ProblemSpec spec;
  spec.family.name = "toy_auto_family";
  spec.family.loop_momenta = {"k"};
  spec.family.top_level_sectors = {7};
  spec.family.propagators = {
      {"(k)^2", "0", amflow::PropagatorKind::Standard, -1},
      {"(k-p1-p2)^2", "0", amflow::PropagatorKind::Standard, -1},
      {"(-s)*((k)^2)", "0", amflow::PropagatorKind::Standard, -1},
  };
  spec.kinematics.incoming_momenta = {"p1", "p2"};
  spec.kinematics.outgoing_momenta = {};
  spec.kinematics.momentum_conservation = "p1 + p2 = 0";
  spec.kinematics.invariants = {"s"};
  spec.kinematics.scalar_product_rules = {
      {"p1*p1", "0"},
      {"p2*p2", "0"},
      {"p1*p2", "s/2"},
  };
  spec.kinematics.numeric_substitutions = {
      {"s", "30"},
  };
  spec.targets = {
      {"toy_auto_family", {1, 1, 1}},
  };
  spec.dimension = "4 - 2*eps";
  return spec;
}

amflow::ProblemSpec MakeAutoInvariantAllZeroProblemSpec() {
  amflow::ProblemSpec spec;
  spec.family.name = "toy_auto_zero_family";
  spec.family.loop_momenta = {"k"};
  spec.family.top_level_sectors = {3};
  spec.family.propagators = {
      {"(k)^2", "0", amflow::PropagatorKind::Standard, -1},
      {"(k-p1)^2", "0", amflow::PropagatorKind::Standard, -1},
  };
  spec.kinematics.incoming_momenta = {"p1"};
  spec.kinematics.outgoing_momenta = {};
  spec.kinematics.momentum_conservation = "p1 = 0";
  spec.kinematics.invariants = {"s"};
  spec.kinematics.scalar_product_rules = {
      {"p1*p1", "0"},
  };
  spec.targets = {
      {"toy_auto_zero_family", {1, 1}},
  };
  spec.dimension = "4 - 2*eps";
  return spec;
}

amflow::ProblemSpec MakeAutoInvariantWholeFactorProblemSpec() {
  amflow::ProblemSpec spec;
  spec.family.name = "toy_auto_factor_family";
  spec.family.loop_momenta = {"k"};
  spec.family.top_level_sectors = {7};
  spec.family.propagators = {
      {"(k)^2", "0", amflow::PropagatorKind::Standard, -1},
      {"t*((k)^2)", "0", amflow::PropagatorKind::Standard, -1},
      {"(-s)*(t*((k)^2))", "0", amflow::PropagatorKind::Standard, -1},
  };
  spec.kinematics.incoming_momenta = {};
  spec.kinematics.outgoing_momenta = {};
  spec.kinematics.momentum_conservation = "0 = 0";
  spec.kinematics.invariants = {"s", "t"};
  spec.targets = {
      {"toy_auto_factor_family", {1, 1, 1}},
  };
  spec.dimension = "4 - 2*eps";
  return spec;
}

amflow::ParsedMasterList MakeAutoInvariantHappyMasterBasis() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_auto_family";
  master_basis.masters = {
      {"toy_auto_family", {1, 1, 1}},
      {"toy_auto_family", {0, -1, 2}},
  };
  return master_basis;
}

amflow::ParsedMasterList MakeAutoInvariantWholeFactorMasterBasis() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_auto_factor_family";
  master_basis.masters = {
      {"toy_auto_factor_family", {1, 1, 1}},
  };
  return master_basis;
}

std::string MakeFixtureCopyScript(const std::filesystem::path& fixture_root,
                                  const bool write_stdout = true,
                                  const int exit_code = 0) {
  std::ostringstream script;
  script << "#!/bin/sh\n";
  script << "set -eu\n";
  script << "dest=\"$PWD/results/planar_double_box\"\n";
  script << "mkdir -p \"$dest\"\n";
  script << "cp "
         << std::filesystem::absolute(fixture_root / "results/planar_double_box/masters").string()
         << " \"$dest/masters\"\n";
  const std::filesystem::path rule_path =
      fixture_root / "results/planar_double_box/kira_target.m";
  if (std::filesystem::exists(rule_path)) {
    script << "cp " << std::filesystem::absolute(rule_path).string()
           << " \"$dest/kira_target.m\"\n";
  } else {
    script << "rm -f \"$dest/kira_target.m\"\n";
  }
  if (write_stdout) {
    script << "echo \"fixture:$1\"\n";
  }
  script << "exit " << exit_code << "\n";
  return script.str();
}

std::filesystem::path WritePropagatorHappyFixture(const std::string& label) {
  const std::filesystem::path root = FreshTempDir(label);
  const std::filesystem::path results_root = root / "results" / "planar_double_box";
  std::filesystem::create_directories(results_root);

  {
    std::ofstream stream(results_root / "masters");
    stream << "planar_double_box[1,0,1,0,0,0,1] 0\n";
    stream << "planar_double_box[1,0,0,0,0,0,0] 0\n";
  }

  {
    std::ofstream stream(results_root / "kira_target.m");
    stream << "{\n";
    stream << "  planar_double_box[2,0,1,0,0,0,1] -> "
              "2*planar_double_box[1,0,1,0,0,0,1] + "
              "planar_double_box[1,0,0,0,0,0,0],\n";
    stream << "  planar_double_box[1,0,2,0,0,0,1] -> "
              "planar_double_box[1,0,1,0,0,0,1] + "
              "t*planar_double_box[1,0,0,0,0,0,0],\n";
    stream << "  planar_double_box[1,0,1,0,0,0,2] -> "
              "s*planar_double_box[1,0,1,0,0,0,1] + "
              "2*planar_double_box[1,0,0,0,0,0,0],\n";
    stream << "  planar_double_box[2,0,0,0,0,0,0] -> "
              "t*planar_double_box[1,0,1,0,0,0,1] + "
              "3*planar_double_box[1,0,0,0,0,0,0]\n";
    stream << "}\n";
  }

  return root;
}

std::string MakeK0SmokeFixtureCopyScript(const bool write_stdout = true,
                                         const bool add_direct_results_root = false,
                                         const int exit_code = 0) {
  std::ostringstream script;
  script << "#!/bin/sh\n";
  script << "set -eu\n";
  script << "generated_dest=\"$PWD/results/automatic_vs_manual_k0_smoke\"\n";
  script << "mkdir -p \"$generated_dest\"\n";
  script << "cat > \"$generated_dest/masters\" <<'EOF'\n";
  script << "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0] 0\n";
  script << "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-2,-1] 0\n";
  script << "EOF\n";
  script << "cat > \"$generated_dest/kira_target.m\" <<'EOF'\n";
  script << "{\n";
  script << "  automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0] -> "
            "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0]\n";
  script << "}\n";
  script << "EOF\n";
  if (add_direct_results_root) {
    script << "direct_dest=\"$PWD/../results/automatic_vs_manual_k0_smoke\"\n";
    script << "mkdir -p \"$direct_dest\"\n";
    script << "cat > \"$direct_dest/masters\" <<'EOF'\n";
    script << "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0] 0\n";
    script << "EOF\n";
  }
  if (write_stdout) {
    script << "echo \"k0:$1\"\n";
  }
  script << "exit " << exit_code << "\n";
  return script.str();
}

std::string MakeAutoInvariantHappyRuleFile() {
  return "{\n"
         "  toy_auto_family[1,2,1] -> 2*toy_auto_family[1,1,1] + "
         "3*toy_auto_family[0,-1,2],\n"
         "  toy_auto_family[0,1,2] -> 5*toy_auto_family[1,1,1] + "
         "7*toy_auto_family[0,-1,2],\n"
         "  toy_auto_family[0,0,2] -> 19*toy_auto_family[1,1,1] + "
         "11*toy_auto_family[0,-1,2],\n"
         "  toy_auto_family[-1,-1,3] -> 13*toy_auto_family[1,1,1] + "
         "17*toy_auto_family[0,-1,2]\n"
         "}\n";
}

std::string MakeAutoInvariantMissingGeneratedTargetRuleFile() {
  return "{\n"
         "  toy_auto_family[1,2,1] -> 2*toy_auto_family[1,1,1] + "
         "3*toy_auto_family[0,-1,2],\n"
         "  toy_auto_family[0,1,2] -> 5*toy_auto_family[1,1,1] + "
         "7*toy_auto_family[0,-1,2],\n"
         "  toy_auto_family[0,0,2] -> 19*toy_auto_family[1,1,1] + "
         "11*toy_auto_family[0,-1,2]\n"
         "}\n";
}

std::string MakeAutoInvariantWholeFactorSRuleFile() {
  return "{\n"
         "  toy_auto_factor_family[1,0,2] -> 2*toy_auto_factor_family[1,1,1]\n"
         "}\n";
}

std::string MakeAutoInvariantWholeFactorTRuleFile() {
  return "{\n"
         "  toy_auto_factor_family[0,2,1] -> 3*toy_auto_factor_family[1,1,1],\n"
         "  toy_auto_factor_family[0,1,2] -> 5*toy_auto_factor_family[1,1,1]\n"
         "}\n";
}

std::string MakeAutoInvariantResultScript(const bool write_rule_file,
                                          const std::string& rule_file_contents,
                                          const bool write_stdout = true,
                                          const int exit_code = 0) {
  std::ostringstream script;
  script << "#!/bin/sh\n";
  script << "set -eu\n";
  script << "dest=\"$PWD/results/toy_auto_family\"\n";
  script << "mkdir -p \"$dest\"\n";
  script << "cat > \"$dest/masters\" <<'EOF'\n";
  script << "toy_auto_family[1,1,1] 0\n";
  script << "toy_auto_family[0,-1,2] 0\n";
  script << "EOF\n";
  if (write_rule_file) {
    script << "cat > \"$dest/kira_target.m\" <<'EOF'\n";
    script << rule_file_contents;
    script << "EOF\n";
  } else {
    script << "rm -f \"$dest/kira_target.m\"\n";
  }
  if (write_stdout) {
    script << "echo \"auto-invariant:$1\"\n";
  }
  script << "exit " << exit_code << "\n";
  return script.str();
}

std::string MakeAutoInvariantWholeFactorResultScript(const std::string& rule_file_contents,
                                                     const bool write_stdout = true,
                                                     const int exit_code = 0) {
  std::ostringstream script;
  script << "#!/bin/sh\n";
  script << "set -eu\n";
  script << "dest=\"$PWD/results/toy_auto_factor_family\"\n";
  script << "mkdir -p \"$dest\"\n";
  script << "cat > \"$dest/masters\" <<'EOF'\n";
  script << "toy_auto_factor_family[1,1,1] 0\n";
  script << "EOF\n";
  script << "cat > \"$dest/kira_target.m\" <<'EOF'\n";
  script << rule_file_contents;
  script << "EOF\n";
  if (write_stdout) {
    script << "echo \"whole-factor:$1\"\n";
  }
  script << "exit " << exit_code << "\n";
  return script.str();
}

std::string MakeAutoInvariantWholeFactorListResultScript(const bool fail_second_t = false) {
  std::ostringstream script;
  script << "#!/bin/sh\n";
  script << "set -eu\n";
  script << "dest=\"$PWD/results/toy_auto_factor_family\"\n";
  script << "mkdir -p \"$dest\"\n";
  script << "cat > \"$dest/masters\" <<'EOF'\n";
  script << "toy_auto_factor_family[1,1,1] 0\n";
  script << "EOF\n";
  script << "layout_name=\"$(basename \"$(dirname \"$PWD\")\")\"\n";
  if (fail_second_t) {
    script << "if [ \"$layout_name\" = \"invariant-0002-t\" ]; then\n";
    script << "  echo \"expected-auto-invariant-list-failure\" 1>&2\n";
    script << "  exit 9\n";
    script << "fi\n";
  }
  script << "case \"$layout_name\" in\n";
  script << "  *-s)\n";
  script << "    cat > \"$dest/kira_target.m\" <<'EOF'\n";
  script << MakeAutoInvariantWholeFactorSRuleFile();
  script << "EOF\n";
  script << "    ;;\n";
  script << "  *-t)\n";
  script << "    cat > \"$dest/kira_target.m\" <<'EOF'\n";
  script << MakeAutoInvariantWholeFactorTRuleFile();
  script << "EOF\n";
  script << "    ;;\n";
  script << "  *)\n";
  script << "    echo \"unexpected invariant layout: $layout_name\" 1>&2\n";
  script << "    exit 7\n";
  script << "    ;;\n";
  script << "esac\n";
  script << "echo \"whole-factor-list:$1\"\n";
  script << "exit 0\n";
  return script.str();
}

const amflow::ParsedReductionRule& FindRuleByTarget(
    const std::vector<amflow::ParsedReductionRule>& rules,
    const std::string& target_label) {
  for (const auto& rule : rules) {
    if (rule.target.Label() == target_label) {
      return rule;
    }
  }
  throw std::runtime_error("failed to find parsed reduction rule for target: " + target_label);
}

const amflow::ParsedReductionTerm& FindTermByMaster(const amflow::ParsedReductionRule& rule,
                                                    const std::string& master_label) {
  for (const auto& term : rule.terms) {
    if (term.master.Label() == master_label) {
      return term;
    }
  }
  throw std::runtime_error("failed to find parsed reduction term for master: " + master_label);
}

void WriteExecutableScript(const std::filesystem::path& path, const std::string& body) {
  {
    std::ofstream stream(path);
    stream << body;
  }
  std::filesystem::permissions(
      path,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec | std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::replace);
}

void SampleProblemValidationTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const auto messages = amflow::ValidateProblemSpec(spec);
  Expect(messages.empty(), "sample problem should validate cleanly");

  const std::string yaml = amflow::SerializeProblemSpecYaml(spec);
  Expect(yaml.find("planar_double_box") != std::string::npos,
         "sample problem YAML should contain the family name");
  Expect(yaml.find("targets:") != std::string::npos,
         "sample problem YAML should contain targets");
}

void ProblemSpecRoundTripTest() {
  const amflow::ProblemSpec original = amflow::MakeSampleProblemSpec();
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "amflow-bootstrap-problem-spec.yaml";
  {
    std::ofstream stream(path);
    stream << amflow::SerializeProblemSpecYaml(original);
  }

  const amflow::ProblemSpec loaded = amflow::LoadProblemSpecFile(path);
  Expect(amflow::SerializeProblemSpecYaml(loaded) == amflow::SerializeProblemSpecYaml(original),
         "problem spec should round-trip through file-backed YAML");
}

void ProblemSpecExampleFileTest() {
  const std::filesystem::path path =
      std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.example.yaml";
  const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(path);
  const auto messages = amflow::ValidateProblemSpec(spec);

  Expect(messages.empty(), "example problem spec should validate cleanly");
  Expect(spec.family.name == "planar_double_box",
         "example problem spec should expose the bootstrap family");
  Expect(spec.family.propagators.size() == 7,
         "example problem spec should include the full propagator list");
  Expect(spec.family.top_level_sectors == std::vector<int>{127},
         "example problem spec should preserve the reviewed top sector");
  Expect(spec.family.preferred_masters == std::vector<std::string>{
             "planar_double_box[1,1,1,1,1,1,1]"},
         "example problem spec should preserve the reviewed preferred-master list");
  Expect(amflow::SerializeProblemSpecYaml(spec) ==
             amflow::SerializeProblemSpecYaml(amflow::MakeSampleProblemSpec()),
         "example problem spec should stay locked to MakeSampleProblemSpec");
  Expect(spec.kinematics.scalar_product_rules.size() == 3,
         "example problem spec should preserve the reviewed scalar-product replacement system");
  Expect(spec.kinematics.scalar_product_rules[2].left == "p1*p2" &&
             spec.kinematics.scalar_product_rules[2].right == "s/2",
         "example problem spec should preserve the reviewed p1.p2 replacement");
  Expect(spec.family.propagators[6].expression == "(k2-p1-p2)^2" &&
             spec.family.propagators[6].mass == "0",
         "example problem spec should preserve the reviewed seventh propagator");
  Expect(spec.targets.size() == 1,
         "example problem spec should include the reviewed bootstrap target");
  Expect(spec.targets[0].Label() == "planar_double_box[1,1,1,1,1,1,1]",
         "example problem spec should preserve the reviewed target label");
}

void K0SmokeProblemSpecFileTest() {
  const std::filesystem::path path =
      std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml";
  const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(path);
  const auto messages = amflow::ValidateProblemSpec(spec);

  Expect(messages.empty(), "K0 smoke spec should validate cleanly");
  Expect(spec.family.name == "automatic_vs_manual_k0_smoke",
         "K0 smoke spec should use its dedicated family name");
  Expect(spec.family.propagators.size() == 9,
         "K0 smoke spec should preserve the repo-local frozen 9-propagator family");
  Expect(spec.family.top_level_sectors == std::vector<int>{127},
         "K0 smoke spec should preserve the repo-local frozen seven-line top sector");
  Expect(spec.family.preferred_masters.empty(),
         "K0 smoke spec should not claim a preferred-master list");
  Expect(spec.family.propagators[4].expression == "(k2+p3)^2" &&
             spec.family.propagators[4].mass == "msq",
         "K0 smoke spec should preserve the repo-local frozen massive fifth propagator");
  Expect(spec.kinematics.scalar_product_rules.size() == 6,
         "K0 smoke spec should preserve the repo-local frozen scalar-product system");
  Expect(spec.targets.size() == 4,
         "K0 smoke spec should preserve the repo-local frozen target list");
  Expect(std::all_of(spec.targets.begin(), spec.targets.end(), [](const amflow::TargetIntegral& target) {
           return target.indices.size() == 9 &&
                  std::all_of(target.indices.begin(), target.indices.begin() + 7,
                              [](const int index) { return index > 0; }) &&
                  target.indices[7] <= 0 && target.indices[8] <= 0;
         }),
         "K0 smoke spec should keep the frozen target support on lines 1-7 only");
  Expect(spec.targets[0].Label() == "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0]" &&
             spec.targets[1].Label() == "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-2,-1]" &&
             spec.targets[2].Label() == "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-1,-2]" &&
             spec.targets[3].Label() == "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,0,-3]",
         "K0 smoke spec should preserve the repo-local frozen target order");
}

void LoadedSpecValidationRejectsMalformedTargetsTest() {
  const amflow::ProblemSpec sample = amflow::MakeSampleProblemSpec();
  const std::string canonical_yaml = amflow::SerializeProblemSpecYaml(sample);

  std::string missing_indices_yaml = canonical_yaml;
  ReplaceFirst(missing_indices_yaml, "    indices: [1, 1, 1, 1, 1, 1, 1]\n", "    indices: []\n");
  const auto missing_indices_messages =
      amflow::ValidateLoadedProblemSpec(amflow::ParseProblemSpecYaml(missing_indices_yaml));
  Expect(ContainsSubstring(missing_indices_messages, "targets[0].indices must not be empty"),
         "loaded-spec validation should reject empty target indices");

  std::string short_indices_yaml = canonical_yaml;
  ReplaceFirst(short_indices_yaml, "    indices: [1, 1, 1, 1, 1, 1, 1]\n", "    indices: [1, 1]\n");
  const auto short_indices_messages =
      amflow::ValidateLoadedProblemSpec(amflow::ParseProblemSpecYaml(short_indices_yaml));
  Expect(
      ContainsSubstring(short_indices_messages,
                        "targets[0].indices size must match family.propagators size"),
      "loaded-spec validation should reject target arity mismatches");
}

void UnknownFieldsAreIgnoredTest() {
  const amflow::ProblemSpec sample = amflow::MakeSampleProblemSpec();
  const std::string canonical_yaml = amflow::SerializeProblemSpecYaml(sample);
  std::string extended_yaml = canonical_yaml;

  InsertBefore(extended_yaml, "  propagators:\n",
               "  future_family:\n"
               "    tag: \"ignored\"\n");
  InsertBefore(extended_yaml, "  outgoing_momenta:",
               "  future_kinematics:\n"
               "    owner: \"ignored\"\n");
  InsertBefore(extended_yaml, "    indices: [",
               "    future_target_field:\n"
               "      note: \"ignored\"\n");
  InsertBefore(extended_yaml, "dimension:",
               "future_top_level:\n"
               "  enabled: true\n");

  const amflow::ProblemSpec loaded = amflow::ParseProblemSpecYaml(extended_yaml);
  const auto messages = amflow::ValidateLoadedProblemSpec(loaded);
  Expect(messages.empty(), "unknown additive fields should be ignored by the bootstrap loader");
  Expect(amflow::SerializeProblemSpecYaml(loaded) == canonical_yaml,
         "ignored additive fields should not change the canonicalized spec");
}

void DuplicateKeysAreRejectedTest() {
  const std::string canonical_yaml =
      amflow::SerializeProblemSpecYaml(amflow::MakeSampleProblemSpec());

  std::string duplicate_family_yaml = canonical_yaml;
  InsertBefore(duplicate_family_yaml, "  loop_momenta: [", "  name: \"shadowed\"\n");
  ExpectParseFailure(duplicate_family_yaml, "duplicate family field: name",
                     "duplicate family keys should be rejected");

  std::string duplicate_map_yaml = canonical_yaml;
  InsertBefore(duplicate_map_yaml, "    t: \"-10/3\"\n", "    s: \"31\"\n");
  ExpectParseFailure(duplicate_map_yaml, "duplicate mapping entry: s",
                     "duplicate numeric_substitutions entries should be rejected");
}

void EtaInsertionHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {0, 2};
  decision.selected_propagators = {
      spec.family.propagators[0].expression,
      spec.family.propagators[2].expression,
  };

  const amflow::AuxiliaryFamilyTransformResult result =
      amflow::ApplyEtaInsertion(spec, decision);

  Expect(result.eta_symbol == "eta",
         "eta insertion should preserve the default eta symbol in the result");
  Expect(result.rewritten_propagator_indices == decision.selected_propagator_indices,
         "eta insertion should report rewritten propagator indices deterministically");
  Expect(result.transformed_spec.family.name == spec.family.name,
         "eta insertion should preserve the family name");
  Expect(result.transformed_spec.family.top_level_sectors == spec.family.top_level_sectors,
         "eta insertion should preserve top sectors");
  Expect(result.transformed_spec.targets.size() == spec.targets.size(),
         "eta insertion should preserve the target count");
  Expect(result.transformed_spec.targets[0].Label() == spec.targets[0].Label(),
         "eta insertion should preserve target labels");
  Expect(result.transformed_spec.kinematics.scalar_product_rules.size() ==
             spec.kinematics.scalar_product_rules.size(),
         "eta insertion should preserve scalar-product-rule count");
  Expect(result.transformed_spec.kinematics.scalar_product_rules[0].left ==
             spec.kinematics.scalar_product_rules[0].left &&
             result.transformed_spec.kinematics.scalar_product_rules[0].right ==
                 spec.kinematics.scalar_product_rules[0].right,
         "eta insertion should preserve scalar-product rules");
  Expect(result.transformed_spec.kinematics.numeric_substitutions ==
             spec.kinematics.numeric_substitutions,
         "eta insertion should preserve numeric substitutions");
  Expect(result.transformed_spec.family.propagators[0].expression == "((k1)^2) + eta",
         "eta insertion should rewrite the first selected propagator deterministically");
  Expect(result.transformed_spec.family.propagators[2].expression ==
             "((k1-p1-p2)^2) + eta",
         "eta insertion should rewrite the second selected propagator deterministically");
  Expect(result.transformed_spec.family.propagators[1].expression ==
             spec.family.propagators[1].expression,
         "eta insertion should leave unselected propagators untouched");
  Expect(result.transformed_spec.family.propagators[0].kind ==
             spec.family.propagators[0].kind,
         "eta insertion should preserve propagator kind");
  Expect(result.transformed_spec.family.propagators[0].prescription ==
             spec.family.propagators[0].prescription,
         "eta insertion should preserve propagator prescription");
  Expect(result.transformed_spec.kinematics.invariants.size() ==
             spec.kinematics.invariants.size() + 1,
         "eta insertion should append eta to the invariant list once");
  Expect(result.transformed_spec.kinematics.invariants.back() == "eta",
         "eta insertion should append eta at the end of the invariant list");
}

void EtaInsertionLeavesOriginalSpecUnchangedTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string before_yaml = amflow::SerializeProblemSpecYaml(spec);

  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {1};
  decision.selected_propagators = {spec.family.propagators[1].expression};
  static_cast<void>(amflow::ApplyEtaInsertion(spec, decision));

  Expect(amflow::SerializeProblemSpecYaml(spec) == before_yaml,
         "eta insertion should not mutate the input problem spec");
  Expect(spec.family.propagators[1].expression == "(k1-p1)^2",
         "eta insertion should leave the original propagator text unchanged");
  Expect(CountString(spec.kinematics.invariants, "eta") == 0,
         "eta insertion should not append eta to the original invariant list");
}

void EtaInsertionAppendsEtaOnceOnlyTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.kinematics.invariants.push_back("eta");

  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {3};
  decision.selected_propagators = {spec.family.propagators[3].expression};

  const amflow::AuxiliaryFamilyTransformResult result =
      amflow::ApplyEtaInsertion(spec, decision);

  Expect(CountString(result.transformed_spec.kinematics.invariants, "eta") == 1,
         "eta insertion should append eta exactly once even if it is already present");
  Expect(result.transformed_spec.kinematics.invariants.size() ==
             spec.kinematics.invariants.size(),
         "eta insertion should not duplicate an existing eta invariant");
}

void EtaInsertionKiraEmissionTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {0, 5};
  decision.selected_propagators = {
      spec.family.propagators[0].expression,
      spec.family.propagators[5].expression,
  };

  const amflow::ProblemSpec transformed =
      amflow::ApplyEtaInsertion(spec, decision).transformed_spec;

  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  const auto layout = amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-kira"));
  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(transformed, options, layout);

  Expect(preparation.validation_messages.empty(),
         "Kira preparation should accept the transformed auxiliary family");
  const auto family_yaml_it = preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != preparation.generated_files.end(),
         "Kira preparation should emit integralfamilies.yaml for transformed specs");
  Expect(family_yaml_it->second.find("\"((k1)^2) + eta\"") != std::string::npos,
         "Kira emission should contain the eta-rewritten first propagator");
  Expect(family_yaml_it->second.find("\"((k1-k2)^2) + eta\"") != std::string::npos,
         "Kira emission should contain the eta-rewritten second propagator");
  Expect(family_yaml_it->second.find("\"(k1-p1)^2\"") != std::string::npos,
         "Kira emission should leave unselected propagators unchanged");

  const auto kinematics_yaml_it = preparation.generated_files.find("config/kinematics.yaml");
  Expect(kinematics_yaml_it != preparation.generated_files.end(),
         "Kira preparation should emit kinematics.yaml for transformed specs");
  Expect(kinematics_yaml_it->second.find("\"eta\"") != std::string::npos,
         "Kira emission should include eta in the invariant list");
}

void EtaInsertionTrimsSelectedMassLiteralsForCoherentMassSurfaceTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassTrimEqualitySpec();
  const amflow::EtaInsertionDecision decision = amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);

  const amflow::AuxiliaryFamilyTransformResult result =
      amflow::ApplyEtaInsertion(spec, decision);

  Expect(result.rewritten_propagator_indices == std::vector<std::size_t>{0, 6},
         "eta insertion should still rewrite the Mass-selected equal-mass propagators");
  Expect(result.transformed_spec.family.propagators[0].mass == "msq" &&
             result.transformed_spec.family.propagators[6].mass == "msq",
         "eta insertion should trim outer whitespace on selected mass literals so the "
         "rewritten equal-mass surface stays coherent");
  Expect(result.transformed_spec.family.propagators[2].mass == spec.family.propagators[2].mass,
         "eta insertion should not broaden mass canonicalization to unselected propagators");
  Expect(spec.family.propagators[0].mass == " msq " &&
             spec.family.propagators[6].mass == "msq",
         "eta insertion should keep the input problem spec unchanged while trimming only the "
         "rewritten mass literals in the transformed copy");
}

void EtaInsertionKiraEmissionTrimsSelectedMassLiteralsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassTrimEqualitySpec();
  const amflow::EtaInsertionDecision decision = amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);
  const amflow::ProblemSpec transformed =
      amflow::ApplyEtaInsertion(spec, decision).transformed_spec;

  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-kira-mass-trim"));
  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(transformed, options, layout);

  Expect(preparation.validation_messages.empty(),
         "Kira preparation should accept the transformed trim-equality Mass surface");
  const auto family_yaml_it = preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != preparation.generated_files.end(),
         "Kira preparation should emit family YAML for the trimmed Mass path");
  Expect(family_yaml_it->second.find("[\"((k1)^2) + eta\", \"msq\"]") != std::string::npos &&
             family_yaml_it->second.find("[\"((k2-p1-p2)^2) + eta\", \"msq\"]") !=
                 std::string::npos,
         "Kira family YAML should carry canonical trimmed equal-mass literals for the rewritten "
         "Mass-selected propagators");
  Expect(family_yaml_it->second.find("\" msq \"") == std::string::npos,
         "Kira family YAML should not preserve outer-whitespace variants on rewritten "
         "Mass-selected literals");
}

void EtaInsertionRejectsDuplicateIndicesTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {1, 1};

  ExpectRuntimeError(
      [&spec, &decision]() {
        static_cast<void>(amflow::ApplyEtaInsertion(spec, decision));
      },
      "duplicate eta insertion propagator index",
      "eta insertion should reject duplicate propagator indices");
}

void EtaInsertionRejectsOutOfRangeIndicesTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {spec.family.propagators.size()};

  ExpectRuntimeError(
      [&spec, &decision]() {
        static_cast<void>(amflow::ApplyEtaInsertion(spec, decision));
      },
      "out of range",
      "eta insertion should reject out-of-range propagator indices");
}

void EtaInsertionRejectsEmptySelectionTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";

  ExpectRuntimeError(
      [&spec, &decision]() {
        static_cast<void>(amflow::ApplyEtaInsertion(spec, decision));
      },
      "requires at least one selected propagator index",
      "eta insertion should reject an empty propagator selection");
}

void EtaInsertionRejectsAuxiliaryPropagatorsTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[4].kind = amflow::PropagatorKind::Auxiliary;

  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {4};

  ExpectRuntimeError(
      [&spec, &decision]() {
        static_cast<void>(amflow::ApplyEtaInsertion(spec, decision));
      },
      "cannot target auxiliary propagator index",
      "eta insertion should reject selected auxiliary propagators");
}

void EtaInsertionAllowsNonzeroMassPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();

  amflow::EtaInsertionDecision decision;
  decision.mode_name = "Explicit";
  decision.selected_propagator_indices = {0, 6};
  decision.selected_propagators = {
      spec.family.propagators[0].expression,
      spec.family.propagators[6].expression,
  };

  const amflow::AuxiliaryFamilyTransformResult result =
      amflow::ApplyEtaInsertion(spec, decision);

  Expect(result.rewritten_propagator_indices == decision.selected_propagator_indices,
         "eta insertion should continue to rewrite the exact selected nonzero-mass propagators");
  Expect(result.transformed_spec.family.propagators[0].expression == "((k1)^2) + eta" &&
             result.transformed_spec.family.propagators[6].expression ==
                 "((k2-p1-p2)^2) + eta",
         "eta insertion should preserve the reviewed string-level rewrite for selected "
         "nonzero-mass propagators");
  Expect(result.transformed_spec.family.propagators[0].mass == "msq" &&
             result.transformed_spec.family.propagators[6].mass == "msq",
         "eta insertion should preserve coherent canonical mass literals on the selected "
         "nonzero-mass path when the input masses are already trimmed");
}

void AllEtaModeSelectsAllNonAuxiliaryPropagatorsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("All");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.mode_name == "All",
         "All eta mode should preserve its mode name in the planning decision");
  Expect(decision.selected_propagator_indices.size() == spec.family.propagators.size(),
         "All eta mode should select every non-auxiliary propagator in the sample family");
  for (std::size_t index = 0; index < spec.family.propagators.size(); ++index) {
    Expect(decision.selected_propagator_indices[index] == index,
           "All eta mode should preserve propagator order when selecting by index");
    Expect(decision.selected_propagators[index] == spec.family.propagators[index].expression,
           "All eta mode should continue to expose the matching propagator expressions");
  }
}

void AllEtaModeSkipsAuxiliaryPropagatorsTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators[1].kind = amflow::PropagatorKind::Auxiliary;
  spec.family.propagators[4].kind = amflow::PropagatorKind::Auxiliary;

  const auto mode = amflow::MakeBuiltinEtaMode("All");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.selected_propagator_indices.size() == spec.family.propagators.size() - 2,
         "All eta mode should skip propagators marked auxiliary");
  Expect(std::find(decision.selected_propagator_indices.begin(),
                   decision.selected_propagator_indices.end(),
                   1) == decision.selected_propagator_indices.end(),
         "All eta mode should not select the first auxiliary propagator");
  Expect(std::find(decision.selected_propagator_indices.begin(),
                   decision.selected_propagator_indices.end(),
                   4) == decision.selected_propagator_indices.end(),
         "All eta mode should not select the second auxiliary propagator");
}

void PrescriptionEtaModeSelectsAllNonAuxiliaryPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const auto mode = amflow::MakeBuiltinEtaMode("Prescription");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.mode_name == "Prescription",
         "Prescription eta mode should preserve its mode name in the planning decision");
  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 6},
         "Prescription eta mode should select the supported non-auxiliary propagators in "
         "declaration order");
  Expect(decision.selected_propagators ==
             std::vector<std::string>{spec.family.propagators[0].expression,
                                      spec.family.propagators[6].expression},
         "Prescription eta mode should continue to expose the matching propagator expressions");
  Expect(decision.explanation ==
             "Bootstrap alias selected 2 non-auxiliary propagators in declaration order for "
             "mode Prescription",
         "Prescription eta mode should use an honest bootstrap alias explanation");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "Prescription eta mode should not mutate the input problem spec");
}

void PrescriptionEtaModeRejectsAllAuxiliaryPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinAllAuxiliarySpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Prescription");

  ExpectRuntimeError(
      [&mode, &spec]() {
        static_cast<void>(mode->Plan(spec));
      },
      "eta mode Prescription found no non-auxiliary propagators in bootstrap",
      "Prescription eta mode should reject an all-auxiliary bootstrap fixture deterministically");
}

void PropagatorEtaModeSelectsAllNonAuxiliaryPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinPropagatorMixedSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Propagator");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.mode_name == "Propagator",
         "Propagator eta mode should preserve its mode name in the planning decision");
  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 2, 3, 5, 6},
         "Propagator eta mode should select every non-auxiliary propagator in declaration order");
  Expect(decision.selected_propagators ==
             std::vector<std::string>{spec.family.propagators[0].expression,
                                      spec.family.propagators[2].expression,
                                      spec.family.propagators[3].expression,
                                      spec.family.propagators[5].expression,
                                      spec.family.propagators[6].expression},
         "Propagator eta mode should continue to expose the matching propagator expressions");
  Expect(decision.explanation ==
             "Bootstrap structural selector selected 5 non-auxiliary propagators in "
             "declaration order for mode Propagator",
         "Propagator eta mode should use an honest bootstrap structural-selector explanation");
}

void PropagatorEtaModeRejectsAllAuxiliaryPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinAllAuxiliarySpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Propagator");

  ExpectRuntimeError(
      [&mode, &spec]() {
        static_cast<void>(mode->Plan(spec));
      },
      "eta mode Propagator found no non-auxiliary propagators in bootstrap",
      "Propagator eta mode should reject an all-auxiliary bootstrap fixture deterministically");
}

void PropagatorEtaModeDoesNotMutateInputProblemSpecTest() {
  const amflow::ProblemSpec spec = MakeBuiltinPropagatorMixedSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const auto mode = amflow::MakeBuiltinEtaMode("Propagator");

  static_cast<void>(mode->Plan(spec));

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "Propagator eta mode should not mutate the input problem spec");
}

void MassEtaModeSelectsEqualNonzeroMassGroupTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.mode_name == "Mass",
         "Mass eta mode should preserve its mode name in the planning decision");
  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 6},
         "Mass eta mode should select one exact trimmed equal-nonzero-mass group in "
         "declaration order");
  Expect(decision.selected_propagators ==
             std::vector<std::string>{spec.family.propagators[0].expression,
                                      spec.family.propagators[6].expression},
         "Mass eta mode should expose the matching propagator expressions for the chosen "
         "equal-mass group");
  Expect(decision.explanation ==
             "Bootstrap Mass selector chose the first scalar-product-rule-independent equal "
             "non-zero mass group \"msq\" with 2 non-auxiliary propagators on the current "
             "local declaration-order candidate surface",
         "Mass eta mode should use an honest bootstrap explanation for the reviewed local "
         "candidate surface");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "Mass eta mode should not mutate the input problem spec");
}

void MassEtaModeTreatsTrimmedExactMassLabelsAsOneGroupTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassTrimEqualitySpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 6},
         "Mass eta mode should group exact equal nonzero mass labels by trimmed text on the "
         "planner surface");
  Expect(decision.selected_propagators ==
             std::vector<std::string>{spec.family.propagators[0].expression,
                                      spec.family.propagators[6].expression},
         "Mass eta mode should preserve declaration order within the trimmed equal-mass group");
}

void MassEtaModePrefersScalarProductRuleIndependentGroupTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassPreferenceSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{2, 5},
         "Mass eta mode should prefer a later scalar-product-rule-independent equal-mass group "
         "over an earlier dependent one");
  Expect(decision.selected_propagators ==
             std::vector<std::string>{spec.family.propagators[2].expression,
                                      spec.family.propagators[5].expression},
         "Mass eta mode should preserve declaration order within the chosen independent group");
  Expect(decision.explanation ==
             "Bootstrap Mass selector chose the first scalar-product-rule-independent equal "
             "non-zero mass group \"msq\" with 2 non-auxiliary propagators on the current "
             "local declaration-order candidate surface",
         "Mass eta mode should treat exact local mass labels like msq as local mass-parameter "
         "tokens rather than automatically marking them dependent from scalar-product-rule RHS "
         "text");
}

void MassEtaModeFallsBackToFirstDependentGroupTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassDependentOnlySpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 6},
         "Mass eta mode should fall back to the first equal nonzero mass group when every local "
         "candidate group remains syntactically dependent");
  Expect(decision.selected_propagators ==
             std::vector<std::string>{spec.family.propagators[0].expression,
                                      spec.family.propagators[6].expression},
         "Mass eta mode fallback should preserve declaration order within the first dependent "
         "group");
  Expect(decision.explanation ==
             "Bootstrap Mass selector chose the first equal non-zero mass group \"t-msq\" with "
             "2 non-auxiliary propagators on the current local declaration-order candidate "
             "surface because no scalar-product-rule-independent group was available",
         "Mass eta mode should explain the dependent-only fallback honestly");
}

void MassEtaModeRejectsAllZeroMassSpecTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");

  ExpectRuntimeError(
      [&mode, &spec]() {
        static_cast<void>(mode->Plan(spec));
      },
      "eta mode Mass found no non-auxiliary non-zero-mass propagator group in bootstrap",
      "Mass eta mode should fail deterministically when every non-auxiliary propagator is "
      "massless");
}

void MassEtaModeRejectsAuxiliaryOnlyNonzeroMassSpecTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassAuxiliaryOnlyNonzeroSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");

  ExpectRuntimeError(
      [&mode, &spec]() {
        static_cast<void>(mode->Plan(spec));
      },
      "eta mode Mass found no non-auxiliary non-zero-mass propagator group in bootstrap",
      "Mass eta mode should ignore auxiliary-only nonzero masses and fail deterministically "
      "when no reviewed candidate remains");
}

void MassEtaModeDoesNotOverSelectAcrossDistinctMassGroupsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassOverSelectionSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 6},
         "Mass eta mode should not collapse to every massive propagator when only one equal-mass "
         "group should be chosen");
  Expect(decision.selected_propagator_indices.size() == 2,
         "Mass eta mode should keep the selected equal-mass group narrow");
}

void MassEtaModeSkipsAuxiliaryMembersInsideChosenMassGroupTest() {
  const amflow::ProblemSpec spec = MakeBuiltinMassMixedAuxiliarySharedMassSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Mass");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.selected_propagator_indices == std::vector<std::size_t>{0, 6},
         "Mass eta mode should select only the non-auxiliary members of a chosen equal-mass "
         "group");
  Expect(std::find(decision.selected_propagator_indices.begin(),
                   decision.selected_propagator_indices.end(),
                   1) == decision.selected_propagator_indices.end(),
         "Mass eta mode should not leak auxiliary propagators into a matching equal-mass group");
}

void BranchEtaModeHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Branch");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.mode_name == "Branch",
         "Branch eta mode should preserve the builtin mode name");
  Expect(decision.selected_propagator_indices == std::vector<std::size_t>({5, 0, 3}),
         "Branch eta mode should choose one uncut propagator per derived branch group on the "
         "supported sample topology");
  Expect(decision.selected_propagators ==
             std::vector<std::string>({"(k1-k2)^2", "(k1)^2", "(k2)^2"}),
         "Branch eta mode should preserve the selected propagator expressions in decision order");
  Expect(decision.explanation.find("Supported Branch selector chose 3 unique uncut propagators "
                                   "from 3 topology groups") != std::string::npos,
         "Branch eta mode should describe the supported topology-group selection");
}

void LoopEtaModeHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const auto mode = amflow::MakeBuiltinEtaMode("Loop");
  const amflow::EtaInsertionDecision decision = mode->Plan(spec);

  Expect(decision.mode_name == "Loop",
         "Loop eta mode should preserve the builtin mode name");
  Expect(decision.selected_propagator_indices == std::vector<std::size_t>({0, 3}),
         "Loop eta mode should choose the deduplicated first representatives of the derived "
         "loop groups on the supported sample topology");
  Expect(decision.selected_propagators == std::vector<std::string>({"(k1)^2", "(k2)^2"}),
         "Loop eta mode should preserve the selected propagator expressions in decision order");
  Expect(decision.explanation.find("after deduplicating repeated first-choice candidates") !=
             std::string::npos,
         "Loop eta mode should report when topology-group first choices collapse onto repeated "
         "propagators");
}

void UnsupportedBuiltinEtaModesRejectTest() {
  const amflow::ProblemSpec spec = MakeUnsupportedBranchLoopGrammarSpec();
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const auto mode = amflow::MakeBuiltinEtaMode(mode_name);
    const std::string message = CaptureRuntimeErrorMessage(
        [&mode, &spec]() {
          static_cast<void>(mode->Plan(spec));
        },
        "unsupported Branch/Loop propagator grammar should fail deterministically in bootstrap");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "unsupported Branch/Loop propagator grammar should fail deterministically in "
        "bootstrap");
    Expect(message.find("expected propagator expression of the form "
                        "\"(linear_momentum_combination)^2\"") != std::string::npos,
           "unsupported Branch/Loop blocker should surface the grammar reason");
  }
}

void UnsupportedBuiltinEtaModesRejectRepeatedSignGrammarTest() {
  const amflow::ProblemSpec spec = MakeRepeatedSignBranchLoopGrammarSpec();
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const auto mode = amflow::MakeBuiltinEtaMode(mode_name);
    const std::string message = CaptureRuntimeErrorMessage(
        [&mode, &spec]() {
          static_cast<void>(mode->Plan(spec));
        },
        "repeated-sign Branch/Loop grammar should fail deterministically in bootstrap");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "repeated-sign Branch/Loop grammar should fail deterministically in bootstrap");
    Expect(message.find("encountered repeated sign operator") != std::string::npos,
           "repeated-sign Branch/Loop blocker should surface the repeated-sign reason");
  }
}

void UnsupportedBuiltinEtaModesRejectTooManyTopSectorPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeTooManyPropagatorsForTopSectorMaskSpec();
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const auto mode = amflow::MakeBuiltinEtaMode(mode_name);
    const std::string message = CaptureRuntimeErrorMessage(
        [&mode, &spec]() {
          static_cast<void>(mode->Plan(spec));
        },
        "too-large top-sector masks should fail deterministically in bootstrap");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "too-large top-sector masks should fail deterministically in bootstrap");
    Expect(message.find("top-level sector bitmask support is limited to " +
                        std::to_string(std::numeric_limits<int>::digits) + " propagators") !=
               std::string::npos,
           "too-large top-sector blocker should surface the representable-mask limit");
  }
}

void UnsupportedBuiltinEtaModesRejectMissingTopLevelSectorTest() {
  ExpectBranchLoopBootstrapReason(MakeBranchLoopMissingTopLevelSectorSpec(),
                                  "current family definition does not provide a top-level sector",
                                  "missing top-level sector should fail deterministically in "
                                  "bootstrap");
}

void UnsupportedBuiltinEtaModesRejectMultipleTopLevelSectorsTest() {
  ExpectBranchLoopBootstrapReason(MakeBranchLoopMultipleTopLevelSectorsSpec(),
                                  "multiple top-level sectors remain deferred to the later "
                                  "multi-top-sector orchestration lane",
                                  "multiple top-level sectors should fail deterministically in "
                                  "bootstrap");
}

void UnsupportedBuiltinEtaModesRejectInactiveTopLevelSectorMaskTest() {
  ExpectBranchLoopBootstrapReason(MakeBranchLoopInactiveTopLevelSectorSpec(),
                                  "top-level sector bitmask selects no active propagators",
                                  "inactive top-level-sector masks should fail deterministically "
                                  "in bootstrap");
}

void UnsupportedBuiltinEtaModesRejectAllCutActiveCandidatesTest() {
  ExpectBranchLoopBootstrapReason(
      MakeBranchLoopAllCutActiveSpec(),
      "all active non-auxiliary propagators are cut-like, leaving no uncut Branch/Loop candidates",
      "all-cut active Branch/Loop candidates should fail deterministically in bootstrap");
}

void UnsupportedBuiltinEtaModesRejectEmptyFirstSymanzikSupportTest() {
  ExpectBranchLoopBootstrapReason(
      MakeBranchLoopEmptyFirstSymanzikSupportSpec(),
      "the derived first Symanzik support is empty on the current supported subset",
      "empty first-Symanzik support should fail deterministically in bootstrap");
}

void UnsupportedBuiltinEtaModesAllAuxiliaryVarProxyMissingTest() {
  const amflow::ProblemSpec spec = MakeBuiltinAllAuxiliarySpec();
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const auto mode = amflow::MakeBuiltinEtaMode(mode_name);
    const std::string message = CaptureRuntimeErrorMessage(
        [&mode, &spec]() {
          static_cast<void>(mode->Plan(spec));
        },
        "all-auxiliary unsupported builtin eta modes should fail deterministically in "
        "bootstrap");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "all-auxiliary unsupported builtin eta modes should fail deterministically in "
        "bootstrap");
    Expect(message.find("candidate_non_auxiliary_variables=0") != std::string::npos,
           "all-auxiliary Branch/Loop blockers should surface the zero non-auxiliary candidate "
           "count");
    Expect(message.find("var_proxy:<missing>(no declaration-order non-auxiliary propagators are "
                        "available for the current proxy list)") != std::string::npos,
           "all-auxiliary Branch/Loop blockers should report var_proxy as missing with an honest "
           "zero-candidate reason");
    Expect(message.find("var_proxy=<available>") == std::string::npos,
           "all-auxiliary Branch/Loop blockers should not advertise var_proxy as available");
  }
}

void ResolveEtaModeResolvesBuiltinNameWithoutUserDefinedOverrideTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "Custom";
  custom_decision.selected_propagator_indices = {0};
  custom_decision.selected_propagators = {spec.family.propagators[0].expression};
  custom_decision.explanation = "custom selection";
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "Custom");

  const std::shared_ptr<amflow::EtaMode> resolved =
      amflow::ResolveEtaMode("All", {custom_mode});
  const std::shared_ptr<amflow::EtaMode> builtin = amflow::MakeBuiltinEtaMode("All");
  const amflow::EtaInsertionDecision resolved_decision = resolved->Plan(spec);
  const amflow::EtaInsertionDecision builtin_decision = builtin->Plan(spec);

  Expect(SameEtaInsertionDecision(resolved_decision, builtin_decision),
         "eta-mode resolver should preserve builtin All planning semantics when no user-defined "
         "mode claims that builtin name");
  Expect(custom_mode->call_count() == 0,
         "eta-mode resolver should not consult unrelated user-defined mode planners while "
         "resolving a builtin name");
}

void ResolveEtaModeResolvesUniqueUserDefinedModeTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "CustomMode";
  custom_decision.selected_propagator_indices = {0, 6};
  custom_decision.selected_propagators = {
      spec.family.propagators[0].expression,
      spec.family.propagators[6].expression,
  };
  custom_decision.explanation = "custom user-defined mode";
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");

  const std::shared_ptr<amflow::EtaMode> resolved =
      amflow::ResolveEtaMode("CustomMode", {custom_mode});

  Expect(resolved == custom_mode,
         "eta-mode resolver should return the exact registered user-defined eta mode instance");
  Expect(custom_mode->call_count() == 0,
         "eta-mode resolver should not plan the user-defined mode during name resolution");

  const amflow::EtaInsertionDecision resolved_decision = resolved->Plan(spec);
  Expect(SameEtaInsertionDecision(resolved_decision, custom_decision),
         "eta-mode resolver should preserve user-defined planning behavior after resolution");
  Expect(custom_mode->call_count() == 1,
         "eta-mode resolver should leave planning to the resolved user-defined mode");
}

void ResolveEtaModeRejectsUnknownNameWithUserDefinedRegistryTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "CustomMode";
  custom_decision.selected_propagator_indices = {0};
  custom_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");

  ExpectInvalidArgument(
      [&custom_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("MissingMode", {custom_mode}));
      },
      "unknown eta mode: MissingMode",
      "eta-mode resolver should preserve unknown-name diagnostics when no user-defined mode "
      "matches");
  Expect(custom_mode->call_count() == 0,
         "eta-mode resolver should not plan user-defined modes when resolution fails");
}

void ResolveEtaModeRejectsDuplicateMatchingUserDefinedNamesTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "CustomMode";
  custom_decision.selected_propagator_indices = {0};
  custom_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto first_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");
  const auto second_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");

  ExpectInvalidArgument(
      [&first_mode, &second_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("CustomMode", {first_mode, second_mode}));
      },
      "duplicate user-defined eta mode: CustomMode",
      "eta-mode resolver should reject duplicate user-defined registrations for the same name");
  Expect(first_mode->call_count() == 0 && second_mode->call_count() == 0,
         "eta-mode resolver should not plan duplicate user-defined registrations while "
         "rejecting them");
}

void ResolveEtaModeRejectsDuplicateUserDefinedNamesUnrelatedToQueryTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision duplicate_decision;
  duplicate_decision.mode_name = "CustomMode";
  duplicate_decision.selected_propagator_indices = {0};
  duplicate_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto first_mode =
      std::make_shared<RecordingEtaMode>(duplicate_decision, "CustomMode");
  const auto second_mode =
      std::make_shared<RecordingEtaMode>(duplicate_decision, "CustomMode");

  ExpectInvalidArgument(
      [&first_mode, &second_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("All", {first_mode, second_mode}));
      },
      "duplicate user-defined eta mode: CustomMode",
      "eta-mode resolver should reject duplicate user-defined registrations anywhere in the "
      "supplied registry before resolving an unrelated name");
  Expect(first_mode->call_count() == 0 && second_mode->call_count() == 0,
         "eta-mode resolver should not plan duplicate user-defined registrations while "
         "rejecting an unrelated-name query");
}

void ResolveEtaModeRejectsBuiltinNameCollisionsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision collision_decision;
  collision_decision.mode_name = "All";
  collision_decision.selected_propagator_indices = {0};
  collision_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto colliding_mode =
      std::make_shared<RecordingEtaMode>(collision_decision, "All");

  ExpectInvalidArgument(
      [&colliding_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("All", {colliding_mode}));
      },
      "user-defined eta mode conflicts with builtin eta mode: All",
      "eta-mode resolver should reject user-defined names that collide with builtin eta modes");
  Expect(colliding_mode->call_count() == 0,
         "eta-mode resolver should not plan colliding user-defined modes while rejecting "
         "builtin-name collisions");
}

void ResolveEtaModeRejectsPrescriptionBuiltinNameCollisionsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision collision_decision;
  collision_decision.mode_name = "Prescription";
  collision_decision.selected_propagator_indices = {0};
  collision_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto colliding_mode =
      std::make_shared<RecordingEtaMode>(collision_decision, "Prescription");

  ExpectInvalidArgument(
      [&colliding_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("Prescription", {colliding_mode}));
      },
      "user-defined eta mode conflicts with builtin eta mode: Prescription",
      "eta-mode resolver should reject user-defined names that collide with builtin Prescription");
  Expect(colliding_mode->call_count() == 0,
         "eta-mode resolver should not plan Prescription-colliding user-defined modes while "
         "rejecting builtin-name collisions");
}

void ResolveEtaModeRejectsBuiltinNameCollisionsUnrelatedToQueryTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision collision_decision;
  collision_decision.mode_name = "All";
  collision_decision.selected_propagator_indices = {0};
  collision_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto colliding_mode =
      std::make_shared<RecordingEtaMode>(collision_decision, "All");

  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "CustomMode";
  custom_decision.selected_propagator_indices = {1};
  custom_decision.selected_propagators = {spec.family.propagators[1].expression};
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");

  ExpectInvalidArgument(
      [&colliding_mode, &custom_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("CustomMode", {colliding_mode, custom_mode}));
      },
      "user-defined eta mode conflicts with builtin eta mode: All",
      "eta-mode resolver should reject builtin-name collisions anywhere in the supplied "
      "registry before resolving an unrelated name");
  Expect(colliding_mode->call_count() == 0 && custom_mode->call_count() == 0,
         "eta-mode resolver should not plan user-defined modes while rejecting unrelated-name "
         "builtin collisions");
}

void ResolveEtaModeRejectsPrescriptionBuiltinNameCollisionsUnrelatedToQueryTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EtaInsertionDecision collision_decision;
  collision_decision.mode_name = "Prescription";
  collision_decision.selected_propagator_indices = {0};
  collision_decision.selected_propagators = {spec.family.propagators[0].expression};
  const auto colliding_mode =
      std::make_shared<RecordingEtaMode>(collision_decision, "Prescription");

  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "CustomMode";
  custom_decision.selected_propagator_indices = {1};
  custom_decision.selected_propagators = {spec.family.propagators[1].expression};
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");

  ExpectInvalidArgument(
      [&colliding_mode, &custom_mode]() {
        static_cast<void>(amflow::ResolveEtaMode("CustomMode", {colliding_mode, custom_mode}));
      },
      "user-defined eta mode conflicts with builtin eta mode: Prescription",
      "eta-mode resolver should reject builtin-name collisions anywhere in the supplied "
      "registry before resolving an unrelated name");
  Expect(colliding_mode->call_count() == 0 && custom_mode->call_count() == 0,
         "eta-mode resolver should not plan user-defined modes while rejecting unrelated-name "
         "Prescription collisions");
}

void ResolveEtaModeRejectsNullRegistryEntriesTest() {
  const std::vector<std::shared_ptr<amflow::EtaMode>> user_defined_modes = {nullptr};

  ExpectInvalidArgument(
      [&user_defined_modes]() {
        static_cast<void>(amflow::ResolveEtaMode("CustomMode", user_defined_modes));
      },
      "user-defined eta mode registry contains null entry",
      "eta-mode resolver should reject null user-defined registry entries deterministically");
}

void ResolveEndingSchemeResolvesBuiltinNameWithoutUserDefinedOverrideTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "Custom";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const std::shared_ptr<amflow::EndingScheme> resolved =
      amflow::ResolveEndingScheme("Trivial", {custom_scheme});
  const std::shared_ptr<amflow::EndingScheme> builtin =
      amflow::MakeBuiltinEndingScheme("Trivial");
  const amflow::EndingDecision resolved_decision = resolved->Plan(spec);
  const amflow::EndingDecision builtin_decision = builtin->Plan(spec);

  Expect(SameEndingDecision(resolved_decision, builtin_decision),
         "ending-scheme resolver should preserve builtin planning semantics when no user-defined "
         "scheme claims that builtin name");
  Expect(custom_scheme->call_count() == 0,
         "ending-scheme resolver should not consult unrelated user-defined planners while "
         "resolving a builtin name");
}

void ResolveEndingSchemeResolvesUniqueUserDefinedSchemeTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::eta->infinity", "custom::boundary"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const std::shared_ptr<amflow::EndingScheme> resolved =
      amflow::ResolveEndingScheme("CustomScheme", {custom_scheme});

  Expect(resolved == custom_scheme,
         "ending-scheme resolver should return the exact registered user-defined scheme instance");
  Expect(custom_scheme->call_count() == 0,
         "ending-scheme resolver should not plan the user-defined scheme during resolution");

  const amflow::EndingDecision resolved_decision = resolved->Plan(spec);
  Expect(SameEndingDecision(resolved_decision, custom_decision),
         "ending-scheme resolver should preserve user-defined planning behavior after "
         "resolution");
  Expect(custom_scheme->call_count() == 1,
         "ending-scheme resolver should leave planning to the resolved user-defined scheme");
}

void ResolveEndingSchemeRejectsUnknownNameWithUserDefinedRegistryTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&custom_scheme]() {
        static_cast<void>(amflow::ResolveEndingScheme("MissingScheme", {custom_scheme}));
      },
      "unknown ending scheme: MissingScheme",
      "ending-scheme resolver should preserve unknown-name diagnostics when no user-defined "
      "scheme matches");
  Expect(custom_scheme->call_count() == 0,
         "ending-scheme resolver should not plan user-defined schemes when resolution fails");
}

void ResolveEndingSchemeRejectsDuplicateMatchingUserDefinedNamesTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto first_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");
  const auto second_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&first_scheme, &second_scheme]() {
        static_cast<void>(
            amflow::ResolveEndingScheme("CustomScheme", {first_scheme, second_scheme}));
      },
      "duplicate user-defined ending scheme: CustomScheme",
      "ending-scheme resolver should reject duplicate user-defined registrations for the same "
      "name");
  Expect(first_scheme->call_count() == 0 && second_scheme->call_count() == 0,
         "ending-scheme resolver should not plan duplicate user-defined registrations while "
         "rejecting them");
}

void ResolveEndingSchemeRejectsDuplicateUserDefinedNamesUnrelatedToQueryTest() {
  amflow::EndingDecision duplicate_decision;
  duplicate_decision.terminal_strategy = "CustomScheme";
  duplicate_decision.terminal_nodes = {"custom::node"};
  const auto first_scheme =
      std::make_shared<RecordingEndingScheme>(duplicate_decision, "CustomScheme");
  const auto second_scheme =
      std::make_shared<RecordingEndingScheme>(duplicate_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&first_scheme, &second_scheme]() {
        static_cast<void>(amflow::ResolveEndingScheme("Trivial", {first_scheme, second_scheme}));
      },
      "duplicate user-defined ending scheme: CustomScheme",
      "ending-scheme resolver should reject duplicate user-defined registrations anywhere in the "
      "supplied registry before resolving an unrelated name");
  Expect(first_scheme->call_count() == 0 && second_scheme->call_count() == 0,
         "ending-scheme resolver should not plan duplicate user-defined registrations while "
         "rejecting an unrelated-name query");
}

void ResolveEndingSchemeRejectsBuiltinNameCollisionsTest() {
  amflow::EndingDecision collision_decision;
  collision_decision.terminal_strategy = "Trivial";
  collision_decision.terminal_nodes = {"collision::node"};
  const auto colliding_scheme =
      std::make_shared<RecordingEndingScheme>(collision_decision, "Trivial");

  ExpectInvalidArgument(
      [&colliding_scheme]() {
        static_cast<void>(amflow::ResolveEndingScheme("Trivial", {colliding_scheme}));
      },
      "user-defined ending scheme conflicts with builtin ending scheme: Trivial",
      "ending-scheme resolver should reject user-defined names that collide with builtin ending "
      "schemes");
  Expect(colliding_scheme->call_count() == 0,
         "ending-scheme resolver should not plan colliding user-defined schemes while "
         "rejecting builtin-name collisions");
}

void ResolveEndingSchemeRejectsBuiltinNameCollisionsUnrelatedToQueryTest() {
  amflow::EndingDecision collision_decision;
  collision_decision.terminal_strategy = "Trivial";
  collision_decision.terminal_nodes = {"collision::node"};
  const auto colliding_scheme =
      std::make_shared<RecordingEndingScheme>(collision_decision, "Trivial");

  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&colliding_scheme, &custom_scheme]() {
        static_cast<void>(
            amflow::ResolveEndingScheme("CustomScheme", {colliding_scheme, custom_scheme}));
      },
      "user-defined ending scheme conflicts with builtin ending scheme: Trivial",
      "ending-scheme resolver should reject builtin-name collisions anywhere in the supplied "
      "registry before resolving an unrelated name");
  Expect(colliding_scheme->call_count() == 0 && custom_scheme->call_count() == 0,
         "ending-scheme resolver should not plan user-defined schemes while rejecting "
         "unrelated-name builtin collisions");
}

void ResolveEndingSchemeRejectsNullRegistryEntriesTest() {
  const std::vector<std::shared_ptr<amflow::EndingScheme>> user_defined_schemes = {nullptr};

  ExpectInvalidArgument(
      [&user_defined_schemes]() {
        static_cast<void>(amflow::ResolveEndingScheme("CustomScheme", user_defined_schemes));
      },
      "user-defined ending scheme registry contains null entry",
      "ending-scheme resolver should reject null user-defined registry entries deterministically");
}

void PlanEndingSchemeBuiltinHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::EndingDecision expected =
      amflow::MakeBuiltinEndingScheme("Trivial")->Plan(spec);
  const amflow::EndingDecision decision =
      amflow::PlanEndingScheme(spec, "Trivial", {});

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "single-name ending planner should not mutate the input problem spec");
  Expect(SameEndingDecision(decision, expected),
         "single-name ending planner should preserve builtin planning behavior");
}

void PlanEndingSchemeUserDefinedHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::eta->infinity", "custom::region"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const amflow::EndingDecision decision =
      amflow::PlanEndingScheme(spec, "CustomScheme", {custom_scheme});

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "single-name ending planner should not mutate the input problem spec");
  Expect(SameEndingDecision(decision, custom_decision),
         "single-name ending planner should preserve user-defined planning behavior");
  Expect(custom_scheme->call_count() == 1,
         "single-name ending planner should plan the resolved user-defined scheme exactly once");
  Expect(custom_scheme->last_planned_spec_yaml() == original_yaml,
         "single-name ending planner should plan against the original problem spec");
}

void PlanEndingSchemeRejectsUnknownNameWithUserDefinedRegistryTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&custom_scheme]() {
        static_cast<void>(amflow::PlanEndingScheme(amflow::MakeSampleProblemSpec(),
                                                   "MissingScheme",
                                                   {custom_scheme}));
      },
      "unknown ending scheme: MissingScheme",
      "single-name ending planner should preserve unknown ending-scheme diagnostics");
  Expect(custom_scheme->call_count() == 0,
         "single-name ending planner should not plan user-defined schemes when resolution "
         "fails");
}

void PlanEndingSchemeRejectsRegistryValidationFailureTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto first_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");
  const auto second_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&first_scheme, &second_scheme]() {
        static_cast<void>(amflow::PlanEndingScheme(amflow::MakeSampleProblemSpec(),
                                                   "Trivial",
                                                   {first_scheme, second_scheme}));
      },
      "duplicate user-defined ending scheme: CustomScheme",
      "single-name ending planner should preserve registry-validation diagnostics");
  Expect(first_scheme->call_count() == 0 && second_scheme->call_count() == 0,
         "single-name ending planner should not plan any scheme when registry validation "
         "fails");
}

void PlanEndingSchemePlanningFailureTest() {
  amflow::EndingDecision unused_decision;
  unused_decision.terminal_strategy = "RetryScheme";
  unused_decision.terminal_nodes = {"unused::node"};
  const auto failing_scheme = std::make_shared<RecordingEndingScheme>(
      unused_decision, "RetryScheme", "retry ending planning failed");

  ExpectRuntimeError(
      [&failing_scheme]() {
        static_cast<void>(amflow::PlanEndingScheme(amflow::MakeSampleProblemSpec(),
                                                   "RetryScheme",
                                                   {failing_scheme}));
      },
      "retry ending planning failed",
      "single-name ending planner should preserve ending-scheme planning failures");
  Expect(failing_scheme->call_count() == 1,
         "single-name ending planner should call the resolved scheme exactly once before "
         "propagating a planning failure");
}

void PlanEndingSchemeListHappyPathFallbackTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  amflow::EndingDecision retry_decision;
  retry_decision.terminal_strategy = "RetryScheme";
  retry_decision.terminal_nodes = {"retry::node"};
  const auto retry_scheme = std::make_shared<RecordingEndingScheme>(
      retry_decision,
      "RetryScheme",
      "retry ending planning failed",
      PlanningFailureKind::InvalidArgument);

  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::eta->infinity", "custom::boundary"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const amflow::EndingDecision decision =
      amflow::PlanEndingSchemeList(spec,
                                   {"RetryScheme", "CustomScheme"},
                                   {retry_scheme, custom_scheme});

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "ordered ending-scheme selection should not mutate the input problem spec");
  Expect(SameEndingDecision(decision, custom_decision),
         "ordered ending-scheme selection should fall back in caller order to the first "
         "planning-successful scheme");
  Expect(retry_scheme->call_count() == 1,
         "ordered ending-scheme selection should plan the failing first scheme exactly once");
  Expect(custom_scheme->call_count() == 1,
         "ordered ending-scheme selection should plan the selected scheme exactly once");
  Expect(custom_scheme->last_planned_spec_yaml() == original_yaml,
         "ordered ending-scheme selection should plan the selected scheme against the original "
         "problem spec");
}

void PlanEndingSchemeListStopsAfterFirstCompletedSchemeTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::EndingDecision expected =
      amflow::MakeBuiltinEndingScheme("Trivial")->Plan(spec);
  amflow::EndingDecision unused_decision;
  unused_decision.terminal_strategy = "CustomScheme";
  unused_decision.terminal_nodes = {"custom::node"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(unused_decision, "CustomScheme");

  const amflow::EndingDecision decision =
      amflow::PlanEndingSchemeList(spec, {"Trivial", "CustomScheme"}, {custom_scheme});

  Expect(SameEndingDecision(decision, expected),
         "ordered ending-scheme selection should stop after the first completed scheme");
  Expect(custom_scheme->call_count() == 0,
         "ordered ending-scheme selection should not plan later schemes once selection "
         "completes");
}

void PlanEndingSchemeListRejectsEmptySchemeListTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::PlanEndingSchemeList(amflow::MakeSampleProblemSpec(), {}, {}));
      },
      "ending-scheme list must not be empty",
      "ordered ending-scheme selection should reject empty scheme lists deterministically");
}

void PlanEndingSchemeListRejectsUnknownNameImmediatelyTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::node"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectInvalidArgument(
      [&custom_scheme]() {
        static_cast<void>(amflow::PlanEndingSchemeList(amflow::MakeSampleProblemSpec(),
                                                       {"MissingScheme", "CustomScheme"},
                                                       {custom_scheme}));
      },
      "unknown ending scheme: MissingScheme",
      "ordered ending-scheme selection should preserve unknown-name diagnostics");
  Expect(custom_scheme->call_count() == 0,
         "ordered ending-scheme selection should not plan any scheme when name resolution "
         "fails immediately");
}

void PlanEndingSchemeListExhaustedKnownModesPreservesLastDiagnosticTest() {
  amflow::EndingDecision first_decision;
  first_decision.terminal_strategy = "RetryScheme";
  first_decision.terminal_nodes = {"retry::node"};
  const auto first_scheme = std::make_shared<RecordingEndingScheme>(
      first_decision, "RetryScheme", "retry ending planning failed");

  amflow::EndingDecision second_decision;
  second_decision.terminal_strategy = "LastScheme";
  second_decision.terminal_nodes = {"last::node"};
  const auto second_scheme = std::make_shared<RecordingEndingScheme>(
      second_decision, "LastScheme", "last ending planning failed");

  ExpectRuntimeError(
      [&first_scheme, &second_scheme]() {
        static_cast<void>(amflow::PlanEndingSchemeList(amflow::MakeSampleProblemSpec(),
                                                       {"RetryScheme", "LastScheme"},
                                                       {first_scheme, second_scheme}));
      },
      "last ending planning failed",
      "ordered ending-scheme selection should preserve the final planning diagnostic when no "
      "scheme completes");
  Expect(first_scheme->call_count() == 1 && second_scheme->call_count() == 1,
         "ordered ending-scheme selection should plan each failing scheme once before "
         "exhausting the list");
}

void PlanAmfOptionsEndingSchemeHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::AmfOptions amf_options =
      MakePoisonedAmfOptions({"NotUsed"}, {"RetryScheme", "CustomScheme"});

  amflow::EndingDecision retry_decision;
  retry_decision.terminal_strategy = "RetryScheme";
  retry_decision.terminal_nodes = {"retry::node"};
  const auto retry_scheme = std::make_shared<RecordingEndingScheme>(
      retry_decision,
      "RetryScheme",
      "retry ending planning failed",
      PlanningFailureKind::InvalidArgument);

  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::eta->infinity", "custom::boundary"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const amflow::EndingDecision baseline =
      amflow::PlanEndingSchemeList(spec,
                                   amf_options.ending_schemes,
                                   {retry_scheme, custom_scheme});

  const auto retry_scheme_for_wrapper = std::make_shared<RecordingEndingScheme>(
      retry_decision,
      "RetryScheme",
      "retry ending planning failed",
      PlanningFailureKind::InvalidArgument);
  const auto custom_scheme_for_wrapper =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const amflow::EndingDecision decision =
      amflow::PlanAmfOptionsEndingScheme(spec,
                                         amf_options,
                                         {retry_scheme_for_wrapper, custom_scheme_for_wrapper});

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "AmfOptions ending planner should not mutate the input problem spec");
  Expect(retry_scheme_for_wrapper->call_count() == 1 &&
             custom_scheme_for_wrapper->call_count() == 1,
         "AmfOptions ending planner should preserve ordered fallback with one planning probe per "
         "configured scheme");
  Expect(SameEndingDecision(decision, baseline),
         "AmfOptions ending planner should forward the configured ending_schemes list unchanged "
         "into the ordered ending-selection path");
}

void PlanAmfOptionsEndingSchemeUsesDefaultEndingSchemeListTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::AmfOptions amf_options;
  const amflow::EndingDecision baseline =
      amflow::PlanEndingSchemeList(spec, amf_options.ending_schemes, {});
  const amflow::EndingDecision decision =
      amflow::PlanAmfOptionsEndingScheme(spec, amf_options, {});

  Expect(SameEndingDecision(decision, baseline) && decision.terminal_strategy == "Tradition",
         "AmfOptions ending planner should use the default ending_schemes list unchanged");
}

void PlanAmfOptionsEndingSchemeRejectsEmptyEndingSchemeListTest() {
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"NotUsed"}, {});

  ExpectInvalidArgument(
      [&amf_options]() {
        static_cast<void>(
            amflow::PlanAmfOptionsEndingScheme(amflow::MakeSampleProblemSpec(), amf_options, {}));
      },
      "ending-scheme list must not be empty",
      "AmfOptions ending planner should preserve empty ending-scheme list diagnostics");
}

void PlanAmfOptionsEndingSchemeRejectsUnknownNameImmediatelyTest() {
  const amflow::AmfOptions amf_options =
      MakePoisonedAmfOptions({"NotUsed"}, {"MissingScheme", "Trivial"});

  ExpectInvalidArgument(
      [&amf_options]() {
        static_cast<void>(
            amflow::PlanAmfOptionsEndingScheme(amflow::MakeSampleProblemSpec(), amf_options, {}));
      },
      "unknown ending scheme: MissingScheme",
      "AmfOptions ending planner should preserve unknown-name diagnostics");
}

void PlanAmfOptionsEndingSchemeExhaustedKnownModesPreservesLastDiagnosticTest() {
  const amflow::AmfOptions amf_options =
      MakePoisonedAmfOptions({"NotUsed"}, {"RetryScheme", "LastScheme"});

  amflow::EndingDecision retry_decision;
  retry_decision.terminal_strategy = "RetryScheme";
  retry_decision.terminal_nodes = {"retry::node"};
  const auto retry_scheme = std::make_shared<RecordingEndingScheme>(
      retry_decision, "RetryScheme", "retry ending planning failed");

  amflow::EndingDecision last_decision;
  last_decision.terminal_strategy = "LastScheme";
  last_decision.terminal_nodes = {"last::node"};
  const auto last_scheme = std::make_shared<RecordingEndingScheme>(
      last_decision, "LastScheme", "last ending planning failed");

  ExpectRuntimeError(
      [&amf_options, &retry_scheme, &last_scheme]() {
        static_cast<void>(amflow::PlanAmfOptionsEndingScheme(amflow::MakeSampleProblemSpec(),
                                                             amf_options,
                                                             {retry_scheme, last_scheme}));
      },
      "last ending planning failed",
      "AmfOptions ending planner should preserve the final planning diagnostic when the "
      "configured ending list exhausts");
  Expect(retry_scheme->call_count() == 1 && last_scheme->call_count() == 1,
         "AmfOptions ending planner should plan each failing configured scheme once before "
         "exhausting the list");
}

void KiraPreparationTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  const auto layout = amflow::EnsureArtifactLayout(std::filesystem::temp_directory_path() /
                                                   "amflow-bootstrap-kira-test");
  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);

  Expect(preparation.validation_messages.empty(),
         "sample problem should prepare cleanly for Kira");
  Expect(preparation.generated_files.count("config/integralfamilies.yaml") == 1,
         "integralfamilies.yaml should be emitted");
  Expect(preparation.generated_files.count("config/kinematics.yaml") == 1,
         "kinematics.yaml should be emitted");
  Expect(preparation.generated_files.count("jobs.yaml") == 1,
         "jobs.yaml should be emitted");

  const auto write_messages = amflow::WritePreparationFiles(preparation, layout);
  Expect(write_messages.empty(), "sample preparation files should write cleanly");
  Expect(std::filesystem::exists(layout.generated_config_dir / "config/integralfamilies.yaml"),
         "integralfamilies.yaml should be written to disk");

  const auto jobs_it = preparation.generated_files.find("jobs.yaml");
  Expect(jobs_it != preparation.generated_files.end(), "jobs.yaml should be available");
  Expect(jobs_it->second.find("run_back_substitution: true") != std::string::npos,
         "bootstrap Kira job should request back substitution");
}

void KiraPreparationEmitsKira31CompatibleYamlFragmentsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::KiraBackend backend;
  const amflow::KiraJobFiles files = backend.EmitJobFiles(spec, MakeKiraReductionOptions());

  Expect(files.jobs_yaml.find("select_mandatory_list:\n"
                              "          - [\"planar_double_box\", target]\n") !=
             std::string::npos,
         "Kira jobs.yaml should use a file-based select_mandatory_list entry");
  Expect(files.jobs_yaml.find("  - kira2math:\n"
                              "      target:\n"
                              "        - [\"planar_double_box\", target]\n") !=
             std::string::npos,
         "Kira jobs.yaml should restore the kira2math export block for the target file");
  Expect(files.jobs_yaml.find("[planar_double_box, target]") == std::string::npos,
         "Kira jobs.yaml should quote the family name in the selector tuple");
  Expect(files.integralfamilies_yaml.find("top_level_sectors: [127]\n") != std::string::npos &&
             files.integralfamilies_yaml.find("[\"(k2-p1-p2)^2\", \"0\"]") !=
                 std::string::npos,
         "Kira family YAML should preserve the reviewed shared-sample topology");

  Expect(files.kinematics_yaml.find("momentum_conservation: [p4, -p1-p2-p3]\n") !=
             std::string::npos,
         "Kira kinematics.yaml should emit Kira's dependent-momentum conservation pair");
  Expect(files.kinematics_yaml.find("    - [[p1, p1], 0]\n") != std::string::npos &&
             files.kinematics_yaml.find("    - [[p2, p2], 0]\n") != std::string::npos &&
             files.kinematics_yaml.find("    - [[p1, p2], s/2]\n") != std::string::npos &&
             files.kinematics_yaml.find("[[p3, p3], msq]") == std::string::npos,
         "Kira kinematics.yaml should emit the reviewed shared-sample scalarproduct_rules set");
  Expect(files.kinematics_yaml.find("[\"p1 + p2 + p3 + p4 = 0\"]") == std::string::npos &&
             files.kinematics_yaml.find("[\"p1*p1\", \"0\"]") == std::string::npos,
         "Kira kinematics.yaml should no longer emit legacy scalar-string forms");
  Expect(files.target_list == "planar_double_box[1,1,1,1,1,1,1]\n",
         "Kira target emission should preserve the reviewed shared-sample target");
  Expect(files.preferred_masters == "planar_double_box[1,1,1,1,1,1,1]\n\n",
         "Kira sample emission should preserve the reviewed preferred-master list");
}

void KiraPreparationFromFileSpecTest() {
  const std::filesystem::path path =
      std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.example.yaml";
  const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(path);

  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  const auto layout = amflow::EnsureArtifactLayout(std::filesystem::temp_directory_path() /
                                                   "amflow-bootstrap-kira-file-test");
  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);

  Expect(preparation.validation_messages.empty(),
         "file-backed problem spec should prepare cleanly for Kira");
  Expect(preparation.generated_files.count("config/integralfamilies.yaml") == 1,
         "file-backed spec should emit integralfamilies.yaml");
  Expect(preparation.generated_files.at("config/integralfamilies.yaml")
             .find("\"(k2-p1-p2)^2\"") != std::string::npos,
         "file-backed shared sample should preserve the reviewed propagator list");
  Expect(preparation.generated_files.at("target") == "planar_double_box[1,1,1,1,1,1,1]\n",
         "file-backed shared sample should preserve the reviewed target list");
  Expect(preparation.generated_files.at("preferred") ==
             "planar_double_box[1,1,1,1,1,1,1]\n\n",
         "file-backed shared sample should preserve the reviewed preferred-master list");
}

void K0SmokeKiraPreparationFromFileSpecTest() {
  const std::filesystem::path path =
      std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml";
  const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(path);

  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  const auto layout = amflow::EnsureArtifactLayout(std::filesystem::temp_directory_path() /
                                                   "amflow-bootstrap-k0-smoke-kira-file-test");
  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);
  const std::string& kinematics_yaml = preparation.generated_files.at("config/kinematics.yaml");
  const std::string expected_normalized_kinematics_yaml =
      "kinematics:\n"
      "  incoming_momenta: [\"p1\", \"p2\"]\n"
      "  outgoing_momenta: [\"p3\", \"p4\"]\n"
      "  momentum_conservation: [p4, -p1-p2-p3]\n"
      "  kinematic_invariants:\n"
      "    - [\"s\", 2]\n"
      "    - [\"t\", 2]\n"
      "    - [\"msq\", 2]\n"
      "  scalarproduct_rules:\n"
      "    - [[p1, p1], 0]\n"
      "    - [[p2, p2], 0]\n"
      "    - [[p3, p3], msq]\n"
      "    - [[p1, p2], s/2]\n"
      "    - [[p1, p3], (t-msq)/2]\n"
      "    - [[p2, p3], (msq-s-t)/2]\n";
  const std::string expected_target_list =
      "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0]\n"
      "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-2,-1]\n"
      "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-1,-2]\n"
      "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,0,-3]\n";

  Expect(preparation.validation_messages.empty(),
         "K0 smoke spec should prepare cleanly for Kira");
  Expect(preparation.generated_files.count("config/integralfamilies.yaml") == 1,
         "K0 smoke spec should emit integralfamilies.yaml");
  Expect(preparation.generated_files.count("jobs.yaml") == 1,
         "K0 smoke spec should emit jobs.yaml");
  Expect(preparation.generated_files.at("config/integralfamilies.yaml")
             .find("\"automatic_vs_manual_k0_smoke\"") != std::string::npos &&
             preparation.generated_files.at("config/integralfamilies.yaml")
                     .find("top_level_sectors: [127]\n") != std::string::npos &&
             preparation.generated_files.at("config/integralfamilies.yaml")
                     .find("[\"(k2+p3)^2\", \"msq\"]") != std::string::npos &&
             preparation.generated_files.at("config/integralfamilies.yaml")
                     .find("[\"(k2+p1)^2\", \"0\"]") != std::string::npos,
         "K0 smoke Kira preparation should preserve the repo-local frozen 9-propagator family");
  Expect(kinematics_yaml == expected_normalized_kinematics_yaml,
         "K0 smoke Kira preparation should emit the pinned normalized Kira kinematics.yaml "
         "for the repo-local frozen fixture");
  Expect(kinematics_yaml.find("momentum_conservation: \"p1 + p2 + p3 + p4 = 0\"") ==
                 std::string::npos &&
             kinematics_yaml.find("  invariants: [\"s\", \"t\", \"msq\"]\n") ==
                 std::string::npos &&
             kinematics_yaml.find("    - left: \"p1*p1\"\n") == std::string::npos &&
             kinematics_yaml.find("      right: \"0\"\n") == std::string::npos &&
             kinematics_yaml.find("[\"p1 + p2 + p3 + p4 = 0\"]") == std::string::npos &&
             kinematics_yaml.find("[\"p1*p1\", \"0\"]") == std::string::npos,
         "K0 smoke Kira preparation should not emit pre-normalized source-spec or quoted-scalar "
         "legacy kinematics forms");
  Expect(preparation.generated_files.at("target") == expected_target_list,
         "K0 smoke Kira preparation should preserve the repo-local frozen target list");
  Expect(preparation.generated_files.at("jobs.yaml")
                 .find("        - {topologies: [\"automatic_vs_manual_k0_smoke\"], sectors: "
                       "[127], r: 7, s: 3, d: 0}\n") != std::string::npos &&
             preparation.generated_files.at("jobs.yaml").find("sectors: [511]") ==
                 std::string::npos,
         "K0 smoke Kira preparation should derive the Kira seed from the narrowed seven-line "
         "sector and its r value");
  Expect(preparation.generated_files.at("preferred").empty(),
         "K0 smoke Kira preparation should not emit an invented preferred-master list");
}

void KiraPrepareForTargetsHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-explicit-targets"));
  const std::vector<amflow::TargetIntegral> targets = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 0}},
      {"planar_double_box", {2, 1, 1, 1, 1, 1, 1}},
  };

  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.PrepareForTargets(spec, MakeKiraReductionOptions(), layout, targets);

  Expect(preparation.validation_messages.empty(),
         "explicit-target Kira preparation should validate for a well-formed override list");
  const auto target_it = preparation.generated_files.find("target");
  Expect(target_it != preparation.generated_files.end(),
         "explicit-target Kira preparation should emit the target file");
  Expect(target_it->second == "planar_double_box[1,1,1,1,1,1,0]\n"
                              "planar_double_box[2,1,1,1,1,1,1]\n",
         "explicit-target Kira preparation should preserve override-target order exactly");
}

void KiraPrepareStillUsesProblemSpecTargetsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-default-targets"));

  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.Prepare(spec, MakeKiraReductionOptions(), layout);

  const auto target_it = preparation.generated_files.find("target");
  Expect(target_it != preparation.generated_files.end(),
         "default Kira preparation should emit the target file");
  Expect(target_it->second == spec.targets[0].Label() + "\n",
         "default Kira preparation should continue using ProblemSpec.targets");
}

void KiraPrepareForTargetsRejectsEmptyTargetListTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-empty-explicit-targets"));

  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.PrepareForTargets(spec, MakeKiraReductionOptions(), layout, {});

  Expect(ContainsSubstring(preparation.validation_messages,
                           "explicit Kira target list must not be empty"),
         "explicit-target Kira preparation should reject empty target lists");
}

void KiraPrepareForTargetsRejectsDuplicateTargetsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-kira-duplicate-explicit-targets"));
  const std::vector<amflow::TargetIntegral> targets = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 0}},
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 0}},
  };

  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.PrepareForTargets(spec, MakeKiraReductionOptions(), layout, targets);

  Expect(ContainsSubstring(preparation.validation_messages,
                           "duplicate explicit Kira target: planar_double_box[1,1,1,1,1,1,0]"),
         "explicit-target Kira preparation should reject duplicate targets");
}

void KiraPrepareForTargetsRejectsFamilyMismatchTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-kira-explicit-target-family-mismatch"));
  const std::vector<amflow::TargetIntegral> targets = {
      {"wrong_family", {1, 1, 1, 1, 1, 1, 0}},
  };

  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.PrepareForTargets(spec, MakeKiraReductionOptions(), layout, targets);

  Expect(
      ContainsSubstring(preparation.validation_messages,
                        "explicit Kira target family does not match family.name"),
      "explicit-target Kira preparation should reject family mismatches");
}

void KiraPrepareForTargetsRejectsArityMismatchTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-kira-explicit-target-arity-mismatch"));
  const std::vector<amflow::TargetIntegral> targets = {
      {"planar_double_box", {1, 1}},
  };

  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.PrepareForTargets(spec, MakeKiraReductionOptions(), layout, targets);

  Expect(
      ContainsSubstring(preparation.validation_messages,
                        "explicit Kira target index count must match family.propagators size"),
      "explicit-target Kira preparation should reject arity mismatches");
}

void KiraExecutionCommandTest() {
  const auto layout = amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-command"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, "#!/bin/sh\nexit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::KiraBackend backend;
  const amflow::PreparedCommand command =
      backend.MakeExecutionCommand(layout, kira_path, fermat_path);

  Expect(command.label == "kira", "execution command should be labeled kira");
  Expect(command.executable == std::filesystem::absolute(kira_path),
         "execution command should keep the explicit kira path");
  Expect(command.arguments.size() == 1,
         "execution command should pass a single jobs.yaml argument");
  Expect(command.arguments.front().find("jobs.yaml") != std::string::npos,
         "execution command should target jobs.yaml");
  Expect(command.environment_overrides.at("FERMATPATH") ==
             std::filesystem::absolute(fermat_path).string(),
         "execution command should pass the explicit fermat path");
  Expect(command.working_directory == layout.generated_config_dir,
         "execution command should preserve the generated-config working directory");
}

void KiraExecutionSuccessTest() {
  const auto layout = amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-success"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        "#!/bin/sh\n"
                        "echo \"stdout:$1\"\n"
                        "echo \"fermat:$FERMATPATH\"\n"
                        "echo \"stderr:$1\" 1>&2\n"
                        "exit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::Completed,
         "successful fake Kira run should complete");
  Expect(result.exit_code == 0, "successful fake Kira run should exit with code 0");
  Expect(result.Succeeded(), "successful fake Kira run should be marked successful");
  Expect(std::filesystem::exists(result.stdout_log_path), "stdout log should exist");
  Expect(std::filesystem::exists(result.stderr_log_path), "stderr log should exist");

  const std::string stdout_log = ReadFile(result.stdout_log_path);
  const std::string stderr_log = ReadFile(result.stderr_log_path);
  Expect(stdout_log.find("stdout:") != std::string::npos,
         "stdout log should capture fake Kira output");
  Expect(stdout_log.find("jobs.yaml") != std::string::npos,
         "stdout log should mention the generated jobs file");
  Expect(stdout_log.find("fermat:" + std::filesystem::absolute(fermat_path).string()) !=
             std::string::npos,
         "stdout log should capture the explicit FERMATPATH override");
  Expect(stderr_log.find("stderr:") != std::string::npos,
         "stderr log should capture fake Kira stderr");
  Expect(result.command.find(kira_path.string()) != std::string::npos,
         "execution result should record the fake kira command");
  Expect(result.working_directory == layout.generated_config_dir,
         "execution result should record the actual execution working directory");
}

void KiraExecutionFailureTest() {
  const auto layout = amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        "#!/bin/sh\n"
                        "echo \"expected-failure\" 1>&2\n"
                        "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::Completed,
         "nonzero fake Kira exit should still be a completed process");
  Expect(result.exit_code == 9, "fake Kira failure should propagate its exit code");
  Expect(!result.Succeeded(), "nonzero fake Kira exit should not be marked successful");
  Expect(result.error_message.find("code 9") != std::string::npos,
         "failure result should report the exit code");
  Expect(ReadFile(result.stderr_log_path).find("expected-failure") != std::string::npos,
         "stderr log should preserve failure output");
}

void KiraExecutionExit127RemainsCompletedTest() {
  const auto layout = amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-exit-127"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-exit-127.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, "#!/bin/sh\nexit 127\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::Completed,
         "a real reducer process that exits 127 should still be reported as completed");
  Expect(result.exit_code == 127,
         "a real reducer process should preserve its 127 exit code");
  Expect(!result.Succeeded(), "exit 127 should not be marked successful");
}

void KiraExecutionUsesUniqueAttemptLogsTest() {
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-attempt-logs"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-attempts.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  const std::filesystem::path marker_path = layout.generated_config_dir / "run-marker.txt";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, "#!/bin/sh\ncat \"$PWD/run-marker.txt\"\nexit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);

  {
    std::ofstream stream(marker_path);
    stream << "first-attempt\n";
  }
  const amflow::CommandExecutionResult first_result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  {
    std::ofstream stream(marker_path, std::ios::out | std::ios::trunc);
    stream << "second-attempt\n";
  }
  const amflow::CommandExecutionResult second_result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  Expect(first_result.status == amflow::CommandExecutionStatus::Completed,
         "first repeated-run attempt should complete");
  Expect(second_result.status == amflow::CommandExecutionStatus::Completed,
         "second repeated-run attempt should complete");
  Expect(first_result.attempt_number == 1,
         "first repeated run should use attempt number 1 in a fresh layout");
  Expect(second_result.attempt_number == 2,
         "second repeated run should allocate the next attempt number");
  Expect(first_result.stdout_log_path != second_result.stdout_log_path,
         "repeated runs should allocate unique stdout log paths");
  Expect(first_result.stderr_log_path != second_result.stderr_log_path,
         "repeated runs should allocate unique stderr log paths");
  Expect(first_result.stdout_log_path.filename().string().find("attempt-0001") !=
             std::string::npos,
         "first repeated run should use an attempt-scoped stdout log name");
  Expect(second_result.stdout_log_path.filename().string().find("attempt-0002") !=
             std::string::npos,
         "second repeated run should use the next attempt-scoped stdout log name");
  Expect(ReadFile(first_result.stdout_log_path).find("first-attempt") != std::string::npos,
         "the first attempt log should preserve its original output");
  Expect(ReadFile(second_result.stdout_log_path).find("second-attempt") != std::string::npos,
         "the second attempt log should capture the later output");
}

void KiraExecutionRejectsInvalidConfigurationTest() {
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-invalid-config"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, "#!/bin/sh\nexit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::KiraBackend backend;
  amflow::BackendPreparation invalid_preparation;
  invalid_preparation.backend_name = backend.Name();
  invalid_preparation.validation_messages = {"synthetic validation failure"};

  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(invalid_preparation, layout, kira_path, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::InvalidConfiguration,
         "validation failures should block Kira execution");
  Expect(result.exit_code == -1,
         "invalid configuration should not report a process exit code");
  Expect(result.error_message.find("synthetic validation failure") != std::string::npos,
         "invalid configuration result should preserve the validation reason");
  Expect(result.working_directory == layout.generated_config_dir,
         "invalid configuration should still record the execution working directory");
  Expect(std::filesystem::exists(result.stdout_log_path),
         "invalid configuration should still materialize stdout logs");
  Expect(std::filesystem::exists(result.stderr_log_path),
         "invalid configuration should still materialize stderr logs");
  Expect(ReadFile(result.stdout_log_path).find("synthetic validation failure") !=
             std::string::npos,
         "stdout log should capture the invalid-configuration reason");
  Expect(ReadFile(result.stderr_log_path).find("invalid-configuration") != std::string::npos,
         "stderr log should capture the invalid-configuration status");
}

void KiraExecutionRejectsMissingExecutableWithLogsTest() {
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-missing-executable"));
  const std::filesystem::path missing_kira = layout.root / "bin" / "missing-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(fermat_path.parent_path());
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, missing_kira, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::InvalidConfiguration,
         "missing executable should be reported as invalid configuration");
  Expect(std::filesystem::exists(result.stdout_log_path),
         "missing executable rejection should still create stdout logs");
  Expect(std::filesystem::exists(result.stderr_log_path),
         "missing executable rejection should still create stderr logs");
  Expect(ReadFile(result.stdout_log_path).find("missing or not executable") !=
             std::string::npos,
         "stdout log should capture the missing executable reason");
}

void KiraExecutionRejectsMissingPreparedFilesTest() {
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-missing-prepared-file"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, "#!/bin/sh\nexit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  amflow::BackendPreparation preparation = backend.Prepare(spec, options, layout);
  preparation.generated_files.erase("target");
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::InvalidConfiguration,
         "missing prepared files should be rejected before Kira execution");
  Expect(result.error_message.find("prepared Kira file was not generated: target") !=
             std::string::npos,
         "missing prepared-file rejection should identify the missing file");
  Expect(std::filesystem::exists(result.stdout_log_path),
         "missing prepared-file rejection should create stdout logs");
  Expect(ReadFile(result.stderr_log_path).find("prepared Kira file was not generated: target") !=
             std::string::npos,
         "stderr log should capture the missing prepared-file reason");
}

void KiraExecutionExecFailureIsFailedToStartTest() {
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-kira-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-bad-interpreter.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, "#!/definitely/missing/interpreter\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;

  amflow::KiraBackend backend;
  const auto preparation = backend.Prepare(spec, options, layout);
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  Expect(result.status == amflow::CommandExecutionStatus::FailedToStart,
         "execve startup failures should be classified as failed-to-start");
  Expect(result.exit_code == -1,
         "execve startup failures should not report a reducer exit code");
  Expect(!result.Succeeded(), "execve startup failures should not be marked successful");
  Expect(result.error_message.find("failed to exec kira executable") != std::string::npos,
         "execve startup failures should preserve the startup error");
  Expect(std::filesystem::exists(result.stderr_log_path),
         "execve startup failures should still materialize stderr logs");
  Expect(ReadFile(result.stderr_log_path).find("failed to exec kira executable") !=
             std::string::npos,
         "stderr logs should capture execve startup failures");
}

std::filesystem::path KiraResultsFixtureFamilyRoot(const std::string& fixture_name) {
  return TestDataRoot() / "kira-results" / fixture_name / "results" / "planar_double_box";
}

void CopyKiraResultsFixtureFamily(const std::filesystem::path& fixture_family_root,
                                  const std::filesystem::path& destination_family_root,
                                  const bool copy_masters = true,
                                  const bool copy_rules = true) {
  std::filesystem::create_directories(destination_family_root);
  if (copy_masters) {
    std::filesystem::copy_file(fixture_family_root / "masters",
                               destination_family_root / "masters",
                               std::filesystem::copy_options::overwrite_existing);
  }
  if (copy_rules) {
    std::filesystem::copy_file(fixture_family_root / "kira_target.m",
                               destination_family_root / "kira_target.m",
                               std::filesystem::copy_options::overwrite_existing);
  }
}

void KiraParsedResultsHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const std::filesystem::path family_root = root / "results" / "planar_double_box";
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(root, "planar_double_box");
  const auto& first_rule =
      FindRuleByTarget(result.rules, "planar_double_box[2,1,1,1,1,1,1]");

  Expect(result.status == amflow::ParsedReductionStatus::ParsedRules,
         "happy-path fixture should parse explicit rules");
  Expect(result.master_list.masters.size() == 2,
         "happy-path fixture should expose two parsed masters");
  Expect(result.explicit_rule_count == 2,
         "happy-path fixture should keep the explicit target-rule count");
  Expect(result.rules.size() == 4,
         "happy-path fixture should append identity rules for the masters");
  Expect(result.master_list.masters[0].Label() == "planar_double_box[1,1,1,1,1,1,1]",
         "happy-path fixture should preserve the first master label");
  Expect(result.master_list.source_path == family_root / "masters",
         "happy-path fixture should parse masters from the direct results tree");
  Expect(result.rule_path == family_root / "kira_target.m",
         "happy-path fixture should parse rules from the direct results tree");
  Expect(first_rule.target.Label() == "planar_double_box[2,1,1,1,1,1,1]",
         "happy-path fixture should preserve the first reduction target");
  Expect(first_rule.terms.size() == 2,
         "happy-path fixture should parse both linear terms in the first rule");
  Expect(FindTermByMaster(first_rule, "planar_double_box[1,1,1,1,1,1,1]").coefficient == "1",
         "a bare master on the right-hand side should normalize to coefficient 1");
  Expect(FindTermByMaster(first_rule, "planar_double_box[1,1,1,1,1,1,0]").coefficient == "-2",
         "a signed numeric multiplier should normalize deterministically");
  Expect(result.rules[2].target.Label() == result.master_list.masters[0].Label(),
         "identity rules should be appended after explicit rules");
  Expect(result.rules[2].terms.size() == 1 && result.rules[2].terms[0].coefficient == "1",
         "identity rules should keep unit coefficients");
}

void KiraParsedResultsResolveGeneratedConfigReducerRootTest() {
  const std::filesystem::path fixture_family_root = KiraResultsFixtureFamilyRoot("happy");
  const std::filesystem::path reducer_root =
      FreshTempDir("amflow-bootstrap-kira-generated-config-reducer-root");
  const std::filesystem::path generated_family_root =
      reducer_root / "generated-config" / "results" / "planar_double_box";
  CopyKiraResultsFixtureFamily(fixture_family_root, generated_family_root);

  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(reducer_root, "planar_double_box");

  Expect(result.status == amflow::ParsedReductionStatus::ParsedRules,
         "outer reducer-root parsing should still recover explicit Kira rules");
  Expect(result.master_list.source_path == generated_family_root / "masters",
         "outer reducer-root parsing should resolve masters from generated-config/results");
  Expect(result.rule_path == generated_family_root / "kira_target.m",
         "outer reducer-root parsing should resolve the rule file next to the parsed masters");
  Expect(result.explicit_rule_count == 2,
         "outer reducer-root parsing should preserve the explicit-rule count");
  Expect(FindRuleByTarget(result.rules, "planar_double_box[2,1,1,1,1,1,1]").terms.size() == 2,
         "outer reducer-root parsing should preserve the happy-path reduction content");
}

void KiraParsedResultsPreferCompleteGeneratedConfigOverCompleteDirectTest() {
  const std::filesystem::path reducer_root =
      FreshTempDir("amflow-bootstrap-kira-generated-config-wins-complete-tie");
  const std::filesystem::path direct_family_root =
      reducer_root / "results" / "planar_double_box";
  const std::filesystem::path generated_family_root =
      reducer_root / "generated-config" / "results" / "planar_double_box";
  CopyKiraResultsFixtureFamily(KiraResultsFixtureFamilyRoot("happy"), direct_family_root);
  CopyKiraResultsFixtureFamily(KiraResultsFixtureFamilyRoot("canonicalized-rules"),
                               generated_family_root);

  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(reducer_root, "planar_double_box");
  const auto& summed_rule =
      FindRuleByTarget(result.rules, "planar_double_box[2,1,1,1,1,1,1]");

  Expect(result.status == amflow::ParsedReductionStatus::ParsedRules,
         "complete generated-config should beat a complete direct tree");
  Expect(result.master_list.source_path == generated_family_root / "masters",
         "complete generated-config should win ties for the masters path");
  Expect(result.rule_path == generated_family_root / "kira_target.m",
         "complete generated-config should win ties for the rule path");
  Expect(result.explicit_rule_count == 1,
         "complete generated-config should contribute its explicit-rule content");
  Expect(summed_rule.terms.size() == 1 && summed_rule.terms[0].coefficient == "5",
         "complete generated-config should preserve the selected rule payload");
}

void KiraParsedResultsCompleteDirectBeatsGeneratedConfigMastersOnlyTest() {
  const std::filesystem::path reducer_root =
      FreshTempDir("amflow-bootstrap-kira-direct-wins-generated-masters-only");
  const std::filesystem::path direct_family_root =
      reducer_root / "results" / "planar_double_box";
  const std::filesystem::path generated_family_root =
      reducer_root / "generated-config" / "results" / "planar_double_box";
  CopyKiraResultsFixtureFamily(KiraResultsFixtureFamilyRoot("happy"), direct_family_root);
  CopyKiraResultsFixtureFamily(KiraResultsFixtureFamilyRoot("happy"),
                               generated_family_root,
                               true,
                               false);

  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(reducer_root, "planar_double_box");

  Expect(result.status == amflow::ParsedReductionStatus::ParsedRules,
         "generated-config masters-only should not shadow a complete direct tree");
  Expect(result.master_list.source_path == direct_family_root / "masters",
         "generated-config masters-only should leave the direct masters path selected");
  Expect(result.rule_path == direct_family_root / "kira_target.m",
         "generated-config masters-only should leave the direct rule path selected");
  Expect(result.explicit_rule_count == 2,
         "generated-config masters-only should preserve the complete direct explicit rules");
}

void KiraParsedResultsCompleteDirectBeatsGeneratedConfigRuleOnlyTest() {
  const std::filesystem::path reducer_root =
      FreshTempDir("amflow-bootstrap-kira-direct-wins-generated-rule-only");
  const std::filesystem::path direct_family_root =
      reducer_root / "results" / "planar_double_box";
  const std::filesystem::path generated_family_root =
      reducer_root / "generated-config" / "results" / "planar_double_box";
  CopyKiraResultsFixtureFamily(KiraResultsFixtureFamilyRoot("happy"), direct_family_root);
  CopyKiraResultsFixtureFamily(KiraResultsFixtureFamilyRoot("canonicalized-rules"),
                               generated_family_root,
                               false,
                               true);

  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(reducer_root, "planar_double_box");

  Expect(result.status == amflow::ParsedReductionStatus::ParsedRules,
         "generated-config rule-only should not shadow a complete direct tree");
  Expect(result.master_list.source_path == direct_family_root / "masters",
         "generated-config rule-only should leave the direct masters path selected");
  Expect(result.rule_path == direct_family_root / "kira_target.m",
         "generated-config rule-only should leave the direct rule path selected");
  Expect(result.explicit_rule_count == 2,
         "generated-config rule-only should preserve the complete direct explicit rules");
}

void KiraParsedResultsMissingRuleFallsBackToIdentityTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(root, "planar_double_box");

  Expect(result.status == amflow::ParsedReductionStatus::IdentityFallback,
         "missing kira_target.m should fall back to identity rules");
  Expect(result.explicit_rule_count == 0,
         "identity fallback should not report explicit reduction rules");
  Expect(result.rules.size() == result.master_list.masters.size(),
         "identity fallback should emit one rule per master");
  Expect(result.rules.front().target.Label() == result.master_list.masters.front().Label(),
         "identity fallback rules should target the parsed masters directly");
  Expect(result.rules.front().terms.size() == 1 &&
             result.rules.front().terms.front().coefficient == "1",
         "identity fallback rules should use unit coefficients");
}

void KiraParsedResultsRejectMalformedMastersTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/malformed-masters";
  ExpectRuntimeError(
      [&backend, &root]() {
        static_cast<void>(backend.ParseReductionResult(root, "planar_double_box"));
      },
      "invalid integral index",
      "malformed masters should fail deterministically during integral parsing");
}

void KiraParsedResultsRejectMalformedRulesTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/malformed-rules";
  ExpectRuntimeError(
      [&backend, &root]() {
        static_cast<void>(backend.ParseReductionResult(root, "planar_double_box"));
      },
      "reduction term must contain exactly one integral",
      "malformed rule expressions should fail deterministically");
}

void KiraParsedResultsRejectNonlinearRulesTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/nonlinear-rules";
  ExpectRuntimeError(
      [&backend, &root]() {
        static_cast<void>(backend.ParseReductionResult(root, "planar_double_box"));
      },
      "reduction term must be linear in masters",
      "nonlinear master occurrences should fail deterministically");
}

void KiraParsedResultsRejectInconsistentMastersTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/inconsistent-masters";
  ExpectRuntimeError(
      [&backend, &root]() {
        static_cast<void>(backend.ParseReductionResult(root, "planar_double_box"));
      },
      "reduction rule references unknown master integral",
      "rules that reference integrals outside the parsed master basis should fail");
}

void KiraParsedResultsCanonicalizeDuplicateTermsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/canonicalized-rules";
  const amflow::ParsedReductionResult result =
      backend.ParseReductionResult(root, "planar_double_box");
  const auto& summed_rule =
      FindRuleByTarget(result.rules, "planar_double_box[2,1,1,1,1,1,1]");

  Expect(result.status == amflow::ParsedReductionStatus::ParsedRules,
         "duplicate-master fixture should still produce explicit parsed rules");
  Expect(result.explicit_rule_count == 1,
         "zero-net explicit rules should be dropped after canonicalization");
  Expect(result.rules.size() == 3,
         "canonicalized explicit rules should still append identity rules");
  Expect(summed_rule.terms.size() == 1,
         "duplicate master terms should be collapsed to a single canonical term");
  Expect(summed_rule.terms[0].master.Label() == "planar_double_box[1,1,1,1,1,1,1]",
         "canonicalized duplicate terms should preserve the master label");
  Expect(summed_rule.terms[0].coefficient == "5",
         "numeric duplicate coefficients should be combined exactly");
}

void ReductionAssemblyHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[0],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[2,1,1,1,1,1,1]").target},
      {reduction_result.master_list.masters[1],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[1,1,1,1,1,2,1]").target},
  };
  input.reduction_result = reduction_result;

  const amflow::DESystem system =
      amflow::AssembleReducedDESystem(reduction_result.master_list, {input});
  const auto matrix_it = system.coefficient_matrices.find("eta");

  Expect(system.masters.size() == 2, "assembled DE system should preserve the master basis");
  Expect(system.variables.size() == 1, "assembled DE system should preserve the variable count");
  Expect(system.variables.front().name == "eta",
         "assembled DE system should preserve variable metadata");
  Expect(matrix_it != system.coefficient_matrices.end(),
         "assembled DE system should contain the eta coefficient matrix");
  Expect(matrix_it->second.size() == 2,
         "assembled DE system should emit one matrix row per master");
  Expect(matrix_it->second[0].size() == 2,
         "assembled DE system should emit one matrix column per master");
  Expect(matrix_it->second[0][0] ==
             FindTermByMaster(FindRuleByTarget(reduction_result.rules,
                                               "planar_double_box[2,1,1,1,1,1,1]"),
                              "planar_double_box[1,1,1,1,1,1,1]")
                 .coefficient,
         "assembled DE rows should preserve parsed coefficients for the first master");
  Expect(matrix_it->second[0][1] ==
             FindTermByMaster(FindRuleByTarget(reduction_result.rules,
                                               "planar_double_box[2,1,1,1,1,1,1]"),
                              "planar_double_box[1,1,1,1,1,1,0]")
                 .coefficient,
         "assembled DE rows should preserve parsed coefficients for the second master");
  Expect(matrix_it->second[1][0] ==
             FindTermByMaster(FindRuleByTarget(reduction_result.rules,
                                               "planar_double_box[1,1,1,1,1,2,1]"),
                              "planar_double_box[1,1,1,1,1,1,1]")
                 .coefficient,
         "assembled DE rows should preserve symbolic coefficients as-is");
  Expect(matrix_it->second[1][1] == "0",
         "assembled DE rows should fill absent coefficients with zero");
  Expect(amflow::ValidateDESystem(system).empty(),
         "assembled DE system should validate cleanly");
}

void ReductionAssemblyRejectsEmptyMasterBasisTest() {
  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;

  amflow::ParsedMasterList empty_basis;
  ExpectRuntimeError(
      [&empty_basis, &input]() {
        static_cast<void>(amflow::AssembleReducedDESystem(empty_basis, {input}));
      },
      "master basis must not be empty",
      "DE assembly should reject an empty master basis");
}

void ReductionAssemblyRejectsDerivativeTargetArityMismatchTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[0],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[2,1,1,1,1,1,1]").target},
  };
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(
            amflow::AssembleReducedDESystem(reduction_result.master_list, {input}));
      },
      "row binding count must match assembly master count",
      "DE assembly should reject derivative-row arity mismatches");
}

void ReductionAssemblyRejectsMissingReductionRuleTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[0],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[2,1,1,1,1,1,1]").target},
      {reduction_result.master_list.masters[1],
       amflow::TargetIntegral{"planar_double_box", {9, 9, 9, 9, 9, 9, 9}}},
  };
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(
            amflow::AssembleReducedDESystem(reduction_result.master_list, {input}));
      },
      "missing a reduction rule for derivative target",
      "DE assembly should reject derivative targets without explicit reduction rules");
}

void ReductionAssemblyRejectsDuplicateDerivativeTargetsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");
  const amflow::TargetIntegral duplicate_target =
      FindRuleByTarget(reduction_result.rules, "planar_double_box[2,1,1,1,1,1,1]").target;

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[0], duplicate_target},
      {reduction_result.master_list.masters[1], duplicate_target},
  };
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(
            amflow::AssembleReducedDESystem(reduction_result.master_list, {input}));
      },
      "contains duplicate derivative target",
      "DE assembly should reject duplicate derivative targets within one variable input");
}

void ReductionAssemblyRejectsPermutedRowBindingsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[1],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[2,1,1,1,1,1,1]").target},
      {reduction_result.master_list.masters[0],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[1,1,1,1,1,2,1]").target},
  };
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(
            amflow::AssembleReducedDESystem(reduction_result.master_list, {input}));
      },
      "source master does not match assembly master basis",
      "DE assembly should reject row bindings that are permuted relative to the basis");
}

void ReductionAssemblyIgnoresIdentityFallbackRulesForDerivativeLookupTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[0], reduction_result.master_list.masters[0]},
      {reduction_result.master_list.masters[1], reduction_result.master_list.masters[1]},
  };
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(
            amflow::AssembleReducedDESystem(reduction_result.master_list, {input}));
      },
      "missing a reduction rule for derivative target",
      "DE assembly should not satisfy derivative lookups from appended identity rules");
}

void ReductionAssemblyRejectsMasterBasisMismatchTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");

  amflow::ReducedDerivativeVariableInput input;
  input.variable.name = "eta";
  input.variable.kind = amflow::DifferentiationVariableKind::Eta;
  input.row_bindings = {
      {reduction_result.master_list.masters[0],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[2,1,1,1,1,1,1]").target},
      {reduction_result.master_list.masters[1],
       FindRuleByTarget(reduction_result.rules, "planar_double_box[1,1,1,1,1,2,1]").target},
  };
  input.reduction_result = reduction_result;
  std::reverse(input.reduction_result.master_list.masters.begin(),
               input.reduction_result.master_list.masters.end());

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(
            amflow::AssembleReducedDESystem(reduction_result.master_list, {input}));
      },
      "reduction master basis does not match assembly master basis",
      "DE assembly should reject variable inputs whose reduction basis order differs");
}

void ValidateDESystemRejectsMalformedCoefficientMatrixShapeTest() {
  amflow::DESystem system;
  system.masters = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 1}, "planar_double_box[1,1,1,1,1,1,1]"},
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 0}, "planar_double_box[1,1,1,1,1,1,0]"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
      {"eta", amflow::DifferentiationVariableKind::Invariant},
  };
  system.coefficient_matrices["eta"] = {{"1"}, {"0", "1"}};
  system.coefficient_matrices["s"] = {{"1", "0"}, {"0", "1"}};

  const auto messages = amflow::ValidateDESystem(system);
  Expect(ContainsSubstring(messages, "duplicate differentiation variable name: eta"),
         "DESystem validation should reject duplicate variable names");
  Expect(
      ContainsSubstring(messages, "coefficient matrix column count mismatch for variable: eta"),
      "DESystem validation should reject malformed coefficient matrix column shapes");
  Expect(ContainsSubstring(messages, "extra coefficient matrix for undeclared variable: s"),
         "DESystem validation should reject extra matrix keys");
}

void ValidateDESystemRejectsCoefficientMatrixRowMismatchTest() {
  amflow::DESystem system;
  system.masters = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 1}, "planar_double_box[1,1,1,1,1,1,1]"},
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 0}, "planar_double_box[1,1,1,1,1,1,0]"},
  };
  system.variables = {
      {"eta", amflow::DifferentiationVariableKind::Eta},
  };
  system.coefficient_matrices["eta"] = {{"1", "0"}};

  const auto messages = amflow::ValidateDESystem(system);
  Expect(ContainsSubstring(messages, "coefficient matrix row count mismatch for variable: eta"),
         "DESystem validation should reject coefficient matrix row-count mismatches");
}

void ValidateBoundaryRequestHappyPathTest() {
  const auto messages =
      amflow::ValidateBoundaryRequest(MakeBoundaryAttachmentBaselineDESystem(),
                                      MakeValidBoundaryRequest());
  Expect(messages.empty(), "boundary-request validation should accept the baseline happy path");
}

void ValidateBoundaryRequestRejectsEmptyVariableTest() {
  amflow::BoundaryRequest request = MakeValidBoundaryRequest();
  request.variable.clear();

  const auto messages =
      amflow::ValidateBoundaryRequest(MakeBoundaryAttachmentBaselineDESystem(), request);
  Expect(ContainsSubstring(messages, "boundary request variable must not be empty"),
         "boundary-request validation should reject empty variable names");
}

void ValidateBoundaryRequestRejectsEmptyLocationTest() {
  amflow::BoundaryRequest request = MakeValidBoundaryRequest();
  request.location.clear();

  const auto messages =
      amflow::ValidateBoundaryRequest(MakeBoundaryAttachmentBaselineDESystem(), request);
  Expect(ContainsSubstring(messages, "boundary request location must not be empty"),
         "boundary-request validation should reject empty locations");
}

void ValidateBoundaryRequestRejectsEmptyStrategyTest() {
  amflow::BoundaryRequest request = MakeValidBoundaryRequest();
  request.strategy.clear();

  const auto messages =
      amflow::ValidateBoundaryRequest(MakeBoundaryAttachmentBaselineDESystem(), request);
  Expect(ContainsSubstring(messages, "boundary request strategy must not be empty"),
         "boundary-request validation should reject empty strategies");
}

void ValidateBoundaryRequestRejectsUnknownVariableTest() {
  amflow::BoundaryRequest request = MakeValidBoundaryRequest();
  request.variable = "s";

  const auto messages =
      amflow::ValidateBoundaryRequest(MakeBoundaryAttachmentBaselineDESystem(), request);
  Expect(ContainsSubstring(messages, "boundary request variable is not declared in DE system: s"),
         "boundary-request validation should reject variables not declared on the DE system");
}

void ValidateBoundaryRequestPropagatesInvalidDESystemTest() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.variables.push_back({"eta", amflow::DifferentiationVariableKind::Invariant});

  const auto messages = amflow::ValidateBoundaryRequest(system, MakeValidBoundaryRequest());
  Expect(ContainsSubstring(messages, "duplicate differentiation variable name: eta"),
         "boundary-request validation should surface DE-system validation failures");
}

void DescribeDESystemOmitsBoundariesTest() {
  const std::string description =
      amflow::DescribeDESystem(MakeBoundaryAttachmentBaselineDESystem());
  Expect(description.find("boundaries=") == std::string::npos,
         "DESystem description should no longer report embedded boundaries");
}

void SampleBoundaryAttachmentSurfaceHappyPathTest() {
  const auto validation_messages =
      amflow::ValidateBoundaryRequest(amflow::MakeSampleDESystem(),
                                      amflow::MakeSampleBoundaryRequest());
  Expect(validation_messages.empty(),
         "sample boundary request should validate against the sample DE system");

  amflow::SolveRequest request;
  request.system = amflow::MakeSampleDESystem();
  request.boundary_requests = {amflow::MakeSampleBoundaryRequest()};
  request.start_location = "infinity";
  request.target_location = "eta=0";
  request.precision_policy = MakeDistinctPrecisionPolicy();
  request.requested_digits = 61;

  const amflow::BoundaryCondition expected_condition = amflow::MakeSampleBoundaryCondition();
  const amflow::SolveRequest attached =
      amflow::AttachManualBoundaryConditions(request, {expected_condition});

  Expect(attached.boundary_conditions.size() == 1 &&
             SameBoundaryCondition(attached.boundary_conditions.front(), expected_condition),
         "sample boundary helpers should attach explicit manual boundary data through the public "
         "solver boundary surface");
}

void AttachManualBoundaryConditionsHappyPathTest() {
  const amflow::SolveRequest request = MakeBoundarySolveRequest();
  const amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();

  const amflow::SolveRequest attached =
      amflow::AttachManualBoundaryConditions(request, {explicit_boundary});

  Expect(request.boundary_conditions.empty(),
         "manual boundary attachment should not mutate the original request");
  Expect(attached.boundary_conditions.size() == 1 &&
             SameBoundaryCondition(attached.boundary_conditions.front(), explicit_boundary),
         "manual boundary attachment should preserve the explicit boundary data");
  Expect(SameDESystem(attached.system, request.system),
         "manual boundary attachment should preserve the symbolic DE system");
  Expect(attached.start_location == request.start_location &&
             attached.target_location == request.target_location &&
             SamePrecisionPolicy(attached.precision_policy, request.precision_policy) &&
             attached.requested_digits == request.requested_digits,
         "manual boundary attachment should preserve the non-boundary solve request fields");
}

void AttachManualBoundaryConditionsPreservesExplicitOrderTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_requests.push_back({"eta", "eta=2", "manual"});

  amflow::BoundaryCondition first = MakeValidBoundaryCondition();
  amflow::BoundaryCondition second = {"eta", "eta=2", {"7/11", "13/17"}, "manual"};
  const std::vector<amflow::BoundaryCondition> explicit_boundaries = {second, first};

  const amflow::SolveRequest attached =
      amflow::AttachManualBoundaryConditions(request, explicit_boundaries);

  Expect(attached.boundary_conditions.size() == explicit_boundaries.size() &&
             SameBoundaryCondition(attached.boundary_conditions[0], explicit_boundaries[0]) &&
             SameBoundaryCondition(attached.boundary_conditions[1], explicit_boundaries[1]),
         "manual boundary attachment should preserve explicit boundary order");
}

void AttachManualBoundaryConditionsRejectsEmptyExplicitBoundaryListTest() {
  ExpectBoundaryUnsolved(
      []() {
        static_cast<void>(
            amflow::AttachManualBoundaryConditions(MakeBoundarySolveRequest(), {}));
      },
      "explicit boundary list must not be empty",
      "manual boundary attachment should reject missing explicit boundary data");
}

void AttachManualBoundaryConditionsRejectsUnknownRequestVariableTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_requests.front().variable = "s";

  ExpectBoundaryUnsolved(
      [&request]() {
        static_cast<void>(
            amflow::AttachManualBoundaryConditions(request, {MakeValidBoundaryCondition()}));
      },
      "boundary request variable is not declared in DE system: s",
      "manual boundary attachment should surface invalid request variables as boundary_unsolved");
}

void AttachManualBoundaryConditionsRejectsDuplicateExplicitBoundaryConditionTest() {
  const amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();

  ExpectBoundaryUnsolved(
      [&explicit_boundary]() {
        static_cast<void>(amflow::AttachManualBoundaryConditions(
            MakeBoundarySolveRequest(),
            {explicit_boundary, explicit_boundary}));
      },
      "duplicate explicit boundary condition",
      "manual boundary attachment should reject duplicate explicit boundary matches");
}

void AttachManualBoundaryConditionsRejectsPreexistingBoundaryConditionsTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_conditions = {MakeValidBoundaryCondition()};

  ExpectBoundaryUnsolved(
      [&request]() {
        static_cast<void>(
            amflow::AttachManualBoundaryConditions(request, {MakeValidBoundaryCondition()}));
      },
      "manual boundary conditions are already attached to this solve request",
      "manual boundary attachment should reject conflicting reattachment");
}

void AttachManualBoundaryConditionsRejectsBoundaryValueCountMismatchTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.values = {"1/3"};

  ExpectBoundaryUnsolved(
      [&explicit_boundary]() {
        static_cast<void>(amflow::AttachManualBoundaryConditions(MakeBoundarySolveRequest(),
                                                                 {explicit_boundary}));
      },
      "boundary value count does not match master basis",
      "manual boundary attachment should reject value-count mismatches");
}

void AttachManualBoundaryConditionsRejectsMissingStartLocationCoverageTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.location = "eta=3";

  ExpectBoundaryUnsolved(
      [&explicit_boundary]() {
        static_cast<void>(amflow::AttachManualBoundaryConditions(MakeBoundarySolveRequest(),
                                                                 {explicit_boundary}));
      },
      "no explicit boundary data matched the solver start location: eta=1",
      "manual boundary attachment should reject missing start-location coverage");
}

void AttachBoundaryConditionsFromProviderHappyPathTest() {
  const amflow::SolveRequest request = MakeBoundarySolveRequest();
  const amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  RecordingStaticBoundaryProvider provider("manual", {explicit_boundary});

  const amflow::SolveRequest attached =
      amflow::AttachBoundaryConditionsFromProvider(request, provider);

  amflow::SolveRequest expected = request;
  expected.boundary_conditions = {explicit_boundary};

  Expect(request.boundary_conditions.empty(),
         "provider boundary attachment should not mutate the original request");
  Expect(provider.strategy_call_count() == 1 && provider.provide_call_count() == 1,
         "provider boundary attachment should query the provider exactly once for one request");
  Expect(provider.seen_requests().size() == 1 &&
             SameBoundaryRequest(provider.seen_requests().front(),
                                request.boundary_requests.front()),
         "provider boundary attachment should forward the reviewed boundary request unchanged");
  Expect(SameSolveRequest(attached, expected),
         "provider boundary attachment should preserve all solve-request fields while adding "
         "the provider-produced boundary condition");
}

void AttachBoundaryConditionsFromProviderPreservesRequestOrderTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_requests.push_back({"eta", "eta=2", "manual"});

  const amflow::BoundaryCondition first = MakeValidBoundaryCondition();
  const amflow::BoundaryCondition second = {"eta", "eta=2", {"7/11", "13/17"}, "manual"};
  RecordingStaticBoundaryProvider provider("manual", {first, second});

  const amflow::SolveRequest attached =
      amflow::AttachBoundaryConditionsFromProvider(request, provider);

  amflow::SolveRequest expected = request;
  expected.boundary_conditions = {first, second};

  Expect(provider.strategy_call_count() == 1 && provider.provide_call_count() == 2,
         "provider boundary attachment should call Provide exactly once per request");
  Expect(provider.seen_requests().size() == 2 &&
             SameBoundaryRequest(provider.seen_requests()[0], request.boundary_requests[0]) &&
             SameBoundaryRequest(provider.seen_requests()[1], request.boundary_requests[1]),
         "provider boundary attachment should call the provider in boundary-request order");
  Expect(SameSolveRequest(attached, expected),
         "provider boundary attachment should preserve request order in the attached explicit "
         "boundary list");
}

void AttachBoundaryConditionsFromProviderRejectsInvalidRequestBeforeProviderCallTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_requests.front().variable = "s";
  RecordingStaticBoundaryProvider provider("manual", {MakeValidBoundaryCondition()});

  ExpectBoundaryUnsolved(
      [&request, &provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(request, provider));
      },
      "boundary request variable is not declared in DE system: s",
      "provider boundary attachment should reject invalid boundary requests before consulting "
      "the provider");
  Expect(provider.strategy_call_count() == 0 && provider.provide_call_count() == 0,
         "provider boundary attachment should not consult the provider when request validation "
         "fails");
}

void AttachBoundaryConditionsFromProviderRejectsStrategyMismatchTest() {
  const amflow::SolveRequest request = MakeBoundarySolveRequest();
  RecordingStaticBoundaryProvider provider("automatic", {MakeValidBoundaryCondition()});

  ExpectBoundaryUnsolved(
      [&request, &provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(request, provider));
      },
      "boundary provider strategy does not match request for eta @ eta=1",
      "provider boundary attachment should reject provider strategy mismatches as "
      "boundary_unsolved");
  Expect(provider.strategy_call_count() == 1 && provider.provide_call_count() == 0,
         "provider boundary attachment should reject strategy mismatches before Provide calls");
}

void AttachBoundaryConditionsFromProviderPropagatesProviderBoundaryUnsolvedTest() {
  const amflow::SolveRequest request = MakeBoundarySolveRequest();
  ThrowingBoundaryProvider provider("manual", "provider could not resolve eta @ eta=1");

  try {
    static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(request, provider));
  } catch (const amflow::BoundaryUnsolvedError& error) {
    Expect(std::string(error.what()) == "boundary_unsolved: provider could not resolve eta @ eta=1",
           "provider boundary attachment should propagate provider BoundaryUnsolvedError "
           "messages unchanged");
    Expect(std::string(error.failure_code()) == "boundary_unsolved",
           "provider boundary attachment should preserve the typed boundary_unsolved code");
    Expect(provider.strategy_call_count() == 1 && provider.provide_call_count() == 1,
           "provider boundary attachment should stop after the provider reports "
           "boundary_unsolved");
    return;
  }
  throw std::runtime_error(
      "provider boundary attachment should propagate provider BoundaryUnsolvedError unchanged");
}

void AttachBoundaryConditionsFromProviderRejectsWrongVariableOutputTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.variable = "s";
  RecordingStaticBoundaryProvider provider("manual", {explicit_boundary});

  ExpectBoundaryUnsolved(
      [&provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(MakeBoundarySolveRequest(),
                                                                       provider));
      },
      "no explicit boundary data matched request: eta @ eta=1",
      "provider boundary attachment should route wrong-variable provider output through the "
      "reviewed manual attachment validator");
}

void AttachBoundaryConditionsFromProviderRejectsWrongLocationOutputTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.location = "eta=3";
  RecordingStaticBoundaryProvider provider("manual", {explicit_boundary});

  ExpectBoundaryUnsolved(
      [&provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(MakeBoundarySolveRequest(),
                                                                       provider));
      },
      "no explicit boundary data matched the solver start location: eta=1",
      "provider boundary attachment should route wrong-location provider output through the "
      "reviewed manual attachment validator");
}

void AttachBoundaryConditionsFromProviderRejectsWrongStrategyOutputTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.strategy = "automatic";
  RecordingStaticBoundaryProvider provider("manual", {explicit_boundary});

  ExpectBoundaryUnsolved(
      [&provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(MakeBoundarySolveRequest(),
                                                                       provider));
      },
      "explicit boundary strategy does not match request for eta @ eta=1",
      "provider boundary attachment should route wrong-strategy provider output through the "
      "reviewed manual attachment validator");
}

void AttachBoundaryConditionsFromProviderRejectsEmptyValuesOutputTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.values.clear();
  RecordingStaticBoundaryProvider provider("manual", {explicit_boundary});

  ExpectBoundaryUnsolved(
      [&provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(MakeBoundarySolveRequest(),
                                                                       provider));
      },
      "boundary condition values must not be empty",
      "provider boundary attachment should route empty provider values through the reviewed "
      "manual attachment validator");
}

void AttachBoundaryConditionsFromProviderRejectsWrongValueCountOutputTest() {
  amflow::BoundaryCondition explicit_boundary = MakeValidBoundaryCondition();
  explicit_boundary.values = {"1/3"};
  RecordingStaticBoundaryProvider provider("manual", {explicit_boundary});

  ExpectBoundaryUnsolved(
      [&provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(MakeBoundarySolveRequest(),
                                                                       provider));
      },
      "boundary value count does not match master basis",
      "provider boundary attachment should route wrong value counts through the reviewed manual "
      "attachment validator");
}

void AttachBoundaryConditionsFromProviderRejectsDuplicateLociOutputTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_requests.push_back({"eta", "eta=2", "manual"});

  amflow::BoundaryCondition first = MakeValidBoundaryCondition();
  amflow::BoundaryCondition duplicate = MakeValidBoundaryCondition();
  duplicate.values = {"7/11", "13/17"};
  RecordingStaticBoundaryProvider provider("manual", {first, duplicate});

  ExpectBoundaryUnsolved(
      [&request, &provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(request, provider));
      },
      "duplicate explicit boundary condition: eta @ eta=1",
      "provider boundary attachment should route duplicate provider loci through the reviewed "
      "manual attachment validator");
}

void AttachBoundaryConditionsFromProviderRejectsPreexistingBoundaryConditionsTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_conditions = {MakeValidBoundaryCondition()};
  RecordingStaticBoundaryProvider provider("manual", {MakeValidBoundaryCondition()});

  ExpectBoundaryUnsolved(
      [&request, &provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(request, provider));
      },
      "manual boundary conditions are already attached to this solve request",
      "provider boundary attachment should reject conflicting reattachment");
  Expect(provider.strategy_call_count() == 0 && provider.provide_call_count() == 0,
         "provider boundary attachment should reject conflicting reattachment before "
         "consulting the provider");
}

void AttachBoundaryConditionsFromProviderIsDeterministicAndNonMutatingTest() {
  amflow::SolveRequest request = MakeBoundarySolveRequest();
  request.boundary_requests.push_back({"eta", "eta=2", "manual"});
  const amflow::SolveRequest original = request;

  const std::vector<amflow::BoundaryCondition> explicit_boundaries = {
      MakeValidBoundaryCondition(),
      {"eta", "eta=2", {"7/11", "13/17"}, "manual"},
  };
  RecordingStaticBoundaryProvider first_provider("manual", explicit_boundaries);
  RecordingStaticBoundaryProvider second_provider("manual", explicit_boundaries);

  const amflow::SolveRequest first =
      amflow::AttachBoundaryConditionsFromProvider(request, first_provider);
  const amflow::SolveRequest second =
      amflow::AttachBoundaryConditionsFromProvider(request, second_provider);

  amflow::SolveRequest expected = request;
  expected.boundary_conditions = explicit_boundaries;

  Expect(SameSolveRequest(request, original),
         "provider boundary attachment should not mutate the input request across repeated runs");
  Expect(SameSolveRequest(first, expected) && SameSolveRequest(second, expected),
         "provider boundary attachment should be deterministic across repeated runs");
}

void GenerateBuiltinEtaInfinityBoundaryRequestSampleSpecHappyPathTest() {
  const amflow::BoundaryRequest request =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(amflow::MakeSampleProblemSpec());

  const amflow::BoundaryRequest expected = {"eta", "infinity", "builtin::eta->infinity"};
  Expect(SameBoundaryRequest(request, expected),
         "the sample ProblemSpec should map to the reviewed builtin eta->infinity request");
}

void GeneratePlannedEtaInfinityBoundaryRequestBuiltinHappyPathTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::BoundaryRequest request =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});

  const amflow::BoundaryRequest expected = {"eta", "infinity", "builtin::eta->infinity"};
  Expect(SameBoundaryRequest(request, expected),
         "planned eta->infinity boundary generation should return the reviewed builtin request "
         "shape for the builtin Tradition happy path");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "planned eta->infinity boundary generation should not mutate the input ProblemSpec");
}

void GeneratePlannedEtaInfinityBoundaryRequestUserDefinedSingletonHappyPathTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "custom::should-not-be-used";
  custom_decision.terminal_nodes = {"planar_double_box::eta->infinity"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const amflow::BoundaryRequest request =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(amflow::MakeSampleProblemSpec(),
                                                        "CustomScheme",
                                                        {custom_scheme});

  const amflow::BoundaryRequest expected = {"eta", "infinity", "builtin::eta->infinity"};
  Expect(SameBoundaryRequest(request, expected),
         "planned eta->infinity boundary generation should accept a user-defined scheme only "
         "when it returns the exact supported singleton terminal node");
  Expect(custom_scheme->call_count() == 1,
         "planned eta->infinity boundary generation should plan the selected user-defined "
         "scheme exactly once");
}

void GeneratePlannedEtaInfinityBoundaryRequestBuiltinTrivialRejectsExtraNodeTest() {
  ExpectBoundaryUnsolved(
      []() {
        static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
            amflow::MakeSampleProblemSpec(),
            "Trivial",
            {}));
      },
      "unsupported extra terminal node planar_double_box::trivial-region",
      "planned eta->infinity boundary generation should currently reject builtin Trivial "
      "because it adds an unsupported extra terminal node");
}

void GeneratePlannedEtaInfinityBoundaryRequestRejectsUserDefinedExtraNodeTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"planar_double_box::eta->infinity", "custom::region"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectBoundaryUnsolved(
      [&custom_scheme]() {
        static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
            amflow::MakeSampleProblemSpec(),
            "CustomScheme",
            {custom_scheme}));
      },
      "unsupported extra terminal node custom::region",
      "planned eta->infinity boundary generation should reject any extra terminal node beyond "
      "the exact supported singleton");
  Expect(custom_scheme->call_count() == 1,
         "planned eta->infinity boundary generation should plan the rejected user-defined "
         "scheme exactly once");
}

void GeneratePlannedEtaInfinityBoundaryRequestRejectsWrongFamilyInfinityNodeTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"custom::eta->infinity"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectBoundaryUnsolved(
      [&custom_scheme]() {
        static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
            amflow::MakeSampleProblemSpec(),
            "CustomScheme",
            {custom_scheme}));
      },
      "unsupported extra terminal node custom::eta->infinity",
      "planned eta->infinity boundary generation should require the exact family-qualified "
      "supported terminal node and should not suffix-match eta->infinity names");
  Expect(custom_scheme->call_count() == 1,
         "planned eta->infinity boundary generation should plan a wrong-family singleton "
         "scheme exactly once before rejecting it");
}

void GeneratePlannedEtaInfinityBoundaryRequestRejectsDuplicateSupportedNodeTest() {
  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"planar_double_box::eta->infinity",
                                    "planar_double_box::eta->infinity"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  ExpectBoundaryUnsolved(
      [&custom_scheme]() {
        static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
            amflow::MakeSampleProblemSpec(),
            "CustomScheme",
            {custom_scheme}));
      },
      "duplicate supported terminal node",
      "planned eta->infinity boundary generation should reject duplicate supported terminal "
      "nodes");
  Expect(custom_scheme->call_count() == 1,
         "planned eta->infinity boundary generation should plan a duplicate-supported-node "
         "scheme exactly once before rejecting it");
}

void GeneratePlannedEtaInfinityBoundaryRequestPreservesUnknownEndingDiagnosticTest() {
  std::string baseline_message;
  try {
    static_cast<void>(
        amflow::PlanEndingScheme(amflow::MakeSampleProblemSpec(), "MissingScheme", {}));
  } catch (const std::invalid_argument& error) {
    baseline_message = error.what();
  }
  Expect(!baseline_message.empty(),
         "direct ending planning should reject an unknown ending name for the wrapper baseline");

  try {
    static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
        amflow::MakeSampleProblemSpec(),
        "MissingScheme",
        {}));
  } catch (const std::invalid_argument& error) {
    Expect(std::string(error.what()) == baseline_message,
           "planned eta->infinity boundary generation should preserve unknown ending "
           "diagnostics unchanged");
    return;
  }
  throw std::runtime_error(
      "planned eta->infinity boundary generation should preserve the unknown ending "
      "diagnostic as an ordinary invalid_argument");
}

void GeneratePlannedEtaInfinityBoundaryRequestPreservesPlanningFailureDiagnosticTest() {
  amflow::EndingDecision unused_decision;
  unused_decision.terminal_strategy = "RetryScheme";
  unused_decision.terminal_nodes = {"planar_double_box::eta->infinity"};
  const auto direct_scheme = std::make_shared<RecordingEndingScheme>(
      unused_decision, "RetryScheme", "retry ending planning failed");
  const auto wrapper_scheme = std::make_shared<RecordingEndingScheme>(
      unused_decision, "RetryScheme", "retry ending planning failed");

  std::string baseline_message;
  try {
    static_cast<void>(amflow::PlanEndingScheme(amflow::MakeSampleProblemSpec(),
                                               "RetryScheme",
                                               {direct_scheme}));
  } catch (const std::runtime_error& error) {
    baseline_message = error.what();
  }
  Expect(!baseline_message.empty(),
         "direct ending planning should fail for the wrapper planning-failure baseline");

  try {
    static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
        amflow::MakeSampleProblemSpec(),
        "RetryScheme",
        {wrapper_scheme}));
  } catch (const std::runtime_error& error) {
    Expect(std::string(error.what()) == baseline_message,
           "planned eta->infinity boundary generation should preserve ending planning "
           "failures unchanged");
    Expect(wrapper_scheme->call_count() == 1,
           "planned eta->infinity boundary generation should call the failing scheme exactly "
           "once before preserving its diagnostic");
    return;
  }
  throw std::runtime_error(
      "planned eta->infinity boundary generation should preserve planning failures as "
      "ordinary runtime_error diagnostics");
}

void GeneratePlannedEtaInfinityBoundaryRequestPropagatesBatch45SubsetDiagnosticTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators.front().mass = "msq";

  std::string baseline_message;
  try {
    static_cast<void>(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec));
  } catch (const amflow::BoundaryUnsolvedError& error) {
    baseline_message = error.what();
  }
  Expect(!baseline_message.empty(),
         "direct builtin eta->infinity boundary generation should fail for the wrapper "
         "subset-diagnostic baseline");

  try {
    static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {}));
  } catch (const amflow::BoundaryUnsolvedError& error) {
    Expect(std::string(error.what()) == baseline_message,
           "planned eta->infinity boundary generation should propagate Batch 45 subset "
           "diagnostics unchanged after a supported ending decision");
    return;
  }
  throw std::runtime_error(
      "planned eta->infinity boundary generation should surface unsupported Batch 45 "
      "subset specs as boundary_unsolved");
}

void GeneratePlannedEtaInfinityBoundaryRequestPreservesEmptyEtaSymbolDiagnosticTest() {
  std::string baseline_message;
  try {
    static_cast<void>(
        amflow::GenerateBuiltinEtaInfinityBoundaryRequest(amflow::MakeSampleProblemSpec(), ""));
  } catch (const std::invalid_argument& error) {
    baseline_message = error.what();
  }
  Expect(!baseline_message.empty(),
         "direct builtin eta->infinity boundary generation should reject an empty eta_symbol "
         "for the wrapper baseline");

  try {
    static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
        amflow::MakeSampleProblemSpec(),
        "Tradition",
        {},
        ""));
  } catch (const std::invalid_argument& error) {
    Expect(std::string(error.what()) == baseline_message,
           "planned eta->infinity boundary generation should preserve the Batch 45 empty "
           "eta_symbol diagnostic unchanged");
    return;
  }
  throw std::runtime_error(
      "planned eta->infinity boundary generation should preserve empty eta_symbol rejection "
      "as an ordinary invalid_argument");
}

void GenerateBuiltinEtaInfinityBoundaryRequestSupportsEtaSymbolOverrideTest() {
  const amflow::BoundaryRequest request = amflow::GenerateBuiltinEtaInfinityBoundaryRequest(
      amflow::MakeSampleProblemSpec(),
      "eta_aux");

  const amflow::BoundaryRequest expected = {"eta_aux", "infinity", "builtin::eta->infinity"};
  Expect(SameBoundaryRequest(request, expected),
         "builtin eta->infinity boundary generation should honor an explicit eta symbol "
         "override");
}

void GenerateBuiltinEtaInfinityBoundaryRequestIgnoresNonBoundaryProblemSpecFieldsTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest expected =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);

  amflow::ProblemSpec numeric_variant = spec;
  numeric_variant.kinematics.numeric_substitutions = {
      {"s", "101"},
      {"t", "-7"},
      {"msq", "13"},
  };

  amflow::ProblemSpec notes_variant = spec;
  notes_variant.notes = "Changed notes should not affect builtin boundary requests.";

  amflow::ProblemSpec complex_mode_variant = spec;
  complex_mode_variant.complex_mode = true;

  Expect(SameBoundaryRequest(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(numeric_variant),
                             expected),
         "numeric substitutions should not affect builtin eta->infinity boundary requests on "
         "the supported subset");
  Expect(SameBoundaryRequest(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(notes_variant),
                             expected),
         "notes should not affect builtin eta->infinity boundary requests on the supported "
         "subset");
  Expect(SameBoundaryRequest(
             amflow::GenerateBuiltinEtaInfinityBoundaryRequest(complex_mode_variant),
             expected),
         "complex_mode should not affect builtin eta->infinity boundary requests on the "
         "supported subset");
}

void GenerateBuiltinEtaInfinityBoundaryRequestDoesNotMutateProblemSpecTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::BoundaryRequest request =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);

  Expect(request.strategy == "builtin::eta->infinity",
         "builtin eta->infinity boundary generation should still succeed on the sample spec");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta->infinity boundary generation should not mutate the input ProblemSpec");
}

void GenerateBuiltinEtaInfinityBoundaryRequestRejectsMalformedProblemSpecTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.targets.clear();

  ExpectInvalidArgument(
      [&spec]() {
        static_cast<void>(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec));
      },
      "targets must not be empty",
      "malformed ProblemSpec inputs should fail as ordinary invalid_argument validation errors");
}

void GenerateBuiltinEtaInfinityBoundaryRequestValidatesProblemSpecBeforeEtaSymbolTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.targets.clear();

  ExpectInvalidArgument(
      [&spec]() {
        static_cast<void>(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec, ""));
      },
      "targets must not be empty",
      "builtin eta->infinity boundary generation should validate ProblemSpec before checking "
      "eta_symbol");
}

void GenerateBuiltinEtaInfinityBoundaryRequestRejectsEmptyEtaSymbolTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(
            amflow::GenerateBuiltinEtaInfinityBoundaryRequest(amflow::MakeSampleProblemSpec(),
                                                              ""));
      },
      "eta_symbol must not be empty",
      "builtin eta->infinity boundary generation should reject empty eta_symbol values");
}

void GenerateBuiltinEtaInfinityBoundaryRequestRejectsNonStandardPropagatorsTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators.front().kind = amflow::PropagatorKind::Cut;

  ExpectBoundaryUnsolved(
      [&spec]() {
        static_cast<void>(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec));
      },
      "only supports standard propagators",
      "well-formed non-Standard propagators should fail as boundary_unsolved");
}

void GenerateBuiltinEtaInfinityBoundaryRequestRejectsNonZeroMassTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators.front().mass = "msq";

  ExpectBoundaryUnsolved(
      [&spec]() {
        static_cast<void>(amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec));
      },
      "mass exactly \"0\"",
      "well-formed nonzero propagator masses should fail as boundary_unsolved");
}

void GenerateBuiltinEtaInfinityBoundaryRequestAttachesThroughProviderSeamTest() {
  const amflow::BoundaryRequest generated =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(amflow::MakeSampleProblemSpec());

  amflow::SolveRequest request;
  request.system = amflow::MakeSampleDESystem();
  request.boundary_requests = {generated};
  request.start_location = "infinity";
  request.target_location = "eta=0";
  request.precision_policy = MakeDistinctPrecisionPolicy();
  request.requested_digits = 61;

  const amflow::BoundaryCondition explicit_boundary = {
      "eta",
      "infinity",
      {"B1", "B2"},
      "builtin::eta->infinity",
  };
  RecordingStaticBoundaryProvider provider("builtin::eta->infinity", {explicit_boundary});

  const amflow::SolveRequest attached =
      amflow::AttachBoundaryConditionsFromProvider(request, provider);

  Expect(provider.strategy_call_count() == 1 && provider.provide_call_count() == 1,
         "builtin eta->infinity request integration should call the provider once");
  Expect(provider.seen_requests().size() == 1 &&
             SameBoundaryRequest(provider.seen_requests().front(), generated),
         "builtin eta->infinity request integration should feed the generated request through "
         "the reviewed provider seam unchanged");
  Expect(attached.boundary_conditions.size() == 1 &&
             SameBoundaryCondition(attached.boundary_conditions.front(), explicit_boundary),
         "builtin eta->infinity request integration should attach the provider-returned "
         "boundary condition unchanged");
}

void Batch47BuiltinTraditionEtaInfinityBoundaryEquivalenceTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_spec_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::BoundaryRequest manual_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const std::vector<amflow::BoundaryCondition> explicit_boundaries = {
      MakeEtaInfinityBoundaryCondition()};
  const std::vector<amflow::BoundaryCondition> original_explicit_boundaries = explicit_boundaries;
  amflow::SolveRequest manual_input = MakeEtaInfinitySolveRequest(manual_request_boundary);
  const amflow::SolveRequest original_manual_input = manual_input;
  const amflow::SolveRequest manual_attached =
      amflow::AttachManualBoundaryConditions(manual_input, explicit_boundaries);

  const amflow::BoundaryRequest automatic_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::SolveRequest automatic_input = MakeEtaInfinitySolveRequest(automatic_request_boundary);
  const amflow::SolveRequest original_automatic_input = automatic_input;
  RecordingStaticBoundaryProvider provider("builtin::eta->infinity", explicit_boundaries);
  const amflow::SolveRequest automatic_attached =
      amflow::AttachBoundaryConditionsFromProvider(automatic_input, provider);
  const amflow::SolveRequest original_manual_attached = manual_attached;
  const amflow::SolveRequest original_automatic_attached = automatic_attached;

  Expect(SameBoundaryRequest(automatic_request_boundary, manual_request_boundary),
         "Batch 47 builtin Tradition equivalence should preserve the exact reviewed "
         "eta->infinity boundary request shape");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_spec_yaml,
         "Batch 47 builtin Tradition equivalence should not mutate the shared input problem "
         "spec");
  Expect(SameSolveRequest(manual_input, original_manual_input) &&
             SameSolveRequest(automatic_input, original_automatic_input),
         "Batch 47 builtin Tradition equivalence should not mutate the caller-owned "
         "pre-attachment solve requests");
  Expect(explicit_boundaries.size() == original_explicit_boundaries.size() &&
             SameBoundaryCondition(explicit_boundaries.front(),
                                   original_explicit_boundaries.front()),
         "Batch 47 builtin Tradition equivalence should not mutate the caller-owned explicit "
         "boundary payload");
  Expect(provider.strategy_call_count() == 1 && provider.provide_call_count() == 1,
         "Batch 47 builtin Tradition equivalence should consult the provider exactly once");
  Expect(provider.seen_requests().size() == 1 &&
             SameBoundaryRequest(provider.seen_requests().front(), automatic_request_boundary),
         "Batch 47 builtin Tradition equivalence should feed the planned request through the "
         "provider seam unchanged");
  Expect(SameSolveRequest(automatic_attached, manual_attached),
         "Batch 47 builtin Tradition equivalence should attach the same SolveRequest through "
         "manual and automatic eta->infinity workflows");

  RecordingSeriesSolver manual_solver;
  RecordingSeriesSolver automatic_solver;
  manual_solver.use_request_driven_diagnostics = true;
  automatic_solver.use_request_driven_diagnostics = true;
  const amflow::SolverDiagnostics manual_diagnostics = manual_solver.Solve(manual_attached);
  const amflow::SolverDiagnostics automatic_diagnostics =
      automatic_solver.Solve(automatic_attached);

  Expect(manual_solver.call_count() == 1 && automatic_solver.call_count() == 1,
         "Batch 47 builtin Tradition equivalence should call the deterministic solver exactly "
         "once per successful lane");
  Expect(SameSolveRequest(manual_attached, original_manual_attached) &&
             SameSolveRequest(automatic_attached, original_automatic_attached),
         "Batch 47 builtin Tradition equivalence should not mutate the attached solve requests "
         "after solver execution");
  Expect(SameSolveRequest(automatic_solver.last_request(), manual_solver.last_request()),
         "Batch 47 builtin Tradition equivalence should feed the same attached request into the "
         "same deterministic solver behavior");
  Expect(SameSolveRequest(manual_solver.last_request(), original_manual_attached) &&
             SameSolveRequest(automatic_solver.last_request(), original_automatic_attached),
         "Batch 47 builtin Tradition equivalence should forward the attached request to the "
         "solver unchanged");
  Expect(SameSolverDiagnostics(automatic_diagnostics, manual_diagnostics),
         "Batch 47 builtin Tradition equivalence should preserve downstream solver diagnostics");
}

void Batch47UserDefinedEtaInfinitySingletonEquivalenceTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_spec_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::BoundaryRequest manual_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const std::vector<amflow::BoundaryCondition> explicit_boundaries = {
      MakeEtaInfinityBoundaryCondition()};
  const std::vector<amflow::BoundaryCondition> original_explicit_boundaries = explicit_boundaries;
  amflow::SolveRequest manual_input = MakeEtaInfinitySolveRequest(manual_request_boundary);
  const amflow::SolveRequest original_manual_input = manual_input;
  const amflow::SolveRequest manual_attached =
      amflow::AttachManualBoundaryConditions(manual_input, explicit_boundaries);

  amflow::EndingDecision custom_decision;
  custom_decision.terminal_strategy = "CustomScheme";
  custom_decision.terminal_nodes = {"planar_double_box::eta->infinity"};
  const auto custom_scheme =
      std::make_shared<RecordingEndingScheme>(custom_decision, "CustomScheme");

  const amflow::BoundaryRequest automatic_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "CustomScheme", {custom_scheme});
  amflow::SolveRequest automatic_input = MakeEtaInfinitySolveRequest(automatic_request_boundary);
  const amflow::SolveRequest original_automatic_input = automatic_input;
  RecordingStaticBoundaryProvider provider("builtin::eta->infinity", explicit_boundaries);
  const amflow::SolveRequest automatic_attached =
      amflow::AttachBoundaryConditionsFromProvider(automatic_input, provider);
  const amflow::SolveRequest original_manual_attached = manual_attached;
  const amflow::SolveRequest original_automatic_attached = automatic_attached;

  Expect(custom_scheme->call_count() == 1,
         "Batch 47 user-defined singleton equivalence should plan the exact custom ending once");
  Expect(custom_scheme->last_planned_spec_yaml() == original_spec_yaml &&
             amflow::SerializeProblemSpecYaml(spec) == original_spec_yaml,
         "Batch 47 user-defined singleton equivalence should not mutate the shared input "
         "problem spec while planning");
  Expect(SameBoundaryRequest(automatic_request_boundary, manual_request_boundary),
         "Batch 47 user-defined singleton equivalence should preserve the exact reviewed "
         "eta->infinity boundary request shape");
  Expect(SameSolveRequest(manual_input, original_manual_input) &&
             SameSolveRequest(automatic_input, original_automatic_input),
         "Batch 47 user-defined singleton equivalence should not mutate the caller-owned "
         "pre-attachment solve requests");
  Expect(explicit_boundaries.size() == original_explicit_boundaries.size() &&
             SameBoundaryCondition(explicit_boundaries.front(),
                                   original_explicit_boundaries.front()),
         "Batch 47 user-defined singleton equivalence should not mutate the caller-owned "
         "explicit boundary payload");
  Expect(SameSolveRequest(automatic_attached, manual_attached),
         "Batch 47 user-defined singleton equivalence should attach the same SolveRequest "
         "through manual and automatic eta->infinity workflows");

  RecordingSeriesSolver manual_solver;
  RecordingSeriesSolver automatic_solver;
  manual_solver.use_request_driven_diagnostics = true;
  automatic_solver.use_request_driven_diagnostics = true;
  const amflow::SolverDiagnostics manual_diagnostics = manual_solver.Solve(manual_attached);
  const amflow::SolverDiagnostics automatic_diagnostics =
      automatic_solver.Solve(automatic_attached);

  Expect(manual_solver.call_count() == 1 && automatic_solver.call_count() == 1,
         "Batch 47 user-defined singleton equivalence should call the deterministic solver "
         "exactly once per successful lane");
  Expect(SameSolveRequest(manual_attached, original_manual_attached) &&
             SameSolveRequest(automatic_attached, original_automatic_attached),
         "Batch 47 user-defined singleton equivalence should not mutate the attached solve "
         "requests after solver execution");
  Expect(SameSolveRequest(automatic_solver.last_request(), manual_solver.last_request()),
         "Batch 47 user-defined singleton equivalence should feed the same attached request "
         "into the same deterministic solver behavior");
  Expect(SameSolveRequest(manual_solver.last_request(), original_manual_attached) &&
             SameSolveRequest(automatic_solver.last_request(), original_automatic_attached),
         "Batch 47 user-defined singleton equivalence should forward the attached request to "
         "the solver unchanged");
  Expect(SameSolverDiagnostics(automatic_diagnostics, manual_diagnostics),
         "Batch 47 user-defined singleton equivalence should preserve downstream solver "
         "diagnostics");
}

void Batch47UnsupportedTerminalNodePreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&spec]() {
        static_cast<void>(
            amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Trivial", {}));
      },
      "direct planned eta->infinity boundary generation should reject unsupported terminal "
      "nodes for the Batch 47 baseline");
  RecordingStaticBoundaryProvider provider("builtin::eta->infinity",
                                           {MakeEtaInfinityBoundaryCondition()});
  RecordingSeriesSolver solver;

  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&spec, &provider, &solver]() {
        const amflow::BoundaryRequest request =
            amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Trivial", {});
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(request), provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve unsupported terminal "
      "node rejection");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve unsupported "
         "terminal-node diagnostics unchanged");
  Expect(provider.strategy_call_count() == 0 && provider.provide_call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not consult the provider "
         "after unsupported terminal-node rejection");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "unsupported terminal-node rejection");
}

void Batch47PlanningFailurePreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::EndingDecision unused_decision;
  unused_decision.terminal_strategy = "RetryScheme";
  unused_decision.terminal_nodes = {"planar_double_box::eta->infinity"};
  const auto direct_scheme = std::make_shared<RecordingEndingScheme>(
      unused_decision, "RetryScheme", "retry ending planning failed");
  const auto composed_scheme = std::make_shared<RecordingEndingScheme>(
      unused_decision, "RetryScheme", "retry ending planning failed");
  RecordingStaticBoundaryProvider provider("builtin::eta->infinity",
                                           {MakeEtaInfinityBoundaryCondition()});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureRuntimeErrorMessage(
      [&spec, &direct_scheme]() {
        static_cast<void>(amflow::GeneratePlannedEtaInfinityBoundaryRequest(
            spec, "RetryScheme", {direct_scheme}));
      },
      "direct planned eta->infinity boundary generation should fail for the Batch 47 "
      "planning-failure baseline");
  const std::string composed_message = CaptureRuntimeErrorMessage(
      [&spec, &composed_scheme, &provider, &solver]() {
        const amflow::BoundaryRequest request =
            amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "RetryScheme", {composed_scheme});
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(request), provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve planning failures");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve planning-failure "
         "diagnostics unchanged");
  Expect(composed_scheme->call_count() == 1 &&
             provider.strategy_call_count() == 0 && provider.provide_call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should stop before provider use "
         "when planning fails");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver when "
         "planning fails");
}

void Batch47UnsupportedBatch45SubsetPreservesDiagnosticTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  spec.family.propagators.front().mass = "msq";
  RecordingStaticBoundaryProvider provider("builtin::eta->infinity",
                                           {MakeEtaInfinityBoundaryCondition()});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&spec]() {
        static_cast<void>(
            amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {}));
      },
      "direct planned eta->infinity boundary generation should reject unsupported Batch 45 "
      "subset specs for the Batch 47 baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&spec, &provider, &solver]() {
        const amflow::BoundaryRequest request =
            amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(request), provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve Batch 45 subset "
      "rejection");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve unsupported Batch "
         "45 subset diagnostics unchanged");
  Expect(provider.strategy_call_count() == 0 && provider.provide_call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not consult the provider "
         "after Batch 45 subset rejection");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "unsupported Batch 45 subset rejection");
}

void Batch47ProviderStrategyMismatchPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  RecordingStaticBoundaryProvider direct_provider("manual", {MakeEtaInfinityBoundaryCondition()});
  RecordingStaticBoundaryProvider composed_provider("manual", {MakeEtaInfinityBoundaryCondition()});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should reject strategy mismatch for the Batch 47 baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve strategy mismatch "
      "rejection");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve provider strategy "
         "mismatch diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "provider strategy mismatch");
}

void Batch47ProviderBoundaryUnsolvedPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  ThrowingBoundaryProvider direct_provider("builtin::eta->infinity",
                                           "provider could not resolve eta @ infinity");
  ThrowingBoundaryProvider composed_provider("builtin::eta->infinity",
                                             "provider could not resolve eta @ infinity");
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should propagate provider boundary_unsolved for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve provider "
      "boundary_unsolved diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve provider "
         "boundary_unsolved diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "provider boundary_unsolved");
}

void Batch47ProviderWrongVariableOutputPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::BoundaryCondition malformed_boundary = MakeEtaInfinityBoundaryCondition();
  malformed_boundary.variable = "s";
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should reject wrong-variable provider output for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve wrong-variable "
      "provider output diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve wrong-variable "
         "output diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "wrong-variable provider output");
}

void Batch47ProviderWrongLocationOutputPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::BoundaryCondition malformed_boundary = MakeEtaInfinityBoundaryCondition();
  malformed_boundary.location = "eta=3";
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should reject wrong-location provider output for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve wrong-location "
      "provider output diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve wrong-location "
         "provider output diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "wrong-location provider output");
}

void Batch47ProviderWrongStrategyOutputPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::BoundaryCondition malformed_boundary = MakeEtaInfinityBoundaryCondition();
  malformed_boundary.strategy = "manual";
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should reject wrong-strategy provider output for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve wrong-strategy "
      "provider output diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve wrong-strategy "
         "provider output diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "wrong-strategy provider output");
}

void Batch47ProviderEmptyValuesOutputPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::BoundaryCondition malformed_boundary = MakeEtaInfinityBoundaryCondition();
  malformed_boundary.values.clear();
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should reject empty-values provider output for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve empty-values "
      "provider output diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve empty-values "
         "provider output diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "empty-values provider output");
}

void Batch47ProviderWrongValueCountOutputPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::BoundaryCondition malformed_boundary = MakeEtaInfinityBoundaryCondition();
  malformed_boundary.values = {"B1"};
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity", {malformed_boundary});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request_boundary, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(direct_request_boundary), direct_provider));
      },
      "direct provider attachment should reject wrong-value-count provider output for the Batch "
      "47 baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request_boundary, &composed_provider, &solver]() {
        const amflow::SolveRequest attached = amflow::AttachBoundaryConditionsFromProvider(
            MakeEtaInfinitySolveRequest(planned_request_boundary), composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve wrong-value-count "
      "provider output diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve wrong-value-count "
         "provider output diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "wrong-value-count provider output");
}

void Batch47ProviderDuplicateLociOutputPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::SolveRequest direct_request = MakeEtaInfinitySolveRequest(direct_request_boundary);
  amflow::SolveRequest planned_request = MakeEtaInfinitySolveRequest(planned_request_boundary);
  direct_request.boundary_requests.push_back(direct_request_boundary);
  planned_request.boundary_requests.push_back(planned_request_boundary);
  amflow::BoundaryCondition first = MakeEtaInfinityBoundaryCondition();
  amflow::BoundaryCondition duplicate = MakeEtaInfinityBoundaryCondition();
  duplicate.values = {"7/11", "13/17"};
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity",
                                                  {first, duplicate});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity",
                                                    {first, duplicate});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(direct_request,
                                                                       direct_provider));
      },
      "direct provider attachment should reject duplicate-loci provider output for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request, &composed_provider, &solver]() {
        const amflow::SolveRequest attached =
            amflow::AttachBoundaryConditionsFromProvider(planned_request, composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve duplicate-loci "
      "provider output diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve duplicate-loci "
         "provider output diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "duplicate-loci provider output");
}

void Batch47ConflictingReattachmentPreservesDiagnosticTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::BoundaryRequest direct_request_boundary =
      amflow::GenerateBuiltinEtaInfinityBoundaryRequest(spec);
  const amflow::BoundaryRequest planned_request_boundary =
      amflow::GeneratePlannedEtaInfinityBoundaryRequest(spec, "Tradition", {});
  amflow::SolveRequest direct_request = MakeEtaInfinitySolveRequest(direct_request_boundary);
  amflow::SolveRequest planned_request = MakeEtaInfinitySolveRequest(planned_request_boundary);
  direct_request.boundary_conditions = {MakeEtaInfinityBoundaryCondition()};
  planned_request.boundary_conditions = {MakeEtaInfinityBoundaryCondition()};
  RecordingStaticBoundaryProvider direct_provider("builtin::eta->infinity",
                                                  {MakeEtaInfinityBoundaryCondition()});
  RecordingStaticBoundaryProvider composed_provider("builtin::eta->infinity",
                                                    {MakeEtaInfinityBoundaryCondition()});
  RecordingSeriesSolver solver;

  const std::string baseline_message = CaptureBoundaryUnsolvedMessage(
      [&direct_request, &direct_provider]() {
        static_cast<void>(amflow::AttachBoundaryConditionsFromProvider(direct_request,
                                                                       direct_provider));
      },
      "direct provider attachment should reject conflicting reattachment for the Batch 47 "
      "baseline");
  const std::string composed_message = CaptureBoundaryUnsolvedMessage(
      [&planned_request, &composed_provider, &solver]() {
        const amflow::SolveRequest attached =
            amflow::AttachBoundaryConditionsFromProvider(planned_request, composed_provider);
        static_cast<void>(solver.Solve(attached));
      },
      "Batch 47 composed automatic eta->infinity harness should preserve conflicting "
      "reattachment diagnostics");

  Expect(composed_message == baseline_message,
         "Batch 47 composed automatic eta->infinity harness should preserve conflicting "
         "reattachment diagnostics unchanged");
  Expect(solver.call_count() == 0,
         "Batch 47 composed automatic eta->infinity harness should not call the solver after "
         "conflicting reattachment");
}

void BootstrapSeriesSolverReturnsBoundaryUnsolvedForIncompleteManualAttachmentTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics diagnostics = solver.Solve(MakeBoundarySolveRequest());

  Expect(!diagnostics.success, "bootstrap solver should reject unsatisfied boundary requests");
  Expect(diagnostics.failure_code == "boundary_unsolved",
         "bootstrap solver should surface typed boundary_unsolved failures for incomplete "
         "manual boundary attachment");
  Expect(diagnostics.summary.find("explicit boundary list must not be empty") !=
             std::string::npos,
         "bootstrap solver should preserve the manual boundary diagnostic");
}

void BootstrapSeriesSolverReturnsBoundaryUnsolvedWithoutExplicitStartBoundaryTest() {
  amflow::BootstrapSeriesSolver solver;
  amflow::SolveRequest request;
  request.system = MakeScalarRegularPointSeriesSystem("1/(eta+1)");
  request.start_location = "eta=0";
  request.target_location = "eta=2";

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject missing explicit start-boundary attachment");
  Expect(diagnostics.failure_code == "boundary_unsolved",
         "bootstrap solver should report typed boundary_unsolved when no explicit start "
         "boundary is attached");
  Expect(diagnostics.summary.find("explicit start boundary attachment is required") !=
             std::string::npos,
         "bootstrap solver should report the missing explicit start-boundary diagnostic");
}

void BootstrapSeriesSolverExactScalarOneHopHappyPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"7/11"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(diagnostics.success,
         "bootstrap solver should accept the reviewed exact scalar one-hop continuation path");
  Expect(diagnostics.residual_norm == 0.0,
         "bootstrap solver should report zero residual norm on the reviewed exact scalar path");
  Expect(diagnostics.overlap_mismatch == 0.0,
         "bootstrap solver should report zero overlap mismatch on the reviewed exact scalar "
         "path");
  Expect(diagnostics.failure_code.empty(),
         "bootstrap solver should leave failure_code empty on exact scalar success");
  Expect(diagnostics.summary == "Solved by exact one-hop regular-point continuation.",
         "bootstrap solver should expose the reviewed short exact-success summary");
}

void BootstrapSeriesSolverExactUpperTriangularOneHopHappyPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeMatrixRegularPointSeriesSystem({{"0", "1"}, {"0", "0"}}),
      "eta",
      "eta=0",
      "eta=1",
      {"2/3", "5/7"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(diagnostics.success,
         "bootstrap solver should accept the reviewed exact upper-triangular one-hop path");
  Expect(diagnostics.residual_norm == 0.0,
         "bootstrap solver should report zero residual norm on the reviewed exact matrix path");
  Expect(diagnostics.overlap_mismatch == 0.0,
         "bootstrap solver should report zero overlap mismatch on the reviewed exact matrix "
         "path");
  Expect(diagnostics.failure_code.empty(),
         "bootstrap solver should leave failure_code empty on exact matrix success");
}

void BootstrapSeriesSolverExactMixedScalarOneHopHappyPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/eta"), "eta", "eta=1", "eta=0", {"7/11"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(diagnostics.success,
         "bootstrap solver should accept the reviewed exact mixed scalar continuation path");
  Expect(diagnostics.residual_norm == 0.0,
         "bootstrap solver should report zero residual norm on the reviewed exact mixed scalar "
         "path");
  Expect(diagnostics.overlap_mismatch == 0.0,
         "bootstrap solver should report zero overlap mismatch on the reviewed exact mixed "
         "scalar path");
  Expect(diagnostics.failure_code.empty(),
         "bootstrap solver should leave failure_code empty on exact mixed scalar success");
  Expect(diagnostics.summary ==
             "Solved by exact one-hop mixed regular/regular-singular continuation.",
         "bootstrap solver should expose the reviewed mixed exact-success summary");
}

void BootstrapSeriesSolverExactMixedUpperTriangularDiagonalHappyPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeMatrixRegularPointSeriesSystem({{"1/eta", "0"}, {"0", "0"}}),
      "eta",
      "eta=1",
      "eta=0",
      {"2/3", "5/7"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(diagnostics.success,
         "bootstrap solver should accept the reviewed exact mixed diagonal matrix path");
  Expect(diagnostics.residual_norm == 0.0,
         "bootstrap solver should report zero residual norm on the reviewed exact mixed "
         "diagonal matrix path");
  Expect(diagnostics.overlap_mismatch == 0.0,
         "bootstrap solver should report zero overlap mismatch on the reviewed exact mixed "
         "diagonal matrix path");
  Expect(diagnostics.failure_code.empty(),
         "bootstrap solver should leave failure_code empty on exact mixed diagonal matrix "
         "success");
}

void BootstrapSeriesSolverExactMixedUpperTriangularZeroForcingResonanceHappyPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeMatrixRegularPointSeriesSystem({{"1/eta", "eta"}, {"0", "0"}}),
      "eta",
      "eta=1",
      "eta=0",
      {"2/3", "5/7"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(diagnostics.success,
         "bootstrap solver should accept the reviewed exact mixed zero-forcing resonance path");
  Expect(diagnostics.residual_norm == 0.0,
         "bootstrap solver should report zero residual norm on the reviewed exact mixed "
         "zero-forcing resonance path");
  Expect(diagnostics.overlap_mismatch == 0.0,
         "bootstrap solver should report zero overlap mismatch on the reviewed exact mixed "
         "zero-forcing resonance path");
  Expect(diagnostics.failure_code.empty(),
         "bootstrap solver should leave failure_code empty on exact mixed zero-forcing "
         "resonance success");
}

void BootstrapSeriesSolverRejectsInexactOneHopPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("2"), "eta", "eta=0", "eta=2", {"7/11"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject inexact one-hop continuation paths");
  Expect(diagnostics.failure_code == "unsupported_solver_path",
         "bootstrap solver should classify inexact one-hop paths as unsupported_solver_path");
}

void BootstrapSeriesSolverRejectsFractionalExponentMixedPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request =
      MakeManualStartBoundarySolveRequest(MakeScalarRegularPointSeriesSystem("1/(2*eta) + 1/(eta+1)"),
                                          "eta",
                                          "eta=1",
                                          "eta=0",
                                          {"7/11"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject mixed continuation when the target Frobenius exponent "
         "subset is fractional");
  Expect(diagnostics.failure_code == "unsupported_solver_path",
         "bootstrap solver should classify fractional-exponent mixed paths as "
         "unsupported_solver_path");
  Expect((diagnostics.summary.find("integral") != std::string::npos ||
          diagnostics.summary.find("integer") != std::string::npos) &&
             diagnostics.summary.find("Frobenius exponents") != std::string::npos,
         "bootstrap solver should mention integer Frobenius exponents when fractional mixed "
         "continuation remains deferred");
}

void BootstrapSeriesSolverRejectsForcedLogarithmicMixedPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeMatrixRegularPointSeriesSystem({{"1/eta", "1"}, {"0", "0"}}),
      "eta",
      "eta=1",
      "eta=0",
      {"2/3", "5/7"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject mixed paths that force logarithmic Frobenius terms");
  Expect(diagnostics.failure_code == "unsupported_solver_path",
         "bootstrap solver should classify forced-log mixed paths as unsupported_solver_path");
  Expect(diagnostics.summary.find("logarithmic") != std::string::npos,
         "bootstrap solver should preserve the forced-logarithmic-resonance diagnostic");
}

void BootstrapSeriesSolverRejectsSingularStartMixedPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/eta"), "eta", "eta=0", "eta=1", {"7/11"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject mixed continuation when the start point is singular");
  Expect(diagnostics.failure_code == "unsupported_solver_path",
         "bootstrap solver should classify singular-start mixed paths as unsupported_solver_path");
  Expect(diagnostics.summary.find("regular start") != std::string::npos,
         "bootstrap solver should mention the regular-start restriction on singular-start "
         "mixed requests");
}

void BootstrapSeriesSolverRejectsInexactMixedPathTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/eta + 2"), "eta", "eta=1", "eta=0", {"7/11"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject mixed continuation when the exact residual or handoff "
         "checks are nonzero");
  Expect(diagnostics.failure_code == "unsupported_solver_path",
         "bootstrap solver should classify inexact mixed paths as unsupported_solver_path");
  Expect(diagnostics.summary.find("nonzero") != std::string::npos,
         "bootstrap solver should mention nonzero mixed continuation checks on inexact mixed "
         "rejection");
}

void BootstrapSeriesSolverRejectsLowerTriangularSystemTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeMatrixRegularPointSeriesSystem({{"0", "0"}, {"1", "0"}}),
      "eta",
      "eta=0",
      "eta=1",
      {"2/3", "5/7"});

  const amflow::SolverDiagnostics diagnostics = solver.Solve(request);
  Expect(!diagnostics.success,
         "bootstrap solver should reject lower-triangular systems outside the reviewed subset");
  Expect(diagnostics.failure_code == "unsupported_solver_path",
         "bootstrap solver should classify lower-triangular systems as unsupported_solver_path");
}

void BootstrapSeriesSolverExactPathIgnoresPrecisionPolicyAndRequestedDigitsTest() {
  amflow::BootstrapSeriesSolver solver;
  const amflow::SolveRequest baseline_request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"7/11"});
  amflow::SolveRequest varied_request = baseline_request;
  varied_request.precision_policy.working_precision = 211;
  varied_request.precision_policy.chop_precision = 5;
  varied_request.precision_policy.rationalize_precision = 17;
  varied_request.precision_policy.escalation_step = 3;
  varied_request.precision_policy.max_working_precision = 233;
  varied_request.precision_policy.x_order = 9;
  varied_request.precision_policy.x_order_step = 2;
  varied_request.requested_digits = 11;

  const amflow::SolverDiagnostics baseline = solver.Solve(baseline_request);
  const amflow::SolverDiagnostics varied = solver.Solve(varied_request);
  Expect(SameSolverDiagnostics(varied, baseline),
         "bootstrap solver should ignore precision_policy and requested_digits on the reviewed "
         "exact one-hop path");
}

void BootstrapSeriesSolverRejectsMalformedBoundaryValueExpressionTest() {
  amflow::BootstrapSeriesSolver solver;
  amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"1/("});

  ExpectInvalidArgument(
      [&solver, &request]() {
        static_cast<void>(solver.Solve(request));
      },
      "malformed expression",
      "bootstrap solver should preserve malformed explicit boundary values as ordinary "
      "deterministic argument errors");
}

void SolveDifferentialEquationExactScalarHappyPathMatchesBootstrapSolverTest() {
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"7/11"});

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should preserve exact scalar Batch 39 diagnostics");
}

void SolveDifferentialEquationExactUpperTriangularHappyPathMatchesBootstrapSolverTest() {
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeMatrixRegularPointSeriesSystem({{"0", "1"}, {"0", "0"}}),
      "eta",
      "eta=0",
      "eta=1",
      {"2/3", "5/7"});

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should preserve exact upper-triangular Batch 39 "
         "diagnostics");
}

void SolveDifferentialEquationExactMixedHappyPathMatchesBootstrapSolverTest() {
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/eta"), "eta", "eta=1", "eta=0", {"7/11"});

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should preserve exact mixed Batch 43 diagnostics");
}

void SolveDifferentialEquationBoundaryUnsolvedPassthroughTest() {
  const amflow::SolveRequest request = MakeBoundarySolveRequest();

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should preserve boundary_unsolved diagnostics");
}

void SolveDifferentialEquationUnsupportedSolverPathPassthroughTest() {
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("2"), "eta", "eta=0", "eta=2", {"7/11"});

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should preserve unsupported_solver_path diagnostics");
}

void SolveDifferentialEquationMixedUnsupportedSolverPathPassthroughTest() {
  const amflow::SolveRequest request =
      MakeManualStartBoundarySolveRequest(MakeScalarRegularPointSeriesSystem("1/(2*eta) + 1/(eta+1)"),
                                          "eta",
                                          "eta=1",
                                          "eta=0",
                                          {"7/11"});

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should preserve mixed unsupported_solver_path "
         "diagnostics");
}

void SolveDifferentialEquationRejectsMalformedBoundaryValueExpressionLikeBootstrapSolverTest() {
  const amflow::SolveRequest request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"1/("});

  auto capture_invalid_argument = [](auto&& callable,
                                     const std::string& message) -> std::string {
    try {
      static_cast<void>(callable());
    } catch (const std::invalid_argument& error) {
      return error.what();
    }
    throw std::runtime_error(message);
  };

  const std::string expected = capture_invalid_argument(
      [&request]() { return amflow::BootstrapSeriesSolver().Solve(request); },
      "direct bootstrap solver should reject malformed explicit boundary values");
  const std::string actual = capture_invalid_argument(
      [&request]() { return amflow::SolveDifferentialEquation(request); },
      "standalone DE solver wrapper should reject malformed explicit boundary values");
  Expect(actual == expected,
         "standalone DE solver wrapper should preserve the exact malformed-boundary invalid_"
         "argument diagnostic");
}

void SolveDifferentialEquationExactPathIgnoresPrecisionPolicyAndRequestedDigitsTest() {
  const amflow::SolveRequest baseline_request = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"7/11"});
  amflow::SolveRequest varied_request = baseline_request;
  varied_request.precision_policy.working_precision = 211;
  varied_request.precision_policy.chop_precision = 5;
  varied_request.precision_policy.rationalize_precision = 17;
  varied_request.precision_policy.escalation_step = 3;
  varied_request.precision_policy.max_working_precision = 233;
  varied_request.precision_policy.x_order = 9;
  varied_request.precision_policy.x_order_step = 2;
  varied_request.requested_digits = 11;

  const amflow::SolverDiagnostics expected = amflow::BootstrapSeriesSolver().Solve(baseline_request);
  const amflow::SolverDiagnostics actual = amflow::SolveDifferentialEquation(varied_request);
  Expect(SameSolverDiagnostics(actual, expected),
         "standalone DE solver wrapper should keep precision_policy and requested_digits as "
         "no-ops on the reviewed exact path");
}

void SolveDifferentialEquationDoesNotMutateRequestTest() {
  const amflow::SolveRequest original = MakeManualStartBoundarySolveRequest(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=0", "eta=2", {"7/11"});
  amflow::SolveRequest request = original;

  static_cast<void>(amflow::SolveDifferentialEquation(request));
  Expect(SameSolveRequest(request, original),
         "standalone DE solver wrapper should not mutate the caller request");
}

void EvaluateCoefficientMatrixSampleSMatrixTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::ExactRationalMatrix matrix =
      amflow::EvaluateCoefficientMatrix(amflow::MakeSampleDESystem(),
                                        "s",
                                        spec.kinematics.numeric_substitutions);

  Expect(matrix.size() == 2 && matrix[0].size() == 2,
         "coefficient evaluation should preserve the sample s-matrix shape");
  ExpectRationalString(matrix[0][0],
                       "0",
                       "coefficient evaluation should preserve zero sample s-matrix entries");
  ExpectRationalString(matrix[0][1],
                       "1/30",
                       "coefficient evaluation should evaluate the sample s-matrix row-0 col-1 "
                       "exactly");
  ExpectRationalString(matrix[1][0],
                       "1/30",
                       "coefficient evaluation should evaluate the sample s-matrix row-1 col-0 "
                       "exactly");
  ExpectRationalString(matrix[1][1],
                       "0",
                       "coefficient evaluation should preserve zero sample s-matrix entries");
}

void EvaluateCoefficientMatrixSampleEtaMatrixTest() {
  const amflow::NumericEvaluationPoint evaluation_point = {
      {"eta", "1"},
      {"s", "30"},
  };
  const amflow::ExactRationalMatrix matrix =
      amflow::EvaluateCoefficientMatrix(amflow::MakeSampleDESystem(), "eta", evaluation_point);

  Expect(matrix.size() == 2 && matrix[0].size() == 2,
         "coefficient evaluation should preserve the sample eta-matrix shape");
  ExpectRationalString(matrix[0][0],
                       "-1/29",
                       "coefficient evaluation should evaluate the sample eta-matrix row-0 col-0 "
                       "exactly");
  ExpectRationalString(matrix[0][1],
                       "0",
                       "coefficient evaluation should preserve zero sample eta off-diagonal "
                       "entries");
  ExpectRationalString(matrix[1][0],
                       "0",
                       "coefficient evaluation should preserve zero sample eta off-diagonal "
                       "entries");
  ExpectRationalString(matrix[1][1],
                       "-1/29",
                       "coefficient evaluation should evaluate the sample eta-matrix row-1 col-1 "
                       "exactly");
}

void EvaluateCoefficientMatrixGeneratedEtaFixtureTest() {
  const amflow::NumericEvaluationPoint evaluation_point = {
      {"s", "30"},
      {"t", "-10/3"},
  };
  const amflow::ExactRationalMatrix matrix = amflow::EvaluateCoefficientMatrix(
      MakeGeneratedEtaCoefficientEvaluationSystem(), "eta", evaluation_point);

  Expect(matrix.size() == 2 && matrix[0].size() == 2,
         "coefficient evaluation should preserve the generated eta matrix shape");
  ExpectRationalString(matrix[0][0],
                       "-2",
                       "coefficient evaluation should evaluate generated eta row-0 col-0 exactly");
  ExpectRationalString(matrix[0][1],
                       "-31",
                       "coefficient evaluation should evaluate generated eta row-0 col-1 exactly");
  ExpectRationalString(matrix[1][0],
                       "10/3",
                       "coefficient evaluation should evaluate generated eta row-1 col-0 "
                       "exactly");
  ExpectRationalString(matrix[1][1],
                       "-3",
                       "coefficient evaluation should evaluate generated eta row-1 col-1 exactly");
}

void EvaluateCoefficientMatrixAutomaticInvariantFixtureTest() {
  const amflow::NumericEvaluationPoint evaluation_point = {
      {"s", "30"},
  };
  const amflow::ExactRationalMatrix matrix = amflow::EvaluateCoefficientMatrix(
      MakeAutomaticInvariantCoefficientEvaluationSystem(), "s", evaluation_point);

  Expect(matrix.size() == 2 && matrix[0].size() == 2,
         "coefficient evaluation should preserve the automatic invariant matrix shape");
  ExpectRationalString(
      matrix[0][0],
      "3",
      "coefficient evaluation should evaluate the automatic invariant row-0 col-0 exactly");
  ExpectRationalString(
      matrix[0][1],
      "4",
      "coefficient evaluation should evaluate the automatic invariant row-0 col-1 exactly");
  ExpectRationalString(
      matrix[1][0],
      "45",
      "coefficient evaluation should evaluate the automatic invariant row-1 col-0 exactly");
  ExpectRationalString(
      matrix[1][1],
      "45",
      "coefficient evaluation should evaluate the automatic invariant row-1 col-1 exactly");
}

void EvaluateCoefficientExpressionConstantOnlyTest() {
  const amflow::ExactRational value =
      amflow::EvaluateCoefficientExpression("(-1)*(2) + ((-1)*(-1))*(5)", {});
  ExpectRationalString(value,
                       "3",
                       "coefficient evaluation should evaluate constant-only expressions without "
                       "symbol bindings");
}

void EvaluateCoefficientMatrixIgnoresUnusedSubstitutionsTest() {
  amflow::NumericEvaluationPoint evaluation_point =
      amflow::MakeSampleProblemSpec().kinematics.numeric_substitutions;
  evaluation_point["unused"] = "999";

  const amflow::ExactRationalMatrix matrix =
      amflow::EvaluateCoefficientMatrix(amflow::MakeSampleDESystem(), "s", evaluation_point);
  ExpectRationalString(matrix[0][1],
                       "1/30",
                       "coefficient evaluation should ignore extra unused substitutions");
  ExpectRationalString(matrix[1][0],
                       "1/30",
                       "coefficient evaluation should ignore extra unused substitutions");
}

void EvaluateCoefficientMatrixRejectsMissingSymbolTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::EvaluateCoefficientMatrix(
            MakeGeneratedEtaCoefficientEvaluationSystem(),
            "eta",
            {{"s", "30"}}));
      },
      "numeric binding for symbol \"t\"",
      "coefficient evaluation should reject unresolved symbols instead of defaulting them");
}

void EvaluateCoefficientMatrixRejectsDivisionByZeroTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::EvaluateCoefficientMatrix(amflow::MakeSampleDESystem(),
                                                            "eta",
                                                            {{"eta", "30"}, {"s", "30"}}));
      },
      "division by zero",
      "coefficient evaluation should surface division by zero as a plain evaluation failure");
}

void EvaluateCoefficientMatrixRejectsMalformedExpressionTest() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-", "0"},
      {"0", "1"},
  };

  ExpectInvalidArgument(
      [&system]() {
        static_cast<void>(amflow::EvaluateCoefficientMatrix(system,
                                                            "eta",
                                                            {{"eta", "1"}, {"s", "30"}}));
      },
      "malformed expression",
      "coefficient evaluation should reject malformed coefficient strings deterministically");
}

void EvaluateCoefficientMatrixRejectsUnknownVariableTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::EvaluateCoefficientMatrix(amflow::MakeSampleDESystem(),
                                                            "t",
                                                            {{"s", "30"}}));
      },
      "coefficient matrix for variable \"t\"",
      "coefficient evaluation should reject unknown differentiation variables");
}

void EvaluateCoefficientMatrixIsDeterministicAndNonMutatingTest() {
  const amflow::DESystem original = amflow::MakeSampleDESystem();
  amflow::DESystem system = original;
  const amflow::NumericEvaluationPoint evaluation_point = {
      {"eta", "1"},
      {"s", "30"},
  };

  const amflow::ExactRationalMatrix first =
      amflow::EvaluateCoefficientMatrix(system, "eta", evaluation_point);
  const amflow::ExactRationalMatrix second =
      amflow::EvaluateCoefficientMatrix(system, "eta", evaluation_point);

  Expect(SameExactRationalMatrix(first, second),
         "coefficient evaluation should be deterministic across repeated evaluations");
  Expect(SameDESystem(system, original),
         "coefficient evaluation should not mutate the source DE system");
}

void DetectFiniteSingularPointsSampleEtaMatrixTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(amflow::MakeSampleDESystem(), "eta", {{"s", "30"}});

  Expect(points.size() == 1,
         "singular-point analysis should detect exactly one finite sample eta singular point");
  ExpectRationalString(points[0].location,
                       "30",
                       "singular-point analysis should detect eta=s as eta=30 for the sample "
                       "eta matrix");
}

void ClassifyFinitePointSampleEtaMatrixTest() {
  Expect(amflow::ClassifyFinitePoint(amflow::MakeSampleDESystem(),
                                     "eta",
                                     "eta=0",
                                     {{"s", "30"}}) == amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=0 as regular for the sample eta matrix");
  Expect(amflow::ClassifyFinitePoint(amflow::MakeSampleDESystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}}) == amflow::PointClassification::Singular,
         "singular-point analysis should classify eta=s as singular for the sample eta matrix");
}

void DetectFiniteSingularPointsSampleSMatrixTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(amflow::MakeSampleDESystem(), "s", {});

  Expect(points.size() == 1,
         "singular-point analysis should detect exactly one finite sample s singular point");
  ExpectRationalString(points[0].location,
                       "0",
                       "singular-point analysis should detect s=0 for the sample s matrix");
  Expect(amflow::ClassifyFinitePoint(amflow::MakeSampleDESystem(),
                                     "s",
                                     "s=0",
                                     {}) == amflow::PointClassification::Singular,
         "singular-point analysis should classify s=0 as singular for the sample s matrix");
  Expect(amflow::ClassifyFinitePoint(amflow::MakeSampleDESystem(),
                                     "s",
                                     "s=30",
                                     {}) == amflow::PointClassification::Regular,
         "singular-point analysis should classify s=30 as regular for the sample s matrix");
}

void DetectFiniteSingularPointsGeneratedEtaFixtureIsEmptyTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakeGeneratedEtaCoefficientEvaluationSystem(),
      "eta",
      {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.empty(),
         "singular-point analysis should ignore legacy singular-point annotations when the "
         "generated eta matrix is constant in eta");
}

void DetectFiniteSingularPointsAutomaticInvariantFixtureIsEmptyTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakeAutomaticInvariantCoefficientEvaluationSystem(), "s", {});

  Expect(points.empty(),
         "singular-point analysis should ignore legacy singular-point annotations when the "
         "automatic invariant matrix is constant in s");
}

void DetectFiniteSingularPointsCancelsMatchedFactorsTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakeCancellationSingularAnalysisSystem(), "eta", {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.empty(),
         "singular-point analysis should cancel grouped matched factors before unsupported-form "
         "checks and avoid reporting a regular surviving factor as singular");
  Expect(amflow::ClassifyFinitePoint(MakeCancellationSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify the cancelled eta=s point as regular");
  Expect(amflow::ClassifyFinitePoint(MakeCancellationSingularAnalysisSystem(),
                                     "eta",
                                     "eta=t",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify the surviving regular eta=t factor as regular");
}

void DetectFiniteSingularPointsPreservesDistinctSimplePoleSumsTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakeDistinctFinitePoleSingularAnalysisSystem(), "eta", {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.size() == 2,
         "singular-point analysis should preserve both distinct finite simple poles in an "
         "unsimplified sum");
  Expect(ContainsFiniteSingularPoint(points, "30"),
         "singular-point analysis should detect eta=s from an unsimplified simple-pole sum");
  Expect(ContainsFiniteSingularPoint(points, "-10/3"),
         "singular-point analysis should detect eta=t from an unsimplified simple-pole sum");
  Expect(amflow::ClassifyFinitePoint(MakeDistinctFinitePoleSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Singular,
         "singular-point analysis should classify eta=s as singular in an unsimplified "
         "simple-pole sum");
  Expect(amflow::ClassifyFinitePoint(MakeDistinctFinitePoleSingularAnalysisSystem(),
                                     "eta",
                                     "eta=t",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Singular,
         "singular-point analysis should classify eta=t as singular in an unsimplified "
         "simple-pole sum");
}

void DetectFiniteSingularPointsDropsZeroMaskedDeadBranchesTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakeZeroMaskedSingularAnalysisSystem(), "eta", {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.size() == 1,
         "singular-point analysis should drop semantically zero dead branches before rejecting "
         "unsupported higher-order poles");
  ExpectRationalString(points[0].location,
                       "-10/3",
                       "singular-point analysis should keep only the live eta=t simple pole once "
                       "the zero-masked branch is eliminated");
  Expect(amflow::ClassifyFinitePoint(MakeZeroMaskedSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=s as regular when only a zero-masked "
         "higher-order branch reaches that location");
  Expect(amflow::ClassifyFinitePoint(MakeZeroMaskedSingularAnalysisSystem(),
                                     "eta",
                                     "eta=t",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Singular,
         "singular-point analysis should classify eta=t as singular when the live simple pole "
         "survives zero elimination");
}

void DetectFiniteSingularPointsShortCircuitsZeroNumeratorDivisionTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakeDivisionShortCircuitSingularAnalysisSystem(), "eta", {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.empty(),
         "singular-point analysis should short-circuit division when the numerator is already "
         "semantically zero");
  Expect(amflow::ClassifyFinitePoint(MakeDivisionShortCircuitSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=s as regular when zero numerator division "
         "short-circuits after parse-time symbol resolution and zero-divisor preservation");
  Expect(amflow::ClassifyFinitePoint(MakeDivisionShortCircuitSingularAnalysisSystem(),
                                     "eta",
                                     "eta=t",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=t as regular when zero numerator division "
         "short-circuits after parse-time symbol resolution and zero-divisor preservation");
}

void DetectFiniteSingularPointsCancelsGroupedSameDenominatorTermsTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(MakeGroupedSameDenominatorCancellationSingularAnalysisSystem(),
                                         "eta",
                                         {{"s", "30"}});

  Expect(points.empty(),
         "singular-point analysis should normalize grouped same-denominator linear-numerator "
         "cancellation before pole extraction");
  Expect(amflow::ClassifyFinitePoint(MakeGroupedSameDenominatorCancellationSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}}) == amflow::PointClassification::Regular,
         "singular-point analysis should classify grouped same-denominator linear-numerator "
         "cancellation as regular");
}

void DetectFiniteSingularPointsPreservesZeroDivisorFailuresTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(MakeZeroDivisorSingularAnalysisSystem(),
                                                             "eta",
                                                             {}));
      },
      "division by zero",
      "singular-point analysis should preserve zero-divisor failures even when the numerator is "
      "semantically zero");
}

void DetectFiniteSingularPointsAllowsLiteralDirectSimpleDifferenceDivisorsTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(MakeDirectSimpleDifferenceSingularAnalysisSystem(),
                                         "eta",
                                         {{"s", "30"}});

  Expect(points.size() == 1,
         "singular-point analysis should keep literal direct eta-s divisors within the reviewed "
         "Batch 35 carveout");
  ExpectRationalString(points[0].location,
                       "30",
                       "singular-point analysis should detect the literal direct eta-s divisor at "
                       "the bound passive value");
  Expect(amflow::ClassifyFinitePoint(MakeDirectSimpleDifferenceSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}}) == amflow::PointClassification::Singular,
         "singular-point analysis should classify literal direct eta-s divisors as singular");
}

void DetectFiniteSingularPointsRejectsNegatedConstantDifferenceDivisorsTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeUnsupportedNegatedConstantDifferenceSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject eta-(-s) because the direct simple-difference "
      "carveout is syntactic and does not include parenthesized negated constants");
}

void DetectFiniteSingularPointsRejectsGroupedConstantDifferenceDivisorsTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeUnsupportedGroupedConstantDifferenceSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject eta-(s-s) because the direct simple-difference "
      "carveout does not include grouped constant-only RHS forms");
}

void DetectFiniteSingularPointsRejectsNormalizedMultiTermDivisorsTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeUnsupportedNormalizedMultiTermDivisorSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject non-identical multi-term divisors even when they "
      "normalize to one term");
}

void DetectFiniteSingularPointsRejectsRegularMultiTermDivisorsTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeUnsupportedRegularMultiTermDivisorSingularAnalysisSystem(), "eta", {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject non-identical regular multi-term divisors even when "
      "they normalize to one regular term");
}

void DetectFiniteSingularPointsRejectsDirectLinearMultiTermDivisorsTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeUnsupportedDirectLinearMultiTermDivisorSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject syntactically multi-term direct linear divisors such "
      "as eta+s");
}

void DetectFiniteSingularPointsPreservesDivisionByZeroForNormalizedMultiTermDivisorsTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeZeroNormalizedMultiTermDivisorSingularAnalysisSystem(), "eta", {{"s", "30"}}));
      },
      "division by zero",
      "singular-point analysis should report division by zero when a non-identical multi-term "
      "divisor normalizes to zero");
}

void DetectFiniteSingularPointsPreservesDivisionByZeroForZeroNumeratorNormalizedDivisorsTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeZeroNumeratorZeroNormalizedDivisorSingularAnalysisSystem(), "eta", {{"s", "30"}}));
      },
      "division by zero",
      "singular-point analysis should still report division by zero when a zero numerator faces "
      "a divisor that becomes zero only after grouped normalization");
}

void DetectFiniteSingularPointsCancelsUnsupportedHigherOrderDeadBranchesTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(MakeCancelledUnsupportedHigherOrderSingularAnalysisSystem(),
                                         "eta",
                                         {{"s", "30"}});

  Expect(points.empty(),
         "singular-point analysis should cancel semantically identical higher-order dead "
         "branches before unsupported-form rejection");
  Expect(amflow::ClassifyFinitePoint(MakeCancelledUnsupportedHigherOrderSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}}) == amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=s as regular after higher-order dead "
         "branches cancel exactly");
}

void DetectFiniteSingularPointsCancelsUnsupportedMultiFactorDeadBranchesTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(MakeCancelledUnsupportedMultiFactorSingularAnalysisSystem(),
                                         "eta",
                                         {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.empty(),
         "singular-point analysis should cancel semantically identical multi-factor dead "
         "branches before unsupported-form rejection");
  Expect(amflow::ClassifyFinitePoint(MakeCancelledUnsupportedMultiFactorSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=s as regular after multi-factor dead "
         "branches cancel exactly");
  Expect(amflow::ClassifyFinitePoint(MakeCancelledUnsupportedMultiFactorSingularAnalysisSystem(),
                                     "eta",
                                     "eta=t",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify eta=t as regular after multi-factor dead "
         "branches cancel exactly");
}

void ClassifyFinitePointValidatesEveryCellBeforeReturningTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::ClassifyFinitePoint(
            MakeClassificationMatrixAuthoritativeSingularAnalysisSystem(),
            "eta",
            "eta=s",
            {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis classification should validate the full selected matrix before "
      "returning a singular verdict");
}

void DetectFiniteSingularPointsAllowsRegularPolynomialNumeratorProductsTest() {
  const std::vector<amflow::FiniteSingularPoint> points = amflow::DetectFiniteSingularPoints(
      MakePolynomialNumeratorRegularSingularAnalysisSystem(), "eta", {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.empty(),
         "singular-point analysis should treat surviving polynomial numerator products without "
         "denominators as regular");
  Expect(amflow::ClassifyFinitePoint(MakePolynomialNumeratorRegularSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify polynomial numerator products as regular");
}

void DetectFiniteSingularPointsAllowsSimplePoleWithPolynomialNumeratorTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(MakePolynomialNumeratorSimplePoleSingularAnalysisSystem(),
                                         "eta",
                                         {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}});

  Expect(points.size() == 1,
         "singular-point analysis should keep a single surviving simple pole even when the "
         "numerator is a polynomial product");
  ExpectRationalString(points[0].location,
                       "7",
                       "singular-point analysis should detect the denominator root eta=u for a "
                       "simple pole with polynomial numerator");
  Expect(amflow::ClassifyFinitePoint(MakePolynomialNumeratorSimplePoleSingularAnalysisSystem(),
                                     "eta",
                                     "eta=u",
                                     {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}) ==
             amflow::PointClassification::Singular,
         "singular-point analysis should classify eta=u as singular for a simple pole with "
         "polynomial numerator");
  Expect(amflow::ClassifyFinitePoint(MakePolynomialNumeratorSimplePoleSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify numerator roots as regular when only the "
         "denominator root survives");
}

void DetectFiniteSingularPointsAcceptsDuplicateNonlinearNumeratorsAfterCanonicalCombinationTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(
          MakeDuplicateNonlinearNumeratorSharedDenominatorSingularAnalysisSystem(),
          "eta",
          {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}});

  Expect(points.size() == 1,
         "singular-point analysis should treat exact duplicate nonlinear same-denominator terms "
         "as one surviving canonical term after exact combination");
  ExpectRationalString(points[0].location,
                       "7",
                       "singular-point analysis should detect only eta=u for exact duplicate "
                       "nonlinear same-denominator terms after canonical combination");
}

void ClassifyFinitePointAcceptsDuplicateNonlinearNumeratorsAfterCanonicalCombinationTest() {
  Expect(amflow::ClassifyFinitePoint(
             MakeDuplicateNonlinearNumeratorSharedDenominatorSingularAnalysisSystem(),
             "eta",
             "eta=u",
             {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}) ==
             amflow::PointClassification::Singular,
         "singular-point analysis should classify eta=u as singular when exact duplicate "
         "nonlinear same-denominator terms combine to one surviving canonical term");
  Expect(amflow::ClassifyFinitePoint(
             MakeDuplicateNonlinearNumeratorSharedDenominatorSingularAnalysisSystem(),
             "eta",
             "eta=s",
             {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify numerator roots as regular when exact duplicate "
         "nonlinear same-denominator terms combine to one surviving canonical term");
}

void DetectFiniteSingularPointsRejectsGroupedNonlinearNumeratorsUnderSharedDenominatorTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeGroupedNonlinearNumeratorSharedDenominatorSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject same-denominator multi-term groups when any "
      "surviving grouped numerator is nonlinear");
}

void ClassifyFinitePointRejectsGroupedNonlinearNumeratorsUnderSharedDenominatorTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::ClassifyFinitePoint(
            MakeGroupedNonlinearNumeratorSharedDenominatorSingularAnalysisSystem(),
            "eta",
            "eta=u",
            {{"s", "30"}, {"t", "-10/3"}, {"u", "7"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis classification should reject same-denominator multi-term groups "
      "when any surviving grouped numerator is nonlinear");
}

void DetectFiniteSingularPointsShortCircuitsIdenticalMultiTermQuotientsToRegularTest() {
  const std::vector<amflow::FiniteSingularPoint> points =
      amflow::DetectFiniteSingularPoints(MakeIdenticalMultiTermQuotientRegularSingularAnalysisSystem(),
                                         "eta",
                                         {{"s", "30"}, {"t", "-10/3"}});

  Expect(points.empty(),
         "singular-point analysis should reduce exact identical multi-term quotients to a regular "
         "constant");
  Expect(amflow::ClassifyFinitePoint(MakeIdenticalMultiTermQuotientRegularSingularAnalysisSystem(),
                                     "eta",
                                     "eta=s",
                                     {{"s", "30"}, {"t", "-10/3"}}) ==
             amflow::PointClassification::Regular,
         "singular-point analysis should classify exact identical multi-term quotients as regular");
}

void DetectFiniteSingularPointsPreservesDivisionByZeroForIdenticalZeroQuotientsTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeIdenticalMultiTermQuotientZeroSingularAnalysisSystem(), "eta", {{"s", "30"}}));
      },
      "division by zero",
      "singular-point analysis should preserve division by zero before collapsing an identical "
      "multi-term quotient to regular constant 1");
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::ClassifyFinitePoint(
            MakeIdenticalMultiTermQuotientZeroSingularAnalysisSystem(), "eta", "eta=s", {{"s", "30"}}));
      },
      "division by zero",
      "singular-point analysis classification should preserve division by zero before collapsing "
      "an identical multi-term quotient to regular constant 1");
}

void DetectFiniteSingularPointsRejectsIdenticalHigherOrderQuotientsOutsideSupportedShapesTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeIdenticalHigherOrderQuotientUnsupportedSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject exact identical quotients when the shared "
      "expression still contains a surviving higher-order pole");
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::ClassifyFinitePoint(
            MakeIdenticalHigherOrderQuotientUnsupportedSingularAnalysisSystem(),
            "eta",
            "eta=s",
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis classification should reject exact identical quotients when the "
      "shared expression still contains a surviving higher-order pole");
}

void DetectFiniteSingularPointsRejectsIdenticalMultiFactorQuotientsOutsideSupportedShapesTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(
            MakeIdenticalMultiFactorQuotientUnsupportedSingularAnalysisSystem(),
            "eta",
            {{"s", "30"}, {"t", "-10/3"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject exact identical quotients when the shared "
      "expression still contains multiple surviving singular factors");
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::ClassifyFinitePoint(
            MakeIdenticalMultiFactorQuotientUnsupportedSingularAnalysisSystem(),
            "eta",
            "eta=s",
            {{"s", "30"}, {"t", "-10/3"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis classification should reject exact identical quotients when the "
      "shared expression still contains multiple surviving singular factors");
}

void DetectFiniteSingularPointsRejectsMissingPassiveBindingTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(amflow::MakeSampleDESystem(),
                                                             "eta",
                                                             {}));
      },
      "numeric binding for symbol \"s\"",
      "singular-point analysis should reject missing passive bindings");
}

void DetectFiniteSingularPointsRejectsMalformedExpressionTest() {
  amflow::DESystem system = MakeBoundaryAttachmentBaselineDESystem();
  system.coefficient_matrices["eta"] = {
      {"1/(eta-", "0"},
      {"0", "1"},
  };

  ExpectInvalidArgument(
      [&system]() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(system, "eta", {{"s", "30"}}));
      },
      "malformed expression",
      "singular-point analysis should reject malformed coefficient strings deterministically");
}

void DetectFiniteSingularPointsRejectsUnsupportedSingularFormTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(MakeUnsupportedSingularAnalysisSystem(),
                                                             "eta",
                                                             {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject higher-order poles outside the Batch 35 scope");
}

void DetectFiniteSingularPointsRejectsUnsupportedMultiFactorSingularFormTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(
            amflow::DetectFiniteSingularPoints(MakeUnsupportedMultiFactorSingularAnalysisSystem(),
                                               "eta",
                                               {{"s", "30"}, {"t", "-10/3"}}));
      },
      "unsupported singular-form analysis",
      "singular-point analysis should reject surviving multi-factor singular forms outside the "
      "Batch 35 scope");
}

void DetectFiniteSingularPointsRejectsUnknownVariableTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::DetectFiniteSingularPoints(amflow::MakeSampleDESystem(),
                                                             "t",
                                                             {{"s", "30"}}));
      },
      "variable \"t\"",
      "singular-point analysis should reject unknown variables deterministically");
}

void ClassifyFinitePointRejectsMalformedPointExpressionTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::ClassifyFinitePoint(amflow::MakeSampleDESystem(),
                                                      "eta",
                                                      "t=0",
                                                      {{"s", "30"}}));
      },
      "malformed point expression",
      "singular-point analysis should reject malformed point expressions");
}

void DetectFiniteSingularPointsIsDeterministicAndNonMutatingTest() {
  const amflow::DESystem original = amflow::MakeSampleDESystem();
  amflow::DESystem system = original;

  const std::vector<amflow::FiniteSingularPoint> first =
      amflow::DetectFiniteSingularPoints(system, "eta", {{"s", "30"}});
  const std::vector<amflow::FiniteSingularPoint> second =
      amflow::DetectFiniteSingularPoints(system, "eta", {{"s", "30"}});

  Expect(SameFiniteSingularPoints(first, second),
         "singular-point analysis should be deterministic across repeated runs");
  Expect(SameDESystem(system, original),
         "singular-point analysis should not mutate the source DE system");
}

void GenerateScalarRegularPointSeriesPatchZeroCoefficientTest() {
  const amflow::SeriesPatch patch = amflow::GenerateScalarRegularPointSeriesPatch(
      MakeScalarRegularPointSeriesSystem("0"), "eta", "eta=0", 4, {});

  Expect(patch.center == "eta=0",
         "scalar regular-point patch generation should expose the resolved center");
  Expect(patch.order == 4,
         "scalar regular-point patch generation should preserve the requested order");
  ExpectStringVector(patch.basis_functions,
                     {"1",
                      "(eta-(0))",
                      "((eta-(0))*(eta-(0)))",
                      "(((eta-(0))*(eta-(0)))*(eta-(0)))",
                      "((((eta-(0))*(eta-(0)))*(eta-(0)))*(eta-(0)))"},
                     "scalar regular-point patch generation should emit monomial local-shift "
                     "basis functions in increasing order");
  ExpectStringVector(patch.coefficients,
                     {"1", "0", "0", "0", "0"},
                     "scalar regular-point patch generation should keep the zero ODE patch "
                     "constant");
}

void GenerateScalarRegularPointSeriesPatchConstantCoefficientTest() {
  const amflow::SeriesPatch patch = amflow::GenerateScalarRegularPointSeriesPatch(
      MakeScalarRegularPointSeriesSystem("2"), "eta", "eta=0", 4, {});

  ExpectStringVector(patch.coefficients,
                     {"1", "2", "2", "4/3", "2/3"},
                     "scalar regular-point patch generation should solve I' = 2 * I exactly "
                     "through order 4");
}

void GenerateScalarRegularPointSeriesPatchLinearCoefficientTest() {
  const amflow::SeriesPatch patch = amflow::GenerateScalarRegularPointSeriesPatch(
      MakeScalarRegularPointSeriesSystem("eta"), "eta", "eta=0", 5, {});

  ExpectStringVector(patch.coefficients,
                     {"1", "0", "1/2", "0", "1/8", "0"},
                     "scalar regular-point patch generation should solve I' = eta * I exactly "
                     "through order 5");
}

void GenerateScalarRegularPointSeriesPatchShiftedRegularPointTest() {
  const amflow::SeriesPatch patch = amflow::GenerateScalarRegularPointSeriesPatch(
      MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=2", 4, {});

  Expect(patch.center == "eta=2",
         "scalar regular-point patch generation should preserve exact resolved regular centers");
  ExpectStringVector(patch.coefficients,
                     {"1", "1/3", "0", "0", "0"},
                     "scalar regular-point patch generation should solve I' = 1/(eta+1) * I "
                     "exactly around eta=2");
}

void GenerateScalarRegularPointSeriesPatchPassiveBindingCenterTest() {
  const amflow::SeriesPatch patch = amflow::GenerateScalarRegularPointSeriesPatch(
      MakeScalarRegularPointSeriesSystem("1/(eta-s)"), "eta", "eta=s+2", 3, {{"s", "30"}});

  Expect(patch.center == "eta=32",
         "scalar regular-point patch generation should resolve passive-bound center expressions "
         "exactly");
  ExpectStringVector(patch.coefficients,
                     {"1", "1/2", "0", "0"},
                     "scalar regular-point patch generation should solve passive-bound regular "
                     "points exactly");
}

void GenerateScalarRegularPointSeriesPatchRejectsNonScalarSystemTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeBoundaryAttachmentBaselineDESystem(), "eta", "eta=0", 4, {}));
      },
      "exactly one master",
      "scalar regular-point patch generation should reject non-scalar systems");
}

void GenerateScalarRegularPointSeriesPatchRejectsSingularCenterTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta-s)"), "eta", "eta=s", 4, {{"s", "30"}}));
      },
      "requires a regular center",
      "scalar regular-point patch generation should reject singular centers");
}

void GenerateScalarRegularPointSeriesPatchRejectsFallbackSingularCenterTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta+1)"), "eta", "eta=-1", 4, {}));
      },
      "requires a regular center",
      "scalar regular-point patch generation should translate fallback exact-evaluation division "
      "by zero into the normal singular-center rejection");
}

void GenerateScalarRegularPointSeriesPatchRejectsNegativeOrderTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeScalarRegularPointSeriesSystem("0"), "eta", "eta=0", -1, {}));
      },
      "non-negative order",
      "scalar regular-point patch generation should reject negative orders");
}

void GenerateScalarRegularPointSeriesPatchRejectsMalformedCenterExpressionTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeScalarRegularPointSeriesSystem("0"), "eta", "t=0", 4, {}));
      },
      "malformed center expression",
      "scalar regular-point patch generation should reject malformed center expressions");
}

void GenerateScalarRegularPointSeriesPatchRejectsMissingPassiveBindingTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta-s)"), "eta", "eta=0", 4, {}));
      },
      "numeric binding for symbol \"s\"",
      "scalar regular-point patch generation should reject missing passive bindings");
}

void GenerateScalarRegularPointSeriesPatchRejectsUnknownVariableTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeScalarRegularPointSeriesSystem("0"), "s", "s=0", 4, {}));
      },
      "variable \"s\"",
      "scalar regular-point patch generation should reject unknown variables");
}

void GenerateScalarRegularPointSeriesPatchRejectsUnsupportedMatrixShapeTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarRegularPointSeriesPatch(
            MakeUnsupportedScalarSeriesMatrixShapeSystem(), "eta", "eta=0", 4, {}));
      },
      "1x1",
      "scalar regular-point patch generation should reject unsupported coefficient shapes");
}

void GenerateScalarRegularPointSeriesPatchIsDeterministicAndNonMutatingTest() {
  const amflow::DESystem original = MakeScalarRegularPointSeriesSystem("1/(eta+1)");
  amflow::DESystem system = original;

  const amflow::SeriesPatch first =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=2", 4, {});
  const amflow::SeriesPatch second =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=2", 4, {});

  Expect(SameSeriesPatch(first, second),
         "scalar regular-point patch generation should be deterministic across repeated runs");
  Expect(SameDESystem(system, original),
         "scalar regular-point patch generation should not mutate the source DE system");
}

void GenerateScalarFrobeniusSeriesPatchPurePoleHappyPathTest() {
  const amflow::ScalarFrobeniusSeriesPatch patch =
      amflow::GenerateScalarFrobeniusSeriesPatch(
          MakeScalarRegularPointSeriesSystem("1/eta"), "eta", "eta=0", 4, {});

  Expect(patch.center == "eta=0",
         "scalar Frobenius patch generation should expose the resolved center");
  Expect(patch.indicial_exponent == "1",
         "scalar Frobenius patch generation should expose the exact indicial exponent");
  Expect(patch.order == 4,
         "scalar Frobenius patch generation should preserve the requested order");
  ExpectStringVector(patch.basis_functions,
                     {"1",
                      "(eta-(0))",
                      "((eta-(0))*(eta-(0)))",
                      "(((eta-(0))*(eta-(0)))*(eta-(0)))",
                      "((((eta-(0))*(eta-(0)))*(eta-(0)))*(eta-(0)))"},
                     "scalar Frobenius patch generation should reuse the monomial local-shift "
                     "basis for the reduced regular factor");
  ExpectStringVector(patch.coefficients,
                     {"1", "0", "0", "0", "0"},
                     "scalar Frobenius patch generation should keep the reduced factor "
                     "constant for a pure simple pole");
}

void GenerateScalarFrobeniusSeriesPatchPolePlusConstantHappyPathTest() {
  const amflow::ScalarFrobeniusSeriesPatch patch =
      amflow::GenerateScalarFrobeniusSeriesPatch(
          MakeScalarRegularPointSeriesSystem("1/eta + 2"), "eta", "eta=0", 4, {});

  Expect(patch.indicial_exponent == "1",
         "scalar Frobenius patch generation should recover rho from the simple-pole residue");
  ExpectStringVector(patch.coefficients,
                     {"1", "2", "2", "4/3", "2/3"},
                     "scalar Frobenius patch generation should solve the reduced regular "
                     "factor exactly when a constant regular term survives");
}

void GenerateScalarFrobeniusSeriesPatchPolePlusLinearHappyPathTest() {
  const amflow::ScalarFrobeniusSeriesPatch patch =
      amflow::GenerateScalarFrobeniusSeriesPatch(
          MakeScalarRegularPointSeriesSystem("1/eta + eta"), "eta", "eta=0", 5, {});

  ExpectStringVector(patch.coefficients,
                     {"1", "0", "1/2", "0", "1/8", "0"},
                     "scalar Frobenius patch generation should solve the reduced regular "
                     "factor exactly when the regular remainder is linear");
}

void GenerateScalarFrobeniusSeriesPatchHalfIntegerExponentHappyPathTest() {
  const amflow::ScalarFrobeniusSeriesPatch patch =
      amflow::GenerateScalarFrobeniusSeriesPatch(
          MakeScalarRegularPointSeriesSystem("1/(2*eta) + 1/(eta+1)"),
          "eta",
          "eta=0",
          4,
          {});

  Expect(patch.indicial_exponent == "1/2",
         "scalar Frobenius patch generation should preserve fractional indicial exponents");
  ExpectStringVector(patch.coefficients,
                     {"1", "1", "0", "0", "0"},
                     "scalar Frobenius patch generation should solve the reviewed mixed "
                     "half-pole example exactly");
}

void GenerateScalarFrobeniusSeriesPatchPassiveBindingCenterHappyPathTest() {
  const amflow::ScalarFrobeniusSeriesPatch patch =
      amflow::GenerateScalarFrobeniusSeriesPatch(
          MakeScalarRegularPointSeriesSystem("1/(eta-s)"),
          "eta",
          "eta=s",
          3,
          {{"s", "30"}});

  Expect(patch.center == "eta=30",
         "scalar Frobenius patch generation should resolve passive-bound singular centers "
         "exactly");
  Expect(patch.indicial_exponent == "1",
         "scalar Frobenius patch generation should preserve the resolved simple-pole residue");
  ExpectStringVector(patch.coefficients,
                     {"1", "0", "0", "0"},
                     "scalar Frobenius patch generation should keep the reduced factor "
                     "constant for a shifted pure simple pole");
}

void GenerateScalarFrobeniusSeriesPatchRejectsRegularCenterTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("2"), "eta", "eta=0", 4, {}));
      },
      "requires a singular center",
      "scalar Frobenius patch generation should reject regular centers");
}

void GenerateScalarFrobeniusSeriesPatchRejectsHigherOrderPoleTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta*eta)"), "eta", "eta=0", 4, {}));
      },
      "higher-order pole",
      "scalar Frobenius patch generation should reject higher-order poles");
}

void GenerateScalarFrobeniusSeriesPatchRejectsNonScalarSystemTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeBoundaryAttachmentBaselineDESystem(), "eta", "eta=0", 4, {}));
      },
      "exactly one master",
      "scalar Frobenius patch generation should reject non-scalar systems");
}

void GenerateScalarFrobeniusSeriesPatchRejectsNegativeOrderTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/eta"), "eta", "eta=0", -1, {}));
      },
      "non-negative order",
      "scalar Frobenius patch generation should reject negative orders");
}

void GenerateScalarFrobeniusSeriesPatchRejectsMalformedCenterExpressionTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/eta"), "eta", "t=0", 4, {}));
      },
      "malformed center expression",
      "scalar Frobenius patch generation should reject malformed center expressions");
}

void GenerateScalarFrobeniusSeriesPatchRejectsMissingPassiveBindingTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta-s)"), "eta", "eta=s", 4, {}));
      },
      "numeric binding for symbol \"s\"",
      "scalar Frobenius patch generation should reject missing passive bindings");
}

void GenerateScalarFrobeniusSeriesPatchRejectsUnsupportedSingularShapeTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta-(-s))"),
            "eta",
            "eta=-s",
            4,
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "scalar Frobenius patch generation should reject singular shapes outside the reviewed "
      "Batch 35 grammar");
}

void GenerateScalarFrobeniusSeriesPatchRejectsWhitespaceVariantUnsupportedSingularShapeTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateScalarFrobeniusSeriesPatch(
            MakeScalarRegularPointSeriesSystem("1/(eta -(-s))"),
            "eta",
            "eta=-s",
            4,
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "scalar Frobenius patch generation should reject whitespace variants of unsupported "
      "parenthesized direct-difference singular shapes");
}

void GenerateScalarFrobeniusSeriesPatchIsDeterministicAndNonMutatingTest() {
  const amflow::DESystem original =
      MakeScalarRegularPointSeriesSystem("1/(2*eta) + 1/(eta+1)");
  amflow::DESystem system = original;

  const amflow::ScalarFrobeniusSeriesPatch first =
      amflow::GenerateScalarFrobeniusSeriesPatch(system, "eta", "eta=0", 4, {});
  const amflow::ScalarFrobeniusSeriesPatch second =
      amflow::GenerateScalarFrobeniusSeriesPatch(system, "eta", "eta=0", 4, {});

  Expect(SameScalarFrobeniusSeriesPatch(first, second),
         "scalar Frobenius patch generation should be deterministic across repeated runs");
  Expect(SameDESystem(system, original),
         "scalar Frobenius patch generation should not mutate the source DE system");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchNonResonantHappyPathTest() {
  const amflow::UpperTriangularMatrixFrobeniusSeriesPatch patch =
      amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"1/(2*eta)", "1"}, {"0", "0"}}),
          "eta",
          "eta=0",
          3,
          {});

  Expect(patch.center == "eta=0",
         "upper-triangular matrix Frobenius patch generation should expose the resolved "
         "center");
  ExpectStringVector(patch.indicial_exponents,
                     {"1/2", "0"},
                     "upper-triangular matrix Frobenius patch generation should expose the "
                     "diagonal indicial exponents in declared order");
  Expect(patch.order == 3,
         "upper-triangular matrix Frobenius patch generation should preserve the requested "
         "order");
  ExpectStringVector(patch.basis_functions,
                     {"1",
                      "(eta-(0))",
                      "((eta-(0))*(eta-(0)))",
                      "(((eta-(0))*(eta-(0)))*(eta-(0)))"},
                     "upper-triangular matrix Frobenius patch generation should reuse the "
                     "monomial local-shift basis for the reduced factor");

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeExactRationalMatrix({{"0", "2"}, {"0", "0"}}),
      MakeZeroExactRationalMatrix(2),
      MakeZeroExactRationalMatrix(2),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix Frobenius patch generation should solve the nonresonant "
      "upper-triangular simple-pole system exactly");

  std::vector<amflow::ExactRationalMatrix> regular_tail_degree_matrices(
      4, MakeZeroExactRationalMatrix(2));
  regular_tail_degree_matrices[0] = MakeExactRationalMatrix({{"0", "1"}, {"0", "0"}});
  ExpectMatrixFrobeniusPatchRecurrence(
      patch,
      regular_tail_degree_matrices,
      "upper-triangular matrix Frobenius patch generation should satisfy the reduced exact "
      "matrix recurrence for the nonresonant happy path");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchCompatibleZeroForcingResonanceHappyPathTest() {
  const amflow::UpperTriangularMatrixFrobeniusSeriesPatch patch =
      amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"1/eta", "eta"}, {"0", "0"}}),
          "eta",
          "eta=0",
          3,
          {});

  ExpectStringVector(
      patch.indicial_exponents,
      {"1", "0"},
      "upper-triangular matrix Frobenius patch generation should keep the diagonal residues as "
      "the indicial exponents on the compatible-resonance path");

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeZeroExactRationalMatrix(2),
      MakeExactRationalMatrix({{"0", "1"}, {"0", "0"}}),
      MakeZeroExactRationalMatrix(2),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix Frobenius patch generation should deterministically zero-force "
      "the compatible resonant degree and continue exactly");

  std::vector<amflow::ExactRationalMatrix> regular_tail_degree_matrices(
      4, MakeZeroExactRationalMatrix(2));
  regular_tail_degree_matrices[1] = MakeExactRationalMatrix({{"0", "1"}, {"0", "0"}});
  ExpectMatrixFrobeniusPatchRecurrence(
      patch,
      regular_tail_degree_matrices,
      "upper-triangular matrix Frobenius patch generation should satisfy the reduced exact "
      "matrix recurrence on the compatible zero-forcing resonance path");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchDiagonalDirectSumHappyPathTest() {
  const amflow::UpperTriangularMatrixFrobeniusSeriesPatch patch =
      amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
          MakeMatrixRegularPointSeriesSystem(
              {{"1/eta", "0"}, {"0", "1/(2*eta) + 1/(eta+1)"}}),
          "eta",
          "eta=0",
          4,
          {});

  ExpectStringVector(
      patch.indicial_exponents,
      {"1", "1/2"},
      "upper-triangular matrix Frobenius patch generation should preserve diagonal exponents "
      "for a direct-sum system");

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeExactRationalMatrix({{"0", "0"}, {"0", "1"}}),
      MakeZeroExactRationalMatrix(2),
      MakeZeroExactRationalMatrix(2),
      MakeZeroExactRationalMatrix(2),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix Frobenius patch generation should reduce the diagonal direct-sum "
      "baseline to independent scalar Frobenius factors exactly");

  std::vector<amflow::ExactRationalMatrix> regular_tail_degree_matrices(
      5, MakeZeroExactRationalMatrix(2));
  regular_tail_degree_matrices[0] = MakeExactRationalMatrix({{"0", "0"}, {"0", "1"}});
  regular_tail_degree_matrices[1] = MakeExactRationalMatrix({{"0", "0"}, {"0", "-1"}});
  regular_tail_degree_matrices[2] = MakeExactRationalMatrix({{"0", "0"}, {"0", "1"}});
  regular_tail_degree_matrices[3] = MakeExactRationalMatrix({{"0", "0"}, {"0", "-1"}});
  regular_tail_degree_matrices[4] = MakeExactRationalMatrix({{"0", "0"}, {"0", "1"}});
  ExpectMatrixFrobeniusPatchRecurrence(
      patch,
      regular_tail_degree_matrices,
      "upper-triangular matrix Frobenius patch generation should satisfy the reduced exact "
      "matrix recurrence for the diagonal direct-sum baseline");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchPassiveBindingCenterHappyPathTest() {
  const amflow::UpperTriangularMatrixFrobeniusSeriesPatch patch =
      amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"1/(eta-s)", "0"}, {"0", "0"}}),
          "eta",
          "eta=s",
          3,
          {{"s", "30"}});

  Expect(patch.center == "eta=30",
         "upper-triangular matrix Frobenius patch generation should resolve passive-bound "
         "singular centers exactly");
  ExpectStringVector(
      patch.indicial_exponents,
      {"1", "0"},
      "upper-triangular matrix Frobenius patch generation should preserve the resolved diagonal "
      "simple-pole residues");

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeZeroExactRationalMatrix(2),
      MakeZeroExactRationalMatrix(2),
      MakeZeroExactRationalMatrix(2),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix Frobenius patch generation should keep the reduced factor "
      "constant for a shifted diagonal pure-pole system");

  std::vector<amflow::ExactRationalMatrix> regular_tail_degree_matrices(
      4, MakeZeroExactRationalMatrix(2));
  ExpectMatrixFrobeniusPatchRecurrence(
      patch,
      regular_tail_degree_matrices,
      "upper-triangular matrix Frobenius patch generation should satisfy the reduced exact "
      "matrix recurrence for a passive-bound diagonal pure pole");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsForcedLogarithmicResonanceTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta", "1"}, {"0", "0"}}),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "logarithmic Frobenius terms",
      "upper-triangular matrix Frobenius patch generation should reject resonances that force "
      "logarithmic terms");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsOffDiagonalResiduePoleTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta", "1/eta"}, {"0", "0"}}),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "diagonal simple-pole residue matrix",
      "upper-triangular matrix Frobenius patch generation should reject off-diagonal "
      "simple-pole residues");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsRegularCenterTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1", "0"}, {"0", "0"}}),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "requires a singular center",
      "upper-triangular matrix Frobenius patch generation should reject regular centers");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsHigherOrderPoleTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/(eta*eta)", "0"}, {"0", "0"}}),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "higher-order pole",
      "upper-triangular matrix Frobenius patch generation should reject higher-order poles");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsLowerTriangularTailSupportTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta", "0"}, {"1", "0"}}),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "strictly lower-triangular local support",
      "upper-triangular matrix Frobenius patch generation should reject lower-triangular "
      "regular-tail support");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsNonSquareMatrixTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta", "0"}, {"0"}}, 2),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "square and dimension-matched",
      "upper-triangular matrix Frobenius patch generation should reject non-square selected "
      "matrices");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsDimensionMismatchedMatrixTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta"}}, 2),
            "eta",
            "eta=0",
            3,
            {}));
      },
      "square and dimension-matched",
      "upper-triangular matrix Frobenius patch generation should reject dimension-mismatched "
      "selected matrices");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsNegativeOrderTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta", "0"}, {"0", "0"}}),
            "eta",
            "eta=0",
            -1,
            {}));
      },
      "non-negative order",
      "upper-triangular matrix Frobenius patch generation should reject negative orders");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsMalformedCenterExpressionTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/eta", "0"}, {"0", "0"}}),
            "eta",
            "t=0",
            3,
            {}));
      },
      "malformed center expression",
      "upper-triangular matrix Frobenius patch generation should reject malformed center "
      "expressions");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsMissingPassiveBindingTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/(eta-s)", "0"}, {"0", "0"}}),
            "eta",
            "eta=s",
            3,
            {}));
      },
      "numeric binding for symbol \"s\"",
      "upper-triangular matrix Frobenius patch generation should reject missing passive "
      "bindings");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsUnsupportedSingularShapeTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/(eta-(-s))", "0"}, {"0", "0"}}),
            "eta",
            "eta=-s",
            3,
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "upper-triangular matrix Frobenius patch generation should reject singular shapes outside "
      "the reviewed Batch 35 grammar");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsWhitespaceVariantUnsupportedSingularShapeTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/(eta -(-s))", "0"}, {"0", "0"}}),
            "eta",
            "eta=-s",
            3,
            {{"s", "30"}}));
      },
      "unsupported singular-form analysis",
      "upper-triangular matrix Frobenius patch generation should reject whitespace variants of "
      "unsupported parenthesized direct-difference singular shapes");
}

void GenerateUpperTriangularMatrixFrobeniusSeriesPatchIsDeterministicAndNonMutatingTest() {
  const amflow::DESystem original =
      MakeMatrixRegularPointSeriesSystem({{"1/eta", "eta"}, {"0", "0"}});
  amflow::DESystem system = original;

  const amflow::UpperTriangularMatrixFrobeniusSeriesPatch first =
      amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
          system, "eta", "eta=0", 3, {});
  const amflow::UpperTriangularMatrixFrobeniusSeriesPatch second =
      amflow::GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
          system, "eta", "eta=0", 3, {});

  Expect(SameUpperTriangularMatrixFrobeniusSeriesPatch(first, second),
         "upper-triangular matrix Frobenius patch generation should be deterministic across "
         "repeated runs");
  Expect(SameDESystem(system, original),
         "upper-triangular matrix Frobenius patch generation should not mutate the source DE "
         "system");
}

void GenerateUpperTriangularRegularPointSeriesPatchDiagonalHappyPathTest() {
  const amflow::UpperTriangularMatrixSeriesPatch patch =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"2", "0"}, {"0", "eta"}}),
          "eta",
          "eta=0",
          4,
          {});

  Expect(patch.center == "eta=0",
         "upper-triangular matrix patch generation should expose the resolved center");
  Expect(patch.order == 4,
         "upper-triangular matrix patch generation should preserve the requested order");
  ExpectStringVector(patch.basis_functions,
                     {"1",
                      "(eta-(0))",
                      "((eta-(0))*(eta-(0)))",
                      "(((eta-(0))*(eta-(0)))*(eta-(0)))",
                      "((((eta-(0))*(eta-(0)))*(eta-(0)))*(eta-(0)))"},
                     "upper-triangular matrix patch generation should reuse the scalar "
                     "monomial local-shift basis");

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeExactRationalMatrix({{"2", "0"}, {"0", "0"}}),
      MakeExactRationalMatrix({{"2", "0"}, {"0", "1/2"}}),
      MakeExactRationalMatrix({{"4/3", "0"}, {"0", "0"}}),
      MakeExactRationalMatrix({{"2/3", "0"}, {"0", "1/8"}}),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix patch generation should solve the mixed constant/linear diagonal "
      "system exactly through order 4");

  std::vector<amflow::ExactRationalMatrix> local_degree_matrices(5,
                                                                 MakeZeroExactRationalMatrix(2));
  local_degree_matrices[0] = MakeExactRationalMatrix({{"2", "0"}, {"0", "0"}});
  local_degree_matrices[1] = MakeExactRationalMatrix({{"0", "0"}, {"0", "1"}});
  ExpectMatrixPatchRecurrence(
      patch,
      local_degree_matrices,
      "upper-triangular matrix patch generation should satisfy the exact matrix recurrence for "
      "the mixed diagonal system");
}

void GenerateUpperTriangularRegularPointSeriesPatchConstantUpperTriangularHappyPathTest() {
  const amflow::UpperTriangularMatrixSeriesPatch patch =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"1", "3"}, {"0", "2"}}),
          "eta",
          "eta=0",
          4,
          {});

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeExactRationalMatrix({{"1", "3"}, {"0", "2"}}),
      MakeExactRationalMatrix({{"1/2", "9/2"}, {"0", "2"}}),
      MakeExactRationalMatrix({{"1/6", "7/2"}, {"0", "4/3"}}),
      MakeExactRationalMatrix({{"1/24", "15/8"}, {"0", "2/3"}}),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix patch generation should solve the constant upper-triangular "
      "system exactly through order 4");

  std::vector<amflow::ExactRationalMatrix> local_degree_matrices(5,
                                                                 MakeZeroExactRationalMatrix(2));
  local_degree_matrices[0] = MakeExactRationalMatrix({{"1", "3"}, {"0", "2"}});
  ExpectMatrixPatchRecurrence(
      patch,
      local_degree_matrices,
      "upper-triangular matrix patch generation should satisfy the exact matrix recurrence for "
      "the constant upper-triangular system");
}

void GenerateUpperTriangularRegularPointSeriesPatchNilpotentChainHappyPathTest() {
  const amflow::UpperTriangularMatrixSeriesPatch patch =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"0", "1", "0"},
                                              {"0", "0", "1"},
                                              {"0", "0", "0"}}),
          "eta",
          "eta=0",
          4,
          {});

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0", "0"}, {"0", "1", "0"}, {"0", "0", "1"}}),
      MakeExactRationalMatrix({{"0", "1", "0"}, {"0", "0", "1"}, {"0", "0", "0"}}),
      MakeExactRationalMatrix({{"0", "0", "1/2"}, {"0", "0", "0"}, {"0", "0", "0"}}),
      MakeZeroExactRationalMatrix(3),
      MakeZeroExactRationalMatrix(3),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix patch generation should terminate the nilpotent chain exactly "
      "through order 4");

  std::vector<amflow::ExactRationalMatrix> local_degree_matrices(5,
                                                                 MakeZeroExactRationalMatrix(3));
  local_degree_matrices[0] =
      MakeExactRationalMatrix({{"0", "1", "0"}, {"0", "0", "1"}, {"0", "0", "0"}});
  ExpectMatrixPatchRecurrence(
      patch,
      local_degree_matrices,
      "upper-triangular matrix patch generation should satisfy the exact matrix recurrence for "
      "the nilpotent chain");
}

void GenerateUpperTriangularRegularPointSeriesPatchPassiveBindingHappyPathTest() {
  const amflow::UpperTriangularMatrixSeriesPatch patch =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"1/(eta-s)", "0"}, {"0", "2"}}),
          "eta",
          "eta=s+2",
          3,
          {{"s", "30"}});

  Expect(patch.center == "eta=32",
         "upper-triangular matrix patch generation should resolve passive-bound center "
         "expressions exactly");

  const std::vector<amflow::ExactRationalMatrix> expected_coefficients = {
      MakeExactRationalMatrix({{"1", "0"}, {"0", "1"}}),
      MakeExactRationalMatrix({{"1/2", "0"}, {"0", "2"}}),
      MakeExactRationalMatrix({{"0", "0"}, {"0", "2"}}),
      MakeExactRationalMatrix({{"0", "0"}, {"0", "4/3"}}),
  };
  ExpectExactRationalMatrixVector(
      patch.coefficient_matrices,
      expected_coefficients,
      "upper-triangular matrix patch generation should solve the passive-bound diagonal system "
      "exactly through order 3");

  std::vector<amflow::ExactRationalMatrix> local_degree_matrices(4,
                                                                 MakeZeroExactRationalMatrix(2));
  local_degree_matrices[0] = MakeExactRationalMatrix({{"1/2", "0"}, {"0", "2"}});
  local_degree_matrices[1] = MakeExactRationalMatrix({{"-1/4", "0"}, {"0", "0"}});
  local_degree_matrices[2] = MakeExactRationalMatrix({{"1/8", "0"}, {"0", "0"}});
  local_degree_matrices[3] = MakeExactRationalMatrix({{"-1/16", "0"}, {"0", "0"}});
  ExpectMatrixPatchRecurrence(
      patch,
      local_degree_matrices,
      "upper-triangular matrix patch generation should satisfy the exact matrix recurrence for "
      "the passive-bound diagonal system");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsSingularCenterTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/(eta+1)", "0"}, {"0", "2"}}),
            "eta",
            "eta=-1",
            3,
            {}));
      },
      "requires a regular center",
      "upper-triangular matrix patch generation should reject singular centers after the "
      "raw-divisor fallback check");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsNonSquareMatrixTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1", "0"}, {"0"}}, 2),
            "eta",
            "eta=0",
            2,
            {}));
      },
      "square and dimension-matched",
      "upper-triangular matrix patch generation should reject non-square selected matrices");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsDimensionMismatchedMatrixTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1"}}, 2),
            "eta",
            "eta=0",
            2,
            {}));
      },
      "square and dimension-matched",
      "upper-triangular matrix patch generation should reject dimension-mismatched selected "
      "matrices");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsLowerTriangularSupportTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"0", "0"}, {"1", "0"}}),
            "eta",
            "eta=0",
            2,
            {}));
      },
      "strictly lower-triangular local support",
      "upper-triangular matrix patch generation should reject surviving lower-triangular "
      "coefficients");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsNegativeOrderTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1", "0"}, {"0", "2"}}),
            "eta",
            "eta=0",
            -1,
            {}));
      },
      "non-negative order",
      "upper-triangular matrix patch generation should reject negative orders");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsMalformedCenterExpressionTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1", "0"}, {"0", "2"}}),
            "eta",
            "t=0",
            2,
            {}));
      },
      "malformed center expression",
      "upper-triangular matrix patch generation should reject malformed center expressions");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsMissingPassiveBindingTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1/(eta-s)", "0"}, {"0", "2"}}),
            "eta",
            "eta=0",
            2,
            {}));
      },
      "numeric binding for symbol \"s\"",
      "upper-triangular matrix patch generation should reject missing passive bindings");
}

void GenerateUpperTriangularRegularPointSeriesPatchRejectsUnknownVariableTest() {
  ExpectInvalidArgument(
      []() {
        static_cast<void>(amflow::GenerateUpperTriangularRegularPointSeriesPatch(
            MakeMatrixRegularPointSeriesSystem({{"1", "0"}, {"0", "2"}}),
            "s",
            "s=0",
            2,
            {}));
      },
      "variable \"s\"",
      "upper-triangular matrix patch generation should reject unknown variables");
}

void GenerateUpperTriangularRegularPointSeriesPatchIsDeterministicAndNonMutatingTest() {
  const amflow::DESystem original =
      MakeMatrixRegularPointSeriesSystem({{"1/(eta-s)", "0"}, {"0", "2"}});
  amflow::DESystem system = original;

  const amflow::UpperTriangularMatrixSeriesPatch first =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=s+2", 3, {{"s", "30"}});
  const amflow::UpperTriangularMatrixSeriesPatch second =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=s+2", 3, {{"s", "30"}});

  Expect(SameUpperTriangularMatrixSeriesPatch(first, second),
         "upper-triangular matrix patch generation should be deterministic across repeated "
         "runs");
  Expect(SameDESystem(system, original),
         "upper-triangular matrix patch generation should not mutate the source DE system");
}

void ScalarSeriesPatchExactZeroOverlapAndResidualTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("1/(eta+1)");
  const amflow::SeriesPatch left =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});
  const amflow::SeriesPatch right =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=2", 4, {});

  const amflow::ScalarSeriesPatchOverlapDiagnostics overlap =
      amflow::MatchScalarSeriesPatches(
          "eta", left, right, "eta=1", "eta=1/2", {});
  ExpectRationalString(overlap.lambda,
                       "1/3",
                       "scalar overlap diagnostics should return the exact match-scale for the "
                       "reviewed exact-zero overlap case");
  ExpectRationalString(overlap.mismatch,
                       "0",
                       "scalar overlap diagnostics should return zero exact mismatch for the "
                       "reviewed exact-zero overlap case");

  const amflow::ExactRational residual =
      amflow::EvaluateScalarSeriesPatchResidual(system, "eta", left, "eta=1/2", {});
  ExpectRationalString(residual,
                       "0",
                       "scalar residual diagnostics should vanish exactly for the reviewed "
                       "exact regular-point solution");
}

void ScalarSeriesPatchTruncationMismatchAndResidualTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("2");
  const amflow::SeriesPatch left =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});
  const amflow::SeriesPatch right =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=1", 4, {});

  const amflow::ScalarSeriesPatchOverlapDiagnostics overlap =
      amflow::MatchScalarSeriesPatches(
          "eta", left, right, "eta=1/2", "eta=1/4", {});
  ExpectRationalString(overlap.lambda,
                       "9/65",
                       "scalar overlap diagnostics should return the reviewed truncation match "
                       "scale exactly");
  ExpectRationalString(overlap.mismatch,
                       "47/1040",
                       "scalar overlap diagnostics should return the reviewed truncation "
                       "mismatch exactly");

  const amflow::ExactRational residual =
      amflow::EvaluateScalarSeriesPatchResidual(system, "eta", left, "eta=1/2", {});
  ExpectRationalString(residual,
                       "-1/12",
                       "scalar residual diagnostics should return the reviewed truncation "
                       "residual exactly");
}

void ScalarSeriesPatchOverlapRejectsZeroMatchValueTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("1/(eta+1)");
  const amflow::SeriesPatch left =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});
  const amflow::SeriesPatch right =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=2", 4, {});

  ExpectRuntimeError(
      [&left, &right]() {
        static_cast<void>(amflow::MatchScalarSeriesPatches(
            "eta", left, right, "eta=-1", "eta=0", {}));
      },
      "division by zero",
      "scalar overlap diagnostics should propagate plain division by zero when the left match "
      "value vanishes");
}

void ScalarSeriesPatchOverlapRejectsIdenticalResolvedPointsTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("2");
  const amflow::SeriesPatch left =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});
  const amflow::SeriesPatch right =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=1", 4, {});

  ExpectInvalidArgument(
      [&left, &right]() {
        static_cast<void>(amflow::MatchScalarSeriesPatches(
            "eta", left, right, "eta=1/2", "1/2", {}));
      },
      "distinct match and check points",
      "scalar overlap diagnostics should require distinct points after exact resolution");
}

void ScalarSeriesPatchResidualRejectsMalformedPointExpressionTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("2");
  const amflow::SeriesPatch patch =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});

  ExpectInvalidArgument(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateScalarSeriesPatchResidual(
            system, "eta", patch, "t=1", {}));
      },
      "malformed point expression",
      "scalar residual diagnostics should reject malformed point expressions");
}

void ScalarSeriesPatchResidualRejectsMissingPassiveBindingTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("1/(eta-s)");
  const amflow::SeriesPatch patch = amflow::GenerateScalarRegularPointSeriesPatch(
      system, "eta", "eta=0", 3, {{"s", "2"}});

  ExpectInvalidArgument(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateScalarSeriesPatchResidual(
            system, "eta", patch, "eta=s+1", {}));
      },
      "numeric binding for symbol \"s\"",
      "scalar residual diagnostics should reject missing passive bindings in point "
      "resolution");
}

void ScalarSeriesPatchResidualRejectsUnknownVariableTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("2");
  const amflow::SeriesPatch patch =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});

  ExpectInvalidArgument(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateScalarSeriesPatchResidual(
            system, "s", patch, "s=1/2", {}));
      },
      "variable \"s\"",
      "scalar residual diagnostics should reject unknown selected variables");
}

void ScalarSeriesPatchResidualRejectsMalformedPatchCenterTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("2");
  amflow::SeriesPatch patch =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});
  patch.center = "eta==0";

  ExpectInvalidArgument(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateScalarSeriesPatchResidual(
            system, "eta", patch, "eta=1/2", {}));
      },
      "malformed patch center",
      "scalar residual diagnostics should reject malformed public patch centers");
}

void ScalarSeriesPatchResidualRejectsBadStorageSizeTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("2");
  amflow::SeriesPatch patch =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});
  patch.coefficients.pop_back();

  ExpectInvalidArgument(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateScalarSeriesPatchResidual(
            system, "eta", patch, "eta=1/2", {}));
      },
      "SeriesPatch.coefficients.size()",
      "scalar residual diagnostics should reject malformed public patch storage sizes");
}

void ScalarSeriesPatchResidualPropagatesSingularPointDivisionByZeroTest() {
  const amflow::DESystem system = MakeScalarRegularPointSeriesSystem("1/(eta+1)");
  const amflow::SeriesPatch patch =
      amflow::GenerateScalarRegularPointSeriesPatch(system, "eta", "eta=0", 4, {});

  ExpectRuntimeError(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateScalarSeriesPatchResidual(
            system, "eta", patch, "eta=-1", {}));
      },
      "division by zero",
      "scalar residual diagnostics should propagate plain division by zero at singular "
      "evaluation points");
}

void UpperTriangularMatrixSeriesPatchExactZeroOverlapAndResidualTest() {
  const amflow::DESystem system =
      MakeMatrixRegularPointSeriesSystem({{"0", "1"}, {"0", "0"}});
  const amflow::UpperTriangularMatrixSeriesPatch left =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=0", 3, {});
  const amflow::UpperTriangularMatrixSeriesPatch right =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=1", 3, {});

  const amflow::UpperTriangularMatrixSeriesPatchOverlapDiagnostics overlap =
      amflow::MatchUpperTriangularMatrixSeriesPatches(
          "eta", left, right, "eta=1/2", "eta=1/4", {});
  ExpectExactRationalMatrix(
      overlap.match_matrix,
      MakeExactRationalMatrix({{"1", "-1"}, {"0", "1"}}),
      "upper-triangular matrix overlap diagnostics should return the exact reviewed match "
      "matrix for the nilpotent chain");
  ExpectExactRationalMatrix(
      overlap.mismatch,
      MakeZeroExactRationalMatrix(2),
      "upper-triangular matrix overlap diagnostics should return zero mismatch for the "
      "reviewed exact-zero overlap case");

  const amflow::ExactRationalMatrix residual =
      amflow::EvaluateUpperTriangularMatrixSeriesPatchResidual(
          system, "eta", left, "eta=1/2", {});
  ExpectExactRationalMatrix(
      residual,
      MakeZeroExactRationalMatrix(2),
      "upper-triangular matrix residual diagnostics should vanish exactly for the reviewed "
      "nilpotent-chain solution");
}

void UpperTriangularMatrixSeriesPatchTruncationMismatchAndResidualTest() {
  const amflow::DESystem system =
      MakeMatrixRegularPointSeriesSystem({{"2", "0"}, {"0", "0"}});
  const amflow::UpperTriangularMatrixSeriesPatch left =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=0", 4, {});
  const amflow::UpperTriangularMatrixSeriesPatch right =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=1", 4, {});

  const amflow::UpperTriangularMatrixSeriesPatchOverlapDiagnostics overlap =
      amflow::MatchUpperTriangularMatrixSeriesPatches(
          "eta", left, right, "eta=1/2", "eta=1/4", {});
  ExpectExactRationalMatrix(
      overlap.match_matrix,
      MakeExactRationalMatrix({{"9/65", "0"}, {"0", "1"}}),
      "upper-triangular matrix overlap diagnostics should return the reviewed truncation "
      "match matrix exactly");
  ExpectExactRationalMatrix(
      overlap.mismatch,
      MakeExactRationalMatrix({{"47/1040", "0"}, {"0", "0"}}),
      "upper-triangular matrix overlap diagnostics should return the reviewed truncation "
      "mismatch matrix exactly");

  const amflow::ExactRationalMatrix residual =
      amflow::EvaluateUpperTriangularMatrixSeriesPatchResidual(
          system, "eta", left, "eta=1/2", {});
  ExpectExactRationalMatrix(
      residual,
      MakeExactRationalMatrix({{"-1/12", "0"}, {"0", "0"}}),
      "upper-triangular matrix residual diagnostics should return the reviewed diagonal "
      "truncation residual exactly");
}

void UpperTriangularMatrixSeriesPatchCoupledResidualTest() {
  const amflow::DESystem system =
      MakeMatrixRegularPointSeriesSystem({{"1", "3"}, {"0", "2"}});
  const amflow::UpperTriangularMatrixSeriesPatch patch =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=0", 4, {});

  const amflow::ExactRationalMatrix residual =
      amflow::EvaluateUpperTriangularMatrixSeriesPatchResidual(
          system, "eta", patch, "eta=1/2", {});
  ExpectExactRationalMatrix(
      residual,
      MakeExactRationalMatrix({{"-1/384", "-31/128"}, {"0", "-1/12"}}),
      "upper-triangular matrix residual diagnostics should return the reviewed coupled "
      "residual exactly");
}

void UpperTriangularMatrixSeriesPatchOverlapRejectsSingularMatchMatrixTest() {
  const amflow::DESystem system =
      MakeMatrixRegularPointSeriesSystem({{"1/(eta+1)", "0"}, {"0", "0"}});
  const amflow::UpperTriangularMatrixSeriesPatch left =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=0", 3, {});
  const amflow::UpperTriangularMatrixSeriesPatch right =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=1", 3, {});

  ExpectRuntimeError(
      [&left, &right]() {
        static_cast<void>(amflow::MatchUpperTriangularMatrixSeriesPatches(
            "eta", left, right, "eta=-1", "eta=0", {}));
      },
      "division by zero",
      "upper-triangular matrix overlap diagnostics should propagate plain division by zero "
      "when the left match matrix is singular");
}

void UpperTriangularMatrixSeriesPatchOverlapRejectsDimensionMismatchTest() {
  const amflow::UpperTriangularMatrixSeriesPatch left =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          MakeMatrixRegularPointSeriesSystem({{"2", "0"}, {"0", "0"}}),
          "eta",
          "eta=0",
          4,
          {});
  const amflow::UpperTriangularMatrixSeriesPatch right =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          MakeMatrixRegularPointSeriesSystem(
              {{"0", "1", "0"}, {"0", "0", "1"}, {"0", "0", "0"}}),
          "eta",
          "eta=0",
          3,
          {});

  ExpectInvalidArgument(
      [&left, &right]() {
        static_cast<void>(amflow::MatchUpperTriangularMatrixSeriesPatches(
            "eta", left, right, "eta=1/2", "eta=1/4", {}));
      },
      "matching dimensions",
      "upper-triangular matrix overlap diagnostics should reject dimension-mismatched patches");
}

void UpperTriangularMatrixSeriesPatchResidualRejectsNonSquareStoredCoefficientsTest() {
  const amflow::DESystem system =
      MakeMatrixRegularPointSeriesSystem({{"2", "0"}, {"0", "0"}});
  amflow::UpperTriangularMatrixSeriesPatch patch =
      amflow::GenerateUpperTriangularRegularPointSeriesPatch(
          system, "eta", "eta=0", 4, {});
  patch.coefficient_matrices[1] = MakeExactRationalMatrix({{"2", "0"}});

  ExpectInvalidArgument(
      [&system, &patch]() {
        static_cast<void>(amflow::EvaluateUpperTriangularMatrixSeriesPatchResidual(
            system, "eta", patch, "eta=1/2", {}));
      },
      "square stored matrix coefficients",
      "upper-triangular matrix residual diagnostics should reject malformed non-square stored "
      "matrix coefficients");
}

void EtaDerivativeGenerationHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(master_basis, MakeEtaGeneratedHappyTransform());

  Expect(generated_variable.variable.name == "eta",
         "eta derivative generation should expose the eta variable name");
  Expect(generated_variable.variable.kind == amflow::DifferentiationVariableKind::Eta,
         "eta derivative generation should expose eta variable kind");
  Expect(generated_variable.rows.size() == 2,
         "eta derivative generation should produce one row per master");
  Expect(generated_variable.rows[0].source_master.Label() ==
             "planar_double_box[1,1,1,1,1,1,1]",
         "eta derivative generation should preserve the first source master");
  Expect(generated_variable.rows[0].terms.size() == 2,
         "eta derivative generation should emit two terms for the first row");
  Expect(generated_variable.rows[0].terms[0].coefficient == "-1" &&
             generated_variable.rows[0].terms[0].target.Label() ==
                 "planar_double_box[2,1,1,1,1,1,1]",
         "eta derivative generation should shift the first selected propagator");
  Expect(generated_variable.rows[0].terms[1].coefficient == "-1" &&
             generated_variable.rows[0].terms[1].target.Label() ==
                 "planar_double_box[1,1,1,1,1,1,2]",
         "eta derivative generation should shift the second selected propagator");
  Expect(generated_variable.rows[1].terms.size() == 1,
         "eta derivative generation should omit zero-exponent terms");
  Expect(generated_variable.rows[1].terms[0].target.Label() ==
             "planar_double_box[2,1,1,1,1,1,0]",
         "eta derivative generation should preserve the row-1 shifted target");
  Expect(generated_variable.reduction_targets.size() == 3,
         "eta derivative generation should collect the deduplicated reduction-target list");
  Expect(generated_variable.reduction_targets[0].Label() ==
             "planar_double_box[2,1,1,1,1,1,1]" &&
             generated_variable.reduction_targets[1].Label() ==
                 "planar_double_box[1,1,1,1,1,1,2]" &&
             generated_variable.reduction_targets[2].Label() ==
                 "planar_double_box[2,1,1,1,1,1,0]",
         "eta derivative generation should preserve first-appearance reduction-target order");
}

void EtaDerivativeGenerationNegativeExponentTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "planar_double_box";
  master_basis.masters = {
      {"planar_double_box", {-2, 1, 1, 1, 1, 1, 0}},
  };

  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(master_basis, MakeEtaGeneratedHappyTransform());

  Expect(generated_variable.rows.size() == 1,
         "negative-exponent eta generation should still emit one row");
  Expect(generated_variable.rows[0].terms.size() == 1,
         "negative-exponent eta generation should still omit zero selected exponents");
  Expect(generated_variable.rows[0].terms[0].coefficient == "2",
         "negative selected exponents should flip sign under the -ai rule");
  Expect(generated_variable.rows[0].terms[0].target.Label() ==
             "planar_double_box[-1,1,1,1,1,1,0]",
         "negative selected exponents should still increment the chosen propagator index");
}

void EtaDerivativeGenerationRejectsArityMismatchTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "planar_double_box";
  master_basis.masters = {
      {"planar_double_box", {1, 1, 1}},
  };

  ExpectRuntimeError(
      [&master_basis]() {
        static_cast<void>(
            amflow::GenerateEtaDerivativeVariable(master_basis, MakeEtaGeneratedHappyTransform()));
      },
      "master index count to match the transformed family propagator count",
      "eta derivative generation should reject masters whose arity mismatches the transformed family");
}

void EtaDerivativeGenerationRejectsFamilyMismatchTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "planar_double_box";
  master_basis.masters = {
      {"wrong_family", {1, 1, 1, 1, 1, 1, 1}},
  };

  ExpectRuntimeError(
      [&master_basis]() {
        static_cast<void>(
            amflow::GenerateEtaDerivativeVariable(master_basis, MakeEtaGeneratedHappyTransform()));
      },
      "requires master family",
      "eta derivative generation should reject masters from the wrong family");
}

void EtaDerivativeGenerationRejectsInconsistentParsedMasterListFamilyTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "wrong_family";
  master_basis.masters = {
      {"planar_double_box", {1, 1, 1, 1, 1, 1, 1}},
  };

  ExpectRuntimeError(
      [&master_basis]() {
        static_cast<void>(
            amflow::GenerateEtaDerivativeVariable(master_basis, MakeEtaGeneratedHappyTransform()));
      },
      "requires ParsedMasterList.family",
      "eta derivative generation should reject an inconsistent ParsedMasterList family header");
}

void InvariantDerivativeGenerationHappyPathTest() {
  const amflow::ParsedMasterList master_basis = MakeInvariantGenerationHappyMasterBasis();
  const amflow::InvariantDerivativeSeed seed = MakeInvariantGenerationHappySeed();

  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateInvariantDerivativeVariable(master_basis, seed);

  Expect(generated_variable.variable.name == "s",
         "invariant derivative generation should preserve the variable name");
  Expect(generated_variable.variable.kind == amflow::DifferentiationVariableKind::Invariant,
         "invariant derivative generation should preserve invariant variable kind");
  Expect(generated_variable.rows.size() == 2,
         "invariant derivative generation should emit one row per master");
  Expect(generated_variable.rows[0].source_master.Label() == "toy_family[1,1,0]",
         "invariant derivative generation should preserve the first source master");
  Expect(generated_variable.rows[0].terms.size() == 2,
         "invariant derivative generation should combine duplicate row targets");
  Expect(generated_variable.rows[0].terms[0].target.Label() == "toy_family[1,1,0]" &&
             generated_variable.rows[0].terms[0].coefficient ==
                 "(-1)*(a) + (-1)*(c)",
         "invariant derivative generation should combine same-target contributions in encounter "
         "order");
  Expect(generated_variable.rows[0].terms[1].target.Label() == "toy_family[2,1,0]" &&
             generated_variable.rows[0].terms[1].coefficient == "(-1)*(b)",
         "invariant derivative generation should preserve first-appearance target order within a "
         "row");
  Expect(generated_variable.rows[1].terms.size() == 2,
         "invariant derivative generation should preserve unique row targets in later rows");
  Expect(generated_variable.rows[1].terms[0].target.Label() == "toy_family[0,2,1]" &&
             generated_variable.rows[1].terms[0].coefficient ==
                 "(-2)*(c) + (-1)*(d)",
         "invariant derivative generation should apply the -ai rule to collected contributions");
  Expect(generated_variable.rows[1].terms[1].target.Label() == "toy_family[2,1,0]" &&
             generated_variable.rows[1].terms[1].coefficient == "(-1)*(e)",
         "invariant derivative generation should still emit repeated global targets in rows");
  Expect(generated_variable.reduction_targets.size() == 3,
         "invariant derivative generation should deduplicate the global reduction target list");
  Expect(generated_variable.reduction_targets[0].Label() == "toy_family[1,1,0]" &&
             generated_variable.reduction_targets[1].Label() == "toy_family[2,1,0]" &&
             generated_variable.reduction_targets[2].Label() == "toy_family[0,2,1]",
         "invariant derivative generation should preserve first-appearance reduction-target "
         "order");
}

void InvariantDerivativeGenerationNegativeExponentTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_family";
  master_basis.masters = {
      {"toy_family", {0, -2, 1}},
  };

  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateInvariantDerivativeVariable(master_basis, MakeInvariantGenerationHappySeed());

  Expect(generated_variable.rows.size() == 1,
         "negative-exponent invariant generation should still emit one row");
  Expect(generated_variable.rows[0].terms.size() == 2,
         "negative-exponent invariant generation should still preserve collected row targets");
  Expect(generated_variable.rows[0].terms[0].target.Label() == "toy_family[0,-2,1]" &&
             generated_variable.rows[0].terms[0].coefficient ==
                 "(2)*(c) + (-1)*(d)",
         "negative invariant exponents should flip sign under the -ai rule");
  Expect(generated_variable.rows[0].terms[1].target.Label() == "toy_family[2,-3,0]" &&
             generated_variable.rows[0].terms[1].coefficient == "(-1)*(e)",
         "negative invariant exponents should still increment then shift the selected index");
}

void InvariantDerivativeGenerationRejectsVariableKindMismatchTest() {
  amflow::InvariantDerivativeSeed seed = MakeInvariantGenerationHappySeed();
  seed.variable.kind = amflow::DifferentiationVariableKind::Eta;

  ExpectRuntimeError(
      [&seed]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            MakeInvariantGenerationHappyMasterBasis(), seed));
      },
      "requires DifferentiationVariableKind::Invariant",
      "invariant derivative generation should reject non-invariant variable metadata");
}

void InvariantDerivativeGenerationRejectsEmptyVariableNameTest() {
  amflow::InvariantDerivativeSeed seed = MakeInvariantGenerationHappySeed();
  seed.variable.name.clear();

  ExpectRuntimeError(
      [&seed]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            MakeInvariantGenerationHappyMasterBasis(), seed));
      },
      "requires a non-empty invariant variable name",
      "invariant derivative generation should reject empty invariant variable names");
}

void InvariantDerivativeGenerationRejectsEtaVariableNameTest() {
  amflow::InvariantDerivativeSeed seed = MakeInvariantGenerationHappySeed();
  seed.variable.name = "eta";

  ExpectRuntimeError(
      [&seed]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            MakeInvariantGenerationHappyMasterBasis(), seed));
      },
      "does not accept eta through the invariant seam",
      "invariant derivative generation should reject eta reuse through the invariant seam");
}

void InvariantDerivativeGenerationRejectsFactorArityMismatchTest() {
  amflow::InvariantDerivativeSeed seed = MakeInvariantGenerationHappySeed();
  seed.propagator_derivatives[0].terms[0].factor_indices = {-1, 0};

  ExpectRuntimeError(
      [&seed]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            MakeInvariantGenerationHappyMasterBasis(), seed));
      },
      "derivative factor index count to match the propagator count",
      "invariant derivative generation should reject factor-index arity mismatches");
}

void InvariantDerivativeGenerationRejectsParsedMasterListFamilyMismatchTest() {
  amflow::ParsedMasterList master_basis = MakeInvariantGenerationHappyMasterBasis();
  master_basis.family = "wrong_family";

  ExpectRuntimeError(
      [&master_basis]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            master_basis, MakeInvariantGenerationHappySeed()));
      },
      "requires ParsedMasterList.family",
      "invariant derivative generation should reject an inconsistent ParsedMasterList family "
      "header");
}

void InvariantDerivativeGenerationRejectsMasterArityMismatchTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_family";
  master_basis.masters = {
      {"toy_family", {1, 1}},
  };

  ExpectRuntimeError(
      [&master_basis]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            master_basis, MakeInvariantGenerationHappySeed()));
      },
      "master index count to match the propagator derivative count",
      "invariant derivative generation should reject masters whose arity mismatches the seed");
}

void InvariantDerivativeGenerationRejectsPropagatorDerivativeTableSizeMismatchTest() {
  amflow::InvariantDerivativeSeed seed = MakeInvariantGenerationHappySeed();
  seed.propagator_derivatives.pop_back();

  ExpectRuntimeError(
      [&seed]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            MakeInvariantGenerationHappyMasterBasis(), seed));
      },
      "propagator derivative count to match the master index count",
      "invariant derivative generation should reject propagator derivative tables with the wrong "
      "arity");
}

void InvariantDerivativeGenerationRejectsMasterFamilyMismatchTest() {
  amflow::ParsedMasterList master_basis = MakeInvariantGenerationHappyMasterBasis();
  master_basis.masters[0].family = "wrong_family";

  ExpectRuntimeError(
      [&master_basis]() {
        static_cast<void>(amflow::GenerateInvariantDerivativeVariable(
            master_basis, MakeInvariantGenerationHappySeed()));
      },
      "requires master family",
      "invariant derivative generation should reject masters from the wrong family");
}

void InvariantDerivativeGenerationDropsZeroNetCollectedTermsTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_family";
  master_basis.masters = {
      {"toy_family", {1, -1, 0}},
  };

  amflow::InvariantDerivativeSeed seed;
  seed.family = "toy_family";
  seed.variable = {"s", amflow::DifferentiationVariableKind::Invariant};
  seed.propagator_derivatives = {
      {{{"cancel", {-1, 0, 0}}}},
      {{{"cancel", {0, -1, 0}}}},
      {},
  };

  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateInvariantDerivativeVariable(master_basis, seed);

  Expect(generated_variable.rows.size() == 1,
         "zero-net invariant generation should still emit one row");
  Expect(generated_variable.rows[0].terms.empty(),
         "zero-net invariant generation should drop collected row terms whose literal "
         "coefficients cancel");
  Expect(generated_variable.reduction_targets.empty(),
         "zero-net invariant generation should not expose cancelled targets for reduction");
}

void BuildInvariantDerivativeSeedHappyPathTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  const std::string before_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::InvariantDerivativeSeed seed =
      amflow::BuildInvariantDerivativeSeed(spec, "s");

  Expect(seed.family == "toy_auto_family",
         "automatic invariant seed construction should preserve the family name");
  Expect(seed.variable.name == "s" &&
             seed.variable.kind == amflow::DifferentiationVariableKind::Invariant,
         "automatic invariant seed construction should emit invariant variable metadata");
  Expect(seed.propagator_derivatives.size() == 3,
         "automatic invariant seed construction should preserve propagator-table arity");
  Expect(seed.propagator_derivatives[0].terms.empty(),
         "automatic invariant seed construction should preserve zero-derivative propagator slots");
  Expect(seed.propagator_derivatives[1].terms.size() == 1 &&
             seed.propagator_derivatives[1].terms[0].coefficient == "1" &&
             seed.propagator_derivatives[1].terms[0].factor_indices ==
                 std::vector<int>({0, 0, 0}),
         "automatic invariant seed construction should derive constant propagator derivatives");
  Expect(seed.propagator_derivatives[2].terms.size() == 1 &&
             seed.propagator_derivatives[2].terms[0].coefficient == "-1" &&
             seed.propagator_derivatives[2].terms[0].factor_indices ==
                 std::vector<int>({-1, 0, 0}),
         "automatic invariant seed construction should map known propagator factors back to "
         "same-family indices");
  Expect(amflow::SerializeProblemSpecYaml(spec) == before_yaml,
         "automatic invariant seed construction should not mutate the input problem spec");
}

void BuildInvariantDerivativeSeedCompositionTest() {
  const amflow::InvariantDerivativeSeed seed =
      amflow::BuildInvariantDerivativeSeed(MakeAutoInvariantHappyProblemSpec(), "s");
  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateInvariantDerivativeVariable(MakeAutoInvariantHappyMasterBasis(), seed);

  Expect(generated_variable.rows.size() == 2,
         "automatic invariant seed construction should compose with invariant generation over "
         "the full master basis");
  Expect(generated_variable.rows[0].source_master.Label() == "toy_auto_family[1,1,1]" &&
             generated_variable.rows[0].terms.size() == 2,
         "automatic invariant seed construction should preserve source-master order when "
         "composed");
  Expect(generated_variable.rows[0].terms[0].target.Label() == "toy_auto_family[1,2,1]" &&
             generated_variable.rows[0].terms[0].coefficient == "-1",
         "automatic invariant seed construction should emit constant derivative targets in row "
         "order");
  Expect(generated_variable.rows[0].terms[1].target.Label() == "toy_auto_family[0,1,2]" &&
             generated_variable.rows[0].terms[1].coefficient == "(-1)*(-1)",
         "automatic invariant seed construction should compose propagator-factor terms into "
         "shifted targets");
  Expect(generated_variable.rows[1].terms.size() == 2 &&
             generated_variable.rows[1].terms[0].target.Label() == "toy_auto_family[0,0,2]" &&
             generated_variable.rows[1].terms[0].coefficient == "1" &&
             generated_variable.rows[1].terms[1].target.Label() == "toy_auto_family[-1,-1,3]" &&
             generated_variable.rows[1].terms[1].coefficient == "(-2)*(-1)",
         "automatic invariant seed construction should preserve reviewed negative-exponent "
         "generation behavior");
  Expect(generated_variable.reduction_targets.size() == 4 &&
             generated_variable.reduction_targets[0].Label() == "toy_auto_family[1,2,1]" &&
             generated_variable.reduction_targets[1].Label() == "toy_auto_family[0,1,2]" &&
             generated_variable.reduction_targets[2].Label() == "toy_auto_family[0,0,2]" &&
             generated_variable.reduction_targets[3].Label() == "toy_auto_family[-1,-1,3]",
         "automatic invariant seed construction should preserve first-appearance reduction-"
         "target order after composition");
}

void BuildInvariantDerivativeSeedAllZeroCaseTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantAllZeroProblemSpec();
  const amflow::InvariantDerivativeSeed seed =
      amflow::BuildInvariantDerivativeSeed(spec, "s");

  Expect(seed.propagator_derivatives.size() == 2 &&
             seed.propagator_derivatives[0].terms.empty() &&
             seed.propagator_derivatives[1].terms.empty(),
         "automatic invariant seed construction should preserve all-zero derivative slots");

  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_auto_zero_family";
  master_basis.masters = {
      {"toy_auto_zero_family", {1, 1}},
  };

  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateInvariantDerivativeVariable(master_basis, seed);
  Expect(generated_variable.rows.size() == 1 &&
             generated_variable.rows[0].terms.empty() &&
             generated_variable.reduction_targets.empty(),
         "all-zero automatic invariant seeds should compose to empty row terms and no reduction "
         "targets");
}

void BuildInvariantDerivativeSeedMatchesWholePropagatorFactorTest() {
  const amflow::InvariantDerivativeSeed seed =
      amflow::BuildInvariantDerivativeSeed(MakeAutoInvariantWholeFactorProblemSpec(), "s");

  Expect(seed.propagator_derivatives.size() == 3,
         "whole-factor automatic invariant seed construction should preserve propagator arity");
  Expect(seed.propagator_derivatives[2].terms.size() == 1 &&
             seed.propagator_derivatives[2].terms[0].coefficient == "-1" &&
             seed.propagator_derivatives[2].terms[0].factor_indices ==
                 std::vector<int>({0, -1, 0}),
         "automatic invariant seed construction should match whole propagator expressions before "
         "decomposing derivative factors");
}

void BuildInvariantDerivativeSeedRejectsEmptyInvariantNameTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(
            MakeAutoInvariantHappyProblemSpec(), ""));
      },
      "requires a non-empty invariant name",
      "automatic invariant seed construction should reject empty invariant names");
}

void BuildInvariantDerivativeSeedRejectsEtaInvariantNameTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(
            MakeAutoInvariantHappyProblemSpec(), "eta"));
      },
      "does not accept eta through the invariant seam",
      "automatic invariant seed construction should reject eta through the invariant seam");
}

void BuildInvariantDerivativeSeedRejectsUnknownInvariantNameTest() {
  ExpectRuntimeError(
      []() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(
            MakeAutoInvariantHappyProblemSpec(), "t"));
      },
      "requires invariant",
      "automatic invariant seed construction should reject unknown invariants");
}

void BuildInvariantDerivativeSeedRejectsMissingScalarProductRuleTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.kinematics.scalar_product_rules.pop_back();

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "requires scalar-product rule data for external pair",
      "automatic invariant seed construction should reject incomplete scalar-product data");
}

void BuildInvariantDerivativeSeedRejectsUnknownMomentumSymbolTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.family.propagators[1].expression = "(k-q)^2";

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "unknown momentum symbol: q",
      "automatic invariant seed construction should reject unknown momentum symbols");
}

void BuildInvariantDerivativeSeedRejectsUnsupportedScalarRuleGrammarTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.kinematics.scalar_product_rules[2].right = "s*s";

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "supports only linear scalar-product-rule expressions",
      "automatic invariant seed construction should reject unsupported scalar-product-rule "
      "grammar");
}

void BuildInvariantDerivativeSeedRejectsUnsupportedPropagatorKindTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.family.propagators[0].kind = amflow::PropagatorKind::Auxiliary;

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "supports Standard propagators only",
      "automatic invariant seed construction should reject unsupported propagator kinds");
}

void BuildInvariantDerivativeSeedRejectsNonzeroMassPropagatorTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.family.propagators[0].mass = "s";

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "mass == \"0\"",
      "automatic invariant seed construction should reject nonzero or invariant-dependent "
      "propagator masses in the bootstrap subset");
}

void BuildInvariantDerivativeSeedRejectsNonRepresentableDerivativeTermsTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.family.propagators[2].expression = "s*((k-p1)^2)";

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "could not represent derivative term using known propagator factors",
      "automatic invariant seed construction should reject derivatives that cannot be "
      "represented on the family propagator table");
}

void BuildInvariantDerivativeSeedRejectsNormalizedDuplicatePropagatorsTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.family.propagators[0].expression = "(k-p1)^2";
  spec.family.propagators[1].expression = "(p1-k)^2";
  spec.family.propagators[2].expression = "(-s)*((k)^2)";

  ExpectRuntimeError(
      [&spec]() {
        static_cast<void>(amflow::BuildInvariantDerivativeSeed(spec, "s"));
      },
      "requires unique propagator expressions for factor matching",
      "automatic invariant seed construction should reject sign-flipped duplicate propagator "
      "expressions");
}

void GeneratedDerivativeAssemblyHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");
  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(reduction_result.master_list,
                                            MakeEtaGeneratedHappyTransform());

  amflow::GeneratedDerivativeVariableReductionInput input;
  input.generated_variable = generated_variable;
  input.reduction_result = reduction_result;

  const amflow::DESystem system =
      amflow::AssembleGeneratedDerivativeDESystem(reduction_result.master_list, {input});
  const auto matrix_it = system.coefficient_matrices.find("eta");

  Expect(matrix_it != system.coefficient_matrices.end(),
         "generated eta assembly should populate the eta coefficient matrix");
  Expect(matrix_it->second.size() == 2 && matrix_it->second[0].size() == 2,
         "generated eta assembly should emit a full 2x2 matrix");
  Expect(matrix_it->second[0][0] == "(-1)*(2)",
         "generated eta assembly should compose the row-0 col-0 contribution literally");
  Expect(matrix_it->second[0][1] == "(-1)*(1) + (-1)*(s)",
         "generated eta assembly should preserve encounter-order sums for row-0 col-1");
  Expect(matrix_it->second[1][0] == "(-1)*(t)",
         "generated eta assembly should compose the row-1 col-0 contribution literally");
  Expect(matrix_it->second[1][1] == "(-1)*(3)",
         "generated eta assembly should compose the row-1 col-1 contribution literally");
}

void GeneratedDerivativeAssemblyRejectsMissingExplicitRuleTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");
  amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(reduction_result.master_list,
                                            MakeEtaGeneratedHappyTransform());
  generated_variable.rows[0].terms[0].target =
      amflow::TargetIntegral{"planar_double_box", {9, 9, 9, 9, 9, 9, 9}};
  generated_variable.reduction_targets[0] = generated_variable.rows[0].terms[0].target;

  amflow::GeneratedDerivativeVariableReductionInput input;
  input.generated_variable = generated_variable;
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(amflow::AssembleGeneratedDerivativeDESystem(
            reduction_result.master_list, {input}));
      },
      "missing a reduction rule for generated target",
      "generated eta assembly should reject generated targets without explicit reduction rules");
}

void GeneratedDerivativeAssemblyRejectsIdentityFallbackTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");
  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(reduction_result.master_list,
                                            MakeEtaGeneratedHappyTransform());

  amflow::GeneratedDerivativeVariableReductionInput input;
  input.generated_variable = generated_variable;
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(amflow::AssembleGeneratedDerivativeDESystem(
            reduction_result.master_list, {input}));
      },
      "requires explicit reduction rules for generated targets",
      "generated eta assembly should reject identity-fallback reductions");
}

void GeneratedDerivativeAssemblyRejectsRowCountMismatchTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");
  amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(reduction_result.master_list,
                                            MakeEtaGeneratedHappyTransform());
  generated_variable.rows.pop_back();

  amflow::GeneratedDerivativeVariableReductionInput input;
  input.generated_variable = generated_variable;
  input.reduction_result = reduction_result;

  ExpectRuntimeError(
      [&reduction_result, &input]() {
        static_cast<void>(amflow::AssembleGeneratedDerivativeDESystem(
            reduction_result.master_list, {input}));
      },
      "generated row count must match assembly master count",
      "generated eta assembly should reject row-count mismatches");
}

void GeneratedDerivativeAssemblyRejectsDuplicateVariableNamesTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(root, "planar_double_box");
  const amflow::GeneratedDerivativeVariable generated_variable =
      amflow::GenerateEtaDerivativeVariable(reduction_result.master_list,
                                            MakeEtaGeneratedHappyTransform());

  amflow::GeneratedDerivativeVariableReductionInput first_input;
  first_input.generated_variable = generated_variable;
  first_input.reduction_result = reduction_result;

  amflow::GeneratedDerivativeVariableReductionInput second_input = first_input;
  second_input.generated_variable.variable.kind =
      amflow::DifferentiationVariableKind::Invariant;

  ExpectRuntimeError(
      [&reduction_result, &first_input, &second_input]() {
        static_cast<void>(amflow::AssembleGeneratedDerivativeDESystem(
            reduction_result.master_list, {first_input, second_input}));
      },
      "duplicate differentiation variable name: eta",
      "generated eta assembly should reject duplicate variable names");
}

void PrepareEtaGeneratedReductionHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-generated-prepare"));

  const amflow::EtaGeneratedReductionPreparation preparation =
      amflow::PrepareEtaGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                           master_basis,
                                           MakeEtaGeneratedHappyDecision(),
                                           MakeKiraReductionOptions(),
                                           layout);

  Expect(preparation.generated_variable.reduction_targets.size() == 3,
         "eta-generated Kira preparation should preserve the reviewed generated target count");
  const auto target_it = preparation.backend_preparation.generated_files.find("target");
  Expect(target_it != preparation.backend_preparation.generated_files.end(),
         "eta-generated Kira preparation should emit the explicit target file");
  Expect(target_it->second ==
             "planar_double_box[2,1,1,1,1,1,1]\n"
             "planar_double_box[1,1,1,1,1,1,2]\n"
             "planar_double_box[2,1,1,1,1,1,0]\n",
         "eta-generated Kira preparation should preserve generated target order exactly");

  const auto family_yaml_it =
      preparation.backend_preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != preparation.backend_preparation.generated_files.end(),
         "eta-generated Kira preparation should emit family YAML");
  Expect(family_yaml_it->second.find("\"((k1)^2) + eta\"") != std::string::npos &&
             family_yaml_it->second.find("\"((k2-p1-p2)^2) + eta\"") !=
                 std::string::npos,
         "eta-generated Kira preparation should emit eta-shifted propagators");

  const auto kinematics_yaml_it =
      preparation.backend_preparation.generated_files.find("config/kinematics.yaml");
  Expect(kinematics_yaml_it != preparation.backend_preparation.generated_files.end(),
         "eta-generated Kira preparation should emit kinematics YAML");
  Expect(kinematics_yaml_it->second.find("\"eta\"") != std::string::npos,
         "eta-generated Kira preparation should include eta in the invariants");
}

void PrepareEtaGeneratedReductionRejectsEmptyGeneratedTargetsTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "planar_double_box";
  master_basis.masters = {
      {"planar_double_box", {0, 1, 1, 1, 1, 1, 0}},
      {"planar_double_box", {0, 1, 1, 1, 1, 1, 0}},
  };
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-empty-targets"));
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::EtaInsertionDecision decision = MakeEtaGeneratedHappyDecision();

  ExpectRuntimeError(
      [&spec, &master_basis, &decision, &layout]() {
        static_cast<void>(amflow::PrepareEtaGeneratedReduction(
            spec, master_basis, decision, MakeKiraReductionOptions(), layout));
      },
      "requires at least one generated reduction target",
      "eta-generated Kira preparation should reject empty generated target lists");
}

void PrepareEtaGeneratedReductionFakeExecutionSmokeTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-generated-exec"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        "#!/bin/sh\n"
                        "echo \"eta-generated:$1\"\n"
                        "exit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::EtaInsertionDecision decision = MakeEtaGeneratedHappyDecision();

  const amflow::EtaGeneratedReductionPreparation preparation =
      amflow::PrepareEtaGeneratedReduction(
          spec, master_basis, decision, MakeKiraReductionOptions(), layout);
  const amflow::CommandExecutionResult result = backend.ExecutePrepared(
      preparation.backend_preparation, layout, kira_path, fermat_path);

  Expect(result.Succeeded(),
         "eta-generated Kira preparation should feed cleanly into fake execution");
  Expect(std::filesystem::exists(result.stdout_log_path),
         "eta-generated fake execution should materialize stdout logs");
  Expect(ReadFile(result.stdout_log_path).find("eta-generated:") != std::string::npos,
         "eta-generated fake execution should preserve fake reducer stdout");
}

void PrepareInvariantGeneratedReductionAutomaticHappyPathTest() {
  amflow::KiraBackend backend;
  const amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-prepare"));
  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-prepare-baseline"));
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::BackendPreparation baseline_preparation =
      backend.Prepare(spec, MakeKiraReductionOptions(), baseline_layout);

  const amflow::InvariantGeneratedReductionPreparation preparation =
      amflow::PrepareInvariantGeneratedReduction(
          spec, master_basis, "s", MakeKiraReductionOptions(), layout);

  Expect(preparation.generated_variable.reduction_targets.size() == 4,
         "automatic invariant-generated preparation should expose the reviewed target count");
  Expect(preparation.generated_variable.reduction_targets[0].Label() == "toy_auto_family[1,2,1]" &&
             preparation.generated_variable.reduction_targets[1].Label() ==
                 "toy_auto_family[0,1,2]" &&
             preparation.generated_variable.reduction_targets[2].Label() ==
                 "toy_auto_family[0,0,2]" &&
             preparation.generated_variable.reduction_targets[3].Label() ==
                 "toy_auto_family[-1,-1,3]",
         "automatic invariant-generated preparation should preserve first-appearance target "
         "order from invariant generation");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "automatic invariant-generated preparation should not mutate the input problem spec");
  Expect(preparation.backend_preparation.validation_messages.empty(),
         "automatic invariant-generated preparation should preserve an empty validation result "
         "for the reviewed happy path");

  const auto target_it = preparation.backend_preparation.generated_files.find("target");
  Expect(target_it != preparation.backend_preparation.generated_files.end(),
         "automatic invariant-generated preparation should emit the explicit target file");
  Expect(target_it->second ==
             "toy_auto_family[1,2,1]\n"
             "toy_auto_family[0,1,2]\n"
             "toy_auto_family[0,0,2]\n"
             "toy_auto_family[-1,-1,3]\n",
         "automatic invariant-generated preparation should preserve exact generated target "
         "order in the target file");

  const auto family_yaml_it =
      preparation.backend_preparation.generated_files.find("config/integralfamilies.yaml");
  const auto baseline_family_yaml_it =
      baseline_preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != preparation.backend_preparation.generated_files.end() &&
             baseline_family_yaml_it != baseline_preparation.generated_files.end(),
         "automatic invariant-generated preparation should emit family YAML");
  Expect(family_yaml_it->second == baseline_family_yaml_it->second,
         "automatic invariant-generated preparation should preserve the original family YAML");

  const auto kinematics_yaml_it =
      preparation.backend_preparation.generated_files.find("config/kinematics.yaml");
  const auto baseline_kinematics_yaml_it =
      baseline_preparation.generated_files.find("config/kinematics.yaml");
  Expect(kinematics_yaml_it != preparation.backend_preparation.generated_files.end() &&
             baseline_kinematics_yaml_it != baseline_preparation.generated_files.end(),
         "automatic invariant-generated preparation should emit kinematics YAML");
  Expect(kinematics_yaml_it->second == baseline_kinematics_yaml_it->second,
         "automatic invariant-generated preparation should preserve the original kinematics "
         "YAML");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsEmptyGeneratedTargetsTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_auto_zero_family";
  master_basis.masters = {
      {"toy_auto_zero_family", {1, 1}},
  };
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-empty-targets"));

  ExpectRuntimeError(
      [&master_basis, &layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            MakeAutoInvariantAllZeroProblemSpec(),
            master_basis,
            "s",
            MakeKiraReductionOptions(),
            layout));
      },
      "requires at least one generated reduction target",
      "automatic invariant-generated preparation should reject all-zero generated target lists");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsEmptyInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-empty-name"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "",
            MakeKiraReductionOptions(),
            layout));
      },
      "requires a non-empty invariant name",
      "automatic invariant-generated preparation should preserve the seed-builder empty-name "
      "diagnostic");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsEtaInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-eta-name"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "eta",
            MakeKiraReductionOptions(),
            layout));
      },
      "does not accept eta through the invariant seam",
      "automatic invariant-generated preparation should preserve the seed-builder eta "
      "diagnostic");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsUnknownInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-unknown-name"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "t",
            MakeKiraReductionOptions(),
            layout));
      },
      "requires invariant",
      "automatic invariant-generated preparation should preserve the seed-builder unknown-"
      "invariant diagnostic");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsUnsupportedScalarRuleGrammarTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.kinematics.scalar_product_rules[2].right = "s*s";
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-unsupported-scalar-rule"));

  ExpectRuntimeError(
      [&spec, &layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            spec,
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout));
      },
      "supports only linear scalar-product-rule expressions",
      "automatic invariant-generated preparation should preserve the seed-builder unsupported "
      "scalar-product-rule grammar diagnostic");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsNonzeroMassPropagatorTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.family.propagators[0].mass = "s";
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-mass"));

  ExpectRuntimeError(
      [&spec, &layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            spec,
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout));
      },
      "mass == \"0\"",
      "automatic invariant-generated preparation should preserve the seed-builder nonzero-mass "
      "diagnostic");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsMasterBasisFamilyMismatchTest() {
  amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  master_basis.family = "wrong_family";
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-family-mismatch"));

  ExpectRuntimeError(
      [&master_basis, &layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            master_basis,
            "s",
            MakeKiraReductionOptions(),
            layout));
      },
      "requires ParsedMasterList.family",
      "automatic invariant-generated preparation should preserve master-basis family mismatch "
      "diagnostics");
}

void PrepareInvariantGeneratedReductionAutomaticRejectsMasterBasisArityMismatchTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_auto_family";
  master_basis.masters = {
      {"toy_auto_family", {1, 1}},
  };
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-arity-mismatch"));

  ExpectRuntimeError(
      [&master_basis, &layout]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            master_basis,
            "s",
            MakeKiraReductionOptions(),
            layout));
      },
      "master index count to match the propagator derivative count",
      "automatic invariant-generated preparation should preserve master-basis arity mismatch "
      "diagnostics");
}

void PrepareInvariantGeneratedReductionAutomaticFakeExecutionSmokeTest() {
  amflow::KiraBackend backend;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-exec"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        "#!/bin/sh\n"
                        "echo \"invariant-auto:$1\"\n"
                        "exit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::InvariantGeneratedReductionPreparation preparation =
      amflow::PrepareInvariantGeneratedReduction(MakeAutoInvariantHappyProblemSpec(),
                                                 MakeAutoInvariantHappyMasterBasis(),
                                                 "s",
                                                 MakeKiraReductionOptions(),
                                                 layout);
  const amflow::CommandExecutionResult result = backend.ExecutePrepared(
      preparation.backend_preparation, layout, kira_path, fermat_path);

  Expect(result.Succeeded(),
         "automatic invariant-generated preparation should feed cleanly into fake execution");
  Expect(std::filesystem::exists(result.stdout_log_path),
         "automatic invariant-generated fake execution should materialize stdout logs");
  Expect(ReadFile(result.stdout_log_path).find("invariant-auto:") != std::string::npos,
         "automatic invariant-generated fake execution should preserve fake reducer stdout");
}

void PrepareInvariantGeneratedReductionHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-invariant-generated-prepare"));
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::InvariantGeneratedReductionPreparation preparation =
      amflow::PrepareInvariantGeneratedReduction(spec,
                                                 master_basis,
                                                 MakeInvariantPreparationHappySeed(),
                                                 MakeKiraReductionOptions(),
                                                 layout);

  Expect(preparation.generated_variable.reduction_targets.size() == 4,
         "invariant-generated Kira preparation should preserve the reviewed generated target "
         "count");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "invariant-generated Kira preparation should not mutate the input problem spec");
  const auto target_it = preparation.backend_preparation.generated_files.find("target");
  Expect(target_it != preparation.backend_preparation.generated_files.end(),
         "invariant-generated Kira preparation should emit the explicit target file");
  Expect(target_it->second ==
             "planar_double_box[1,1,1,1,1,1,1]\n"
             "planar_double_box[2,1,1,1,1,1,1]\n"
             "planar_double_box[1,1,1,1,1,1,0]\n"
             "planar_double_box[2,1,1,1,1,1,0]\n",
         "invariant-generated Kira preparation should preserve generated target order exactly");

  const auto family_yaml_it =
      preparation.backend_preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != preparation.backend_preparation.generated_files.end(),
         "invariant-generated Kira preparation should emit family YAML");
  Expect(family_yaml_it->second.find("\"(k1)^2\"") != std::string::npos &&
             family_yaml_it->second.find("\"(k2-p1-p2)^2\"") != std::string::npos &&
             family_yaml_it->second.find("+ eta") == std::string::npos,
         "invariant-generated Kira preparation should preserve the original family YAML");

  const auto kinematics_yaml_it =
      preparation.backend_preparation.generated_files.find("config/kinematics.yaml");
  Expect(kinematics_yaml_it != preparation.backend_preparation.generated_files.end(),
         "invariant-generated Kira preparation should emit kinematics YAML");
  Expect(kinematics_yaml_it->second.find("\"s\"") != std::string::npos &&
             kinematics_yaml_it->second.find("\"t\"") != std::string::npos &&
             kinematics_yaml_it->second.find("\"msq\"") != std::string::npos &&
             kinematics_yaml_it->second.find("\"eta\"") == std::string::npos,
         "invariant-generated Kira preparation should preserve the original kinematics YAML");
}

void PrepareInvariantGeneratedReductionRejectsEmptyGeneratedTargetsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-empty-targets"));
  amflow::InvariantDerivativeSeed seed = MakeInvariantPreparationHappySeed();
  seed.propagator_derivatives.assign(7, {});

  ExpectRuntimeError(
      [&master_basis, &layout, &seed]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                                     master_basis,
                                                                     seed,
                                                                     MakeKiraReductionOptions(),
                                                                     layout));
      },
      "requires at least one generated reduction target",
      "invariant-generated Kira preparation should reject empty generated target lists");
}

void PrepareInvariantGeneratedReductionRejectsSpecSeedFamilyMismatchTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-family-mismatch"));
  amflow::InvariantDerivativeSeed seed = MakeInvariantPreparationHappySeed();
  seed.family = "wrong_family";

  ExpectRuntimeError(
      [&master_basis, &layout, &seed]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                                     master_basis,
                                                                     seed,
                                                                     MakeKiraReductionOptions(),
                                                                     layout));
      },
      "requires seed.family to match spec.family.name",
      "invariant-generated Kira preparation should reject spec and seed family mismatch");
}

void PrepareInvariantGeneratedReductionRejectsSpecSeedArityMismatchTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-arity-mismatch"));
  amflow::InvariantDerivativeSeed seed = MakeInvariantPreparationHappySeed();
  seed.propagator_derivatives.pop_back();

  ExpectRuntimeError(
      [&master_basis, &layout, &seed]() {
        static_cast<void>(amflow::PrepareInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                                     master_basis,
                                                                     seed,
                                                                     MakeKiraReductionOptions(),
                                                                     layout));
      },
      "requires propagator derivative count to match spec.family.propagators size",
      "invariant-generated Kira preparation should reject spec and seed arity mismatch");
}

void PrepareInvariantGeneratedReductionFakeExecutionSmokeTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(root, "planar_double_box");
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-invariant-generated-exec"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        "#!/bin/sh\n"
                        "echo \"invariant-generated:$1\"\n"
                        "exit 0\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::InvariantGeneratedReductionPreparation preparation =
      amflow::PrepareInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                 master_basis,
                                                 MakeInvariantPreparationHappySeed(),
                                                 MakeKiraReductionOptions(),
                                                 layout);
  const amflow::CommandExecutionResult result = backend.ExecutePrepared(
      preparation.backend_preparation, layout, kira_path, fermat_path);

  Expect(result.Succeeded(),
         "invariant-generated Kira preparation should feed cleanly into fake execution");
  Expect(std::filesystem::exists(result.stdout_log_path),
         "invariant-generated fake execution should materialize stdout logs");
  Expect(ReadFile(result.stdout_log_path).find("invariant-generated:") != std::string::npos,
         "invariant-generated fake execution should preserve fake reducer stdout");
}

void RunEtaGeneratedReductionHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-generated-wrapper"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::EtaGeneratedReductionExecution execution =
      amflow::RunEtaGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                       master_basis,
                                       MakeEtaGeneratedHappyDecision(),
                                       MakeKiraReductionOptions(),
                                       layout,
                                       kira_path,
                                       fermat_path);

  Expect(execution.execution_result.Succeeded(),
         "eta-generated wrapper should preserve successful reducer execution");
  Expect(execution.parsed_reduction_result.has_value(),
         "eta-generated wrapper should expose the parsed reduction result on success");
  Expect(execution.assembled_system.has_value(),
         "eta-generated wrapper should expose the assembled system on success");
  Expect(execution.parsed_reduction_result->master_list.source_path ==
             layout.generated_config_dir / "results/planar_double_box/masters",
         "eta-generated wrapper should parse results from the reducer working-directory artifact "
         "root");
  Expect(execution.preparation.backend_preparation.generated_files.at("target") ==
             "planar_double_box[2,1,1,1,1,1,1]\n"
             "planar_double_box[1,1,1,1,1,1,2]\n"
             "planar_double_box[2,1,1,1,1,1,0]\n",
         "eta-generated wrapper should preserve generated-target order through preparation");
  Expect(execution.parsed_reduction_result->status == amflow::ParsedReductionStatus::ParsedRules,
         "eta-generated wrapper should parse explicit Kira rules on the happy path");
  Expect(execution.assembled_system->variables.size() == 1 &&
             execution.assembled_system->variables.front().name == "eta",
         "eta-generated wrapper should assemble a single eta variable");
  const auto matrix_it = execution.assembled_system->coefficient_matrices.find("eta");
  Expect(matrix_it != execution.assembled_system->coefficient_matrices.end(),
         "eta-generated wrapper should populate the eta matrix");
  Expect(matrix_it->second[0][0] == "(-1)*(2)" &&
             matrix_it->second[0][1] == "(-1)*(1) + (-1)*(s)" &&
             matrix_it->second[1][0] == "(-1)*(t)" &&
             matrix_it->second[1][1] == "(-1)*(3)",
         "eta-generated wrapper should preserve the reviewed eta matrix entries");
}

void RunEtaGeneratedReductionExecutionFailureTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-wrapper-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-wrapper-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::EtaGeneratedReductionExecution execution =
      amflow::RunEtaGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                       master_basis,
                                       MakeEtaGeneratedHappyDecision(),
                                       MakeKiraReductionOptions(),
                                       layout,
                                       kira_path,
                                       fermat_path);

  Expect(execution.execution_result.status == amflow::CommandExecutionStatus::Completed,
         "eta-generated wrapper should surface reducer process failures as completed exits");
  Expect(execution.execution_result.exit_code == 9,
         "eta-generated wrapper should preserve the reducer exit code");
  Expect(!execution.parsed_reduction_result.has_value(),
         "eta-generated wrapper should not parse results after reducer execution failure");
  Expect(!execution.assembled_system.has_value(),
         "eta-generated wrapper should not assemble a system after reducer execution failure");
  Expect(ReadFile(execution.execution_result.stderr_log_path).find("expected-wrapper-failure") !=
             std::string::npos,
         "eta-generated wrapper should preserve reducer stderr on execution failure");
}

void RunEtaGeneratedReductionRejectsIdentityFallbackResultsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path master_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const std::filesystem::path missing_rule_root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(master_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-wrapper-missing-rule"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-missing-rule.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(missing_rule_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&master_basis, &layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::RunEtaGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                           master_basis,
                                                           MakeEtaGeneratedHappyDecision(),
                                                           MakeKiraReductionOptions(),
                                                           layout,
                                                           kira_path,
                                                           fermat_path));
      },
      "requires explicit reduction rules for generated targets",
      "eta-generated wrapper should reject identity-fallback reduction results");
}

void BuildEtaGeneratedDESystemHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-desystem-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::EtaGeneratedReductionExecution baseline_execution =
      amflow::RunEtaGeneratedReduction(spec,
                                       master_basis,
                                       MakeEtaGeneratedHappyDecision(),
                                       MakeKiraReductionOptions(),
                                       baseline_layout,
                                       baseline_kira_path,
                                       baseline_fermat_path);
  Expect(baseline_execution.assembled_system.has_value(),
         "eta-generated DE consumer baseline should assemble a DESystem");

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-generated-desystem"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "eta-generated DE consumer should not mutate the input problem spec");
  Expect(amflow::ValidateDESystem(system).empty(),
         "eta-generated DE consumer should return a valid DESystem");
  Expect(system.masters.size() == baseline_execution.assembled_system->masters.size() &&
             system.masters.size() == 2 &&
             system.masters[0].label == "planar_double_box[1,1,1,1,1,1,1]" &&
             system.masters[1].label == "planar_double_box[1,1,1,1,1,1,0]",
         "eta-generated DE consumer should preserve the reviewed eta master basis");
  Expect(system.variables.size() == baseline_execution.assembled_system->variables.size() &&
             system.variables.size() == 1 &&
             system.variables.front().name == "eta" &&
             system.variables.front().kind == amflow::DifferentiationVariableKind::Eta,
         "eta-generated DE consumer should preserve the reviewed eta variable");
  const auto matrix_it = system.coefficient_matrices.find("eta");
  Expect(matrix_it != system.coefficient_matrices.end(),
         "eta-generated DE consumer should populate the eta coefficient matrix");
  Expect(matrix_it->second ==
                 baseline_execution.assembled_system->coefficient_matrices.at("eta") &&
             matrix_it->second[0][0] == "(-1)*(2)" &&
             matrix_it->second[0][1] == "(-1)*(1) + (-1)*(s)" &&
             matrix_it->second[1][0] == "(-1)*(t)" &&
             matrix_it->second[1][1] == "(-1)*(3)",
         "eta-generated DE consumer should return the reviewed eta matrix entries unchanged");
}

void BuildEtaGeneratedDESystemExecutionFailureTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-desystem-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-eta-desystem-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  try {
    static_cast<void>(amflow::BuildEtaGeneratedDESystem(amflow::MakeSampleProblemSpec(),
                                                        master_basis,
                                                        MakeEtaGeneratedHappyDecision(),
                                                        MakeKiraReductionOptions(),
                                                        layout,
                                                        kira_path,
                                                        fermat_path));
    throw std::runtime_error("eta-generated DE consumer should throw after reducer execution "
                             "failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "eta-generated DE consumer should report reducer execution failure");
    Expect(message.find("status=completed") != std::string::npos,
           "eta-generated DE consumer should preserve the execution status in the failure "
           "message");
    Expect(message.find("exit_code=9") != std::string::npos,
           "eta-generated DE consumer should preserve the reducer exit code in the failure "
           "message");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "eta-generated DE consumer should preserve the stderr log path in the failure "
           "message");
  }
}

void BuildEtaGeneratedDESystemRejectsIdentityFallbackResultsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path master_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const std::filesystem::path missing_rule_root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(master_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-desystem-missing-rule"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-missing-rule.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(missing_rule_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&master_basis, &layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::BuildEtaGeneratedDESystem(amflow::MakeSampleProblemSpec(),
                                                            master_basis,
                                                            MakeEtaGeneratedHappyDecision(),
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            kira_path,
                                                            fermat_path));
      },
      "requires explicit reduction rules for generated targets",
      "eta-generated DE consumer should preserve identity-fallback parse and assembly "
      "diagnostics");
}

void BuildEtaGeneratedDESystemRejectsEmptyGeneratedTargetsTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "planar_double_box";
  master_basis.masters = {
      {"planar_double_box", {0, 1, 1, 1, 1, 1, 0}},
      {"planar_double_box", {0, 1, 1, 1, 1, 1, 0}},
  };
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-generated-desystem-empty-targets"));
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::EtaInsertionDecision decision = MakeEtaGeneratedHappyDecision();

  ExpectRuntimeError(
      [&spec, &master_basis, &decision, &layout]() {
        static_cast<void>(amflow::BuildEtaGeneratedDESystem(spec,
                                                            master_basis,
                                                            decision,
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            layout.root / "bin" / "unused-kira.sh",
                                                            layout.root /
                                                                "bin" / "unused-fermat.sh"));
      },
      "requires at least one generated reduction target",
      "eta-generated DE consumer should preserve empty generated-target diagnostics");
}

void RunInvariantGeneratedReductionHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-wrapper"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::InvariantGeneratedReductionExecution execution =
      amflow::RunInvariantGeneratedReduction(spec,
                                             master_basis,
                                             MakeInvariantExecutionHappySeed(),
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path);

  Expect(execution.execution_result.Succeeded(),
         "invariant-generated wrapper should preserve successful reducer execution");
  Expect(execution.parsed_reduction_result.has_value(),
         "invariant-generated wrapper should expose the parsed reduction result on success");
  Expect(execution.assembled_system.has_value(),
         "invariant-generated wrapper should expose the assembled system on success");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "invariant-generated wrapper should not mutate the input problem spec");
  Expect(execution.parsed_reduction_result->master_list.source_path ==
             execution.execution_result.working_directory / "results/planar_double_box/masters" &&
             execution.execution_result.working_directory == layout.generated_config_dir,
         "invariant-generated wrapper should parse results from the recorded reducer "
         "working-directory artifact root");
  Expect(execution.preparation.backend_preparation.generated_files.at("target") ==
             "planar_double_box[2,1,1,1,1,1,1]\n"
             "planar_double_box[1,1,1,1,1,1,2]\n"
             "planar_double_box[2,1,1,1,1,1,0]\n",
         "invariant-generated wrapper should preserve generated-target order through preparation");
  const auto family_yaml_it =
      execution.preparation.backend_preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != execution.preparation.backend_preparation.generated_files.end(),
         "invariant-generated wrapper should preserve emitted family YAML");
  Expect(family_yaml_it->second.find("+ eta") == std::string::npos,
         "invariant-generated wrapper should not eta-shift the family YAML");
  const auto kinematics_yaml_it =
      execution.preparation.backend_preparation.generated_files.find("config/kinematics.yaml");
  Expect(kinematics_yaml_it != execution.preparation.backend_preparation.generated_files.end(),
         "invariant-generated wrapper should preserve emitted kinematics YAML");
  Expect(kinematics_yaml_it->second.find("\"s\"") != std::string::npos &&
             kinematics_yaml_it->second.find("\"t\"") != std::string::npos &&
             kinematics_yaml_it->second.find("\"msq\"") != std::string::npos &&
             kinematics_yaml_it->second.find("\"eta\"") == std::string::npos,
         "invariant-generated wrapper should preserve original invariants without eta insertion");
  Expect(execution.parsed_reduction_result->status == amflow::ParsedReductionStatus::ParsedRules &&
             execution.parsed_reduction_result->explicit_rule_count == 3 &&
             execution.parsed_reduction_result->rules.size() == 5,
         "invariant-generated wrapper should parse explicit Kira rules on the happy path");
  Expect(execution.assembled_system->variables.size() == 1 &&
             execution.assembled_system->variables.front().name == "s" &&
             execution.assembled_system->variables.front().kind ==
                 amflow::DifferentiationVariableKind::Invariant,
         "invariant-generated wrapper should assemble a single invariant variable");
  const auto matrix_it = execution.assembled_system->coefficient_matrices.find("s");
  Expect(matrix_it != execution.assembled_system->coefficient_matrices.end(),
         "invariant-generated wrapper should populate the invariant matrix");
  Expect(matrix_it->second[0][0] == "(-1)*(2)" &&
             matrix_it->second[0][1] == "(-1)*(1) + (-1)*(s)" &&
             matrix_it->second[1][0] == "(-1)*(t)" &&
             matrix_it->second[1][1] == "(-1)*(3)",
         "invariant-generated wrapper should preserve the reviewed invariant matrix entries");
}

void RunInvariantGeneratedReductionExecutionFailureTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-wrapper-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-invariant-wrapper-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::InvariantGeneratedReductionExecution execution =
      amflow::RunInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                             master_basis,
                                             MakeInvariantExecutionHappySeed(),
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path);

  Expect(execution.execution_result.status == amflow::CommandExecutionStatus::Completed,
         "invariant-generated wrapper should surface reducer process failures as completed exits");
  Expect(execution.execution_result.exit_code == 9,
         "invariant-generated wrapper should preserve the reducer exit code");
  Expect(!execution.parsed_reduction_result.has_value(),
         "invariant-generated wrapper should not parse results after reducer execution failure");
  Expect(!execution.assembled_system.has_value(),
         "invariant-generated wrapper should not assemble a system after reducer execution "
         "failure");
  Expect(ReadFile(execution.execution_result.stderr_log_path).find(
             "expected-invariant-wrapper-failure") != std::string::npos,
         "invariant-generated wrapper should preserve reducer stderr on execution failure");
}

void RunInvariantGeneratedReductionRejectsIdentityFallbackResultsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path master_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const std::filesystem::path missing_rule_root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(master_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-wrapper-missing-rule"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-missing-rule.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(missing_rule_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&master_basis, &layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                                 master_basis,
                                                                 MakeInvariantExecutionHappySeed(),
                                                                 MakeKiraReductionOptions(),
                                                                 layout,
                                                                 kira_path,
                                                                 fermat_path));
      },
      "requires explicit reduction rules for generated targets",
      "invariant-generated wrapper should reject identity-fallback reduction results");
}

void RunInvariantGeneratedReductionRejectsMissingExplicitRuleForMasterTargetTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-generated-wrapper-master-target"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&master_basis, &layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(amflow::MakeSampleProblemSpec(),
                                                                 master_basis,
                                                                 MakeInvariantPreparationHappySeed(),
                                                                 MakeKiraReductionOptions(),
                                                                 layout,
                                                                 kira_path,
                                                                 fermat_path));
      },
      "missing a reduction rule for generated target",
      "invariant-generated wrapper should require explicit rules even for generated master "
      "targets");
}

void RunInvariantGeneratedReductionAutomaticHappyPathTest() {
  amflow::KiraBackend backend;
  const amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper"));
  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-baseline"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::BackendPreparation baseline_preparation =
      backend.Prepare(spec, MakeKiraReductionOptions(), baseline_layout);

  const amflow::InvariantGeneratedReductionExecution execution =
      amflow::RunInvariantGeneratedReduction(spec,
                                             master_basis,
                                             "s",
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path);

  Expect(execution.execution_result.Succeeded(),
         "automatic invariant-generated wrapper should preserve successful reducer execution");
  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "automatic invariant-generated wrapper should not mutate the input problem spec");
  Expect(execution.execution_result.working_directory == layout.generated_config_dir,
         "automatic invariant-generated wrapper should record the reducer working-directory "
         "artifact root");
  Expect(execution.parsed_reduction_result.has_value(),
         "automatic invariant-generated wrapper should expose the parsed reduction result on "
         "success");
  Expect(execution.assembled_system.has_value(),
         "automatic invariant-generated wrapper should expose the assembled system on success");
  Expect(execution.parsed_reduction_result->master_list.source_path ==
             execution.execution_result.working_directory / "results/toy_auto_family/masters",
         "automatic invariant-generated wrapper should parse results from the recorded "
         "working-directory artifact root");
  Expect(execution.preparation.backend_preparation.generated_files.at("target") ==
             "toy_auto_family[1,2,1]\n"
             "toy_auto_family[0,1,2]\n"
             "toy_auto_family[0,0,2]\n"
             "toy_auto_family[-1,-1,3]\n",
         "automatic invariant-generated wrapper should preserve generated-target order through "
         "preparation");

  const auto family_yaml_it =
      execution.preparation.backend_preparation.generated_files.find("config/integralfamilies.yaml");
  const auto baseline_family_yaml_it =
      baseline_preparation.generated_files.find("config/integralfamilies.yaml");
  Expect(family_yaml_it != execution.preparation.backend_preparation.generated_files.end() &&
             baseline_family_yaml_it != baseline_preparation.generated_files.end(),
         "automatic invariant-generated wrapper should preserve emitted family YAML");
  Expect(family_yaml_it->second == baseline_family_yaml_it->second,
         "automatic invariant-generated wrapper should preserve the original family YAML");

  const auto kinematics_yaml_it =
      execution.preparation.backend_preparation.generated_files.find("config/kinematics.yaml");
  const auto baseline_kinematics_yaml_it =
      baseline_preparation.generated_files.find("config/kinematics.yaml");
  Expect(kinematics_yaml_it != execution.preparation.backend_preparation.generated_files.end() &&
             baseline_kinematics_yaml_it != baseline_preparation.generated_files.end(),
         "automatic invariant-generated wrapper should preserve emitted kinematics YAML");
  Expect(kinematics_yaml_it->second == baseline_kinematics_yaml_it->second,
         "automatic invariant-generated wrapper should preserve the original kinematics YAML");

  Expect(execution.parsed_reduction_result->status == amflow::ParsedReductionStatus::ParsedRules &&
             execution.parsed_reduction_result->explicit_rule_count == 4 &&
             execution.parsed_reduction_result->rules.size() == 6,
         "automatic invariant-generated wrapper should parse explicit Kira rules on the happy "
         "path");
  Expect(execution.assembled_system->variables.size() == 1 &&
             execution.assembled_system->variables.front().name == "s" &&
             execution.assembled_system->variables.front().kind ==
                 amflow::DifferentiationVariableKind::Invariant,
         "automatic invariant-generated wrapper should assemble a single invariant variable");
  const auto matrix_it = execution.assembled_system->coefficient_matrices.find("s");
  Expect(matrix_it != execution.assembled_system->coefficient_matrices.end(),
         "automatic invariant-generated wrapper should populate the invariant matrix");
  Expect(matrix_it->second[0][0] == "(-1)*(2) + ((-1)*(-1))*(5)" &&
             matrix_it->second[0][1] == "(-1)*(3) + ((-1)*(-1))*(7)" &&
             matrix_it->second[1][0] == "(1)*(19) + ((-2)*(-1))*(13)" &&
             matrix_it->second[1][1] == "(1)*(11) + ((-2)*(-1))*(17)",
         "automatic invariant-generated wrapper should preserve the reviewed invariant matrix "
         "entries");
}

void RunInvariantGeneratedReductionAutomaticExecutionFailureTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-auto-invariant-wrapper-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::InvariantGeneratedReductionExecution execution =
      amflow::RunInvariantGeneratedReduction(MakeAutoInvariantHappyProblemSpec(),
                                             MakeAutoInvariantHappyMasterBasis(),
                                             "s",
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path);

  Expect(execution.execution_result.status == amflow::CommandExecutionStatus::Completed,
         "automatic invariant-generated wrapper should surface reducer process failures as "
         "completed exits");
  Expect(execution.execution_result.exit_code == 9,
         "automatic invariant-generated wrapper should preserve the reducer exit code");
  Expect(!execution.parsed_reduction_result.has_value(),
         "automatic invariant-generated wrapper should not parse results after reducer execution "
         "failure");
  Expect(!execution.assembled_system.has_value(),
         "automatic invariant-generated wrapper should not assemble a system after reducer "
         "execution failure");
  Expect(ReadFile(execution.execution_result.stderr_log_path).find(
             "expected-auto-invariant-wrapper-failure") != std::string::npos,
         "automatic invariant-generated wrapper should preserve reducer stderr on execution "
         "failure");
}

void RunInvariantGeneratedReductionAutomaticRejectsEmptyInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-empty-name"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "",
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "requires a non-empty invariant name",
      "automatic invariant-generated wrapper should preserve the seed-builder empty-name "
      "diagnostic");
}

void RunInvariantGeneratedReductionAutomaticRejectsUnsupportedScalarRuleGrammarTest() {
  amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  spec.kinematics.scalar_product_rules[2].right = "s*s";
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-bad-scalar-rule"));

  ExpectRuntimeError(
      [&spec, &layout]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(
            spec,
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "supports only linear scalar-product-rule expressions",
      "automatic invariant-generated wrapper should preserve unsupported scalar-rule grammar "
      "diagnostics");
}

void RunInvariantGeneratedReductionAutomaticRejectsMasterBasisFamilyMismatchTest() {
  amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  master_basis.family = "wrong_family";
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-family-mismatch"));

  ExpectRuntimeError(
      [&master_basis, &layout]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            master_basis,
            "s",
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "requires ParsedMasterList.family",
      "automatic invariant-generated wrapper should preserve master-basis family mismatch "
      "diagnostics");
}

void RunInvariantGeneratedReductionAutomaticRejectsEmptyGeneratedTargetsTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "toy_auto_zero_family";
  master_basis.masters = {
      {"toy_auto_zero_family", {1, 1}},
  };
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-empty-targets"));

  ExpectRuntimeError(
      [&master_basis, &layout]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(
            MakeAutoInvariantAllZeroProblemSpec(),
            master_basis,
            "s",
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "requires at least one generated reduction target",
      "automatic invariant-generated wrapper should reject all-zero generated target lists");
}

void RunInvariantGeneratedReductionAutomaticRejectsIdentityFallbackResultsTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-missing-rule"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-missing-rule.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeAutoInvariantResultScript(false, ""));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout,
            kira_path,
            fermat_path));
      },
      "requires explicit reduction rules for generated targets",
      "automatic invariant-generated wrapper should reject identity-fallback reduction results");
}

void RunInvariantGeneratedReductionAutomaticRejectsMissingExplicitRuleForGeneratedTargetTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-generated-wrapper-missing-generated-target"));
  const std::filesystem::path kira_path =
      layout.root / "bin" / "fake-kira-auto-missing-generated-target.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        MakeAutoInvariantResultScript(
                            true, MakeAutoInvariantMissingGeneratedTargetRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::RunInvariantGeneratedReduction(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout,
            kira_path,
            fermat_path));
      },
      "missing a reduction rule for generated target: toy_auto_family[-1,-1,3]",
      "automatic invariant-generated wrapper should require explicit rules for every generated "
      "target");
}

void BuildInvariantGeneratedDESystemAutomaticHappyPathTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path,
                        MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::InvariantGeneratedReductionExecution baseline_execution =
      amflow::RunInvariantGeneratedReduction(spec,
                                             master_basis,
                                             "s",
                                             MakeKiraReductionOptions(),
                                             baseline_layout,
                                             baseline_kira_path,
                                             baseline_fermat_path);
  Expect(baseline_execution.assembled_system.has_value(),
         "automatic invariant-generated wrapper baseline should assemble a DESystem");

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-invariant-auto-desystem"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem system =
      amflow::BuildInvariantGeneratedDESystem(spec,
                                              master_basis,
                                              "s",
                                              MakeKiraReductionOptions(),
                                              layout,
                                              kira_path,
                                              fermat_path);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "automatic invariant DE consumer should not mutate the input problem spec");
  Expect(amflow::ValidateDESystem(system).empty(),
         "automatic invariant DE consumer should return a valid DESystem");
  Expect(system.masters.size() == baseline_execution.assembled_system->masters.size() &&
             system.masters[0].label == "toy_auto_family[1,1,1]" &&
             system.masters[1].label == "toy_auto_family[0,-1,2]",
         "automatic invariant DE consumer should preserve the reviewed master basis");
  Expect(system.variables.size() == 1 &&
             system.variables.front().name == "s" &&
             system.variables.front().kind == amflow::DifferentiationVariableKind::Invariant,
         "automatic invariant DE consumer should preserve the reviewed invariant variable");
  const auto matrix_it = system.coefficient_matrices.find("s");
  Expect(matrix_it != system.coefficient_matrices.end(),
         "automatic invariant DE consumer should populate the invariant coefficient matrix");
  Expect(matrix_it->second == baseline_execution.assembled_system->coefficient_matrices.at("s") &&
             matrix_it->second[0][0] == "(-1)*(2) + ((-1)*(-1))*(5)" &&
             matrix_it->second[0][1] == "(-1)*(3) + ((-1)*(-1))*(7)" &&
             matrix_it->second[1][0] == "(1)*(19) + ((-2)*(-1))*(13)" &&
             matrix_it->second[1][1] == "(1)*(11) + ((-2)*(-1))*(17)",
         "automatic invariant DE consumer should return the reviewed automatic invariant matrix "
         "entries unchanged");
}

void BuildInvariantGeneratedDESystemAutomaticExecutionFailureTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-auto-invariant-desystem-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  try {
    static_cast<void>(amflow::BuildInvariantGeneratedDESystem(MakeAutoInvariantHappyProblemSpec(),
                                                              MakeAutoInvariantHappyMasterBasis(),
                                                              "s",
                                                              MakeKiraReductionOptions(),
                                                              layout,
                                                              kira_path,
                                                              fermat_path));
    throw std::runtime_error("automatic invariant DE consumer should throw after reducer "
                             "execution failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("automatic invariant DE construction requires successful reducer "
                        "execution") != std::string::npos,
           "automatic invariant DE consumer should report reducer execution failure");
    Expect(message.find("status=completed") != std::string::npos,
           "automatic invariant DE consumer should preserve the execution status in the failure "
           "message");
    Expect(message.find("exit_code=9") != std::string::npos,
           "automatic invariant DE consumer should preserve the reducer exit code in the failure "
           "message");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "automatic invariant DE consumer should preserve the stderr log path in the failure "
           "message");
  }
}

void BuildInvariantGeneratedDESystemAutomaticRejectsEmptyInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-empty-name"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::BuildInvariantGeneratedDESystem(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "",
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "requires a non-empty invariant name",
      "automatic invariant DE consumer should preserve the empty invariant-name diagnostic");
}

void BuildInvariantGeneratedDESystemAutomaticRejectsMissingExplicitRuleForGeneratedTargetTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-missing-generated-target"));
  const std::filesystem::path kira_path =
      layout.root / "bin" / "fake-kira-auto-missing-generated-target.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        MakeAutoInvariantResultScript(
                            true, MakeAutoInvariantMissingGeneratedTargetRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  ExpectRuntimeError(
      [&layout, &kira_path, &fermat_path]() {
        static_cast<void>(amflow::BuildInvariantGeneratedDESystem(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout,
            kira_path,
            fermat_path));
      },
      "missing a reduction rule for generated target: toy_auto_family[-1,-1,3]",
      "automatic invariant DE consumer should preserve missing explicit generated-target rule "
      "diagnostics");
}

void SolveInvariantGeneratedSeriesAutomaticHappyPathTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "s=1/5";
  const std::string target_location = "s=9/7";
  const int requested_digits = 73;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path,
                        MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildInvariantGeneratedDESystem(spec,
                                              master_basis,
                                              "s",
                                              MakeKiraReductionOptions(),
                                              baseline_layout,
                                              baseline_kira_path,
                                              baseline_fermat_path);

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-invariant-auto-solver"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.03125;
  solver.returned_diagnostics.overlap_mismatch = 0.0625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded automatic invariant solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveInvariantGeneratedSeries(spec,
                                            master_basis,
                                            "s",
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "automatic invariant solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "automatic invariant solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "automatic invariant solver handoff should forward the direct invariant DESystem "
         "unchanged into SolveRequest");
  Expect(request.system.coefficient_matrices.at("s")[0][0] == "(-1)*(2) + ((-1)*(-1))*(5)" &&
             request.system.coefficient_matrices.at("s")[0][1] ==
                 "(-1)*(3) + ((-1)*(-1))*(7)" &&
             request.system.coefficient_matrices.at("s")[1][0] ==
                 "(1)*(19) + ((-2)*(-1))*(13)" &&
             request.system.coefficient_matrices.at("s")[1][1] ==
                 "(1)*(11) + ((-2)*(-1))*(17)",
         "automatic invariant solver handoff should preserve the reviewed automatic invariant "
         "matrix entries");
  Expect(request.start_location == start_location,
         "automatic invariant solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "automatic invariant solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "automatic invariant solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "automatic invariant solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "automatic invariant solver handoff should return solver diagnostics verbatim");
}

void SolveInvariantGeneratedSeriesAutomaticBootstrapSolverPassthroughTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantHappyProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantHappyMasterBasis();
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "s=-2/3";
  const std::string target_location = "s=11/13";

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path,
                        MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::SolveRequest baseline_request;
  baseline_request.system = amflow::BuildInvariantGeneratedDESystem(spec,
                                                                   master_basis,
                                                                   "s",
                                                                   MakeKiraReductionOptions(),
                                                                   baseline_layout,
                                                                   baseline_kira_path,
                                                                   baseline_fermat_path);
  baseline_request.start_location = start_location;
  baseline_request.target_location = target_location;
  baseline_request.precision_policy = precision_policy;
  baseline_request.requested_digits = 61;

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics expected = solver.Solve(baseline_request);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        MakeAutoInvariantResultScript(true, MakeAutoInvariantHappyRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveInvariantGeneratedSeries(spec,
                                            master_basis,
                                            "s",
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            baseline_request.requested_digits);

  Expect(SameSolverDiagnostics(diagnostics, expected),
         "automatic invariant solver handoff should return the bootstrap solver result "
         "unchanged");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "automatic invariant solver handoff should preserve the baseline bootstrap solver "
         "diagnostics exactly");
}

void SolveInvariantGeneratedSeriesAutomaticExecutionFailureTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-auto-invariant-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveInvariantGeneratedSeries(MakeAutoInvariantHappyProblemSpec(),
                                                            MakeAutoInvariantHappyMasterBasis(),
                                                            "s",
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            kira_path,
                                                            fermat_path,
                                                            solver,
                                                            "s=0",
                                                            "s=1",
                                                            MakeDistinctPrecisionPolicy(),
                                                            55));
    throw std::runtime_error("automatic invariant solver handoff should propagate upstream "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("automatic invariant DE construction requires successful reducer "
                        "execution") != std::string::npos,
           "automatic invariant solver handoff should preserve reducer execution failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "automatic invariant solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "automatic invariant solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "automatic invariant solver handoff should preserve reducer stderr log diagnostics");
  }

  Expect(solver.call_count() == 0,
         "automatic invariant solver handoff should not call the solver when invariant DE "
         "construction fails");
}

void SolveInvariantGeneratedSeriesAutomaticRejectsEmptyInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-empty-name"));
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&layout, &solver]() {
        static_cast<void>(amflow::SolveInvariantGeneratedSeries(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "",
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "s=0",
            "s=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "requires a non-empty invariant name",
      "automatic invariant solver handoff should preserve the empty invariant-name diagnostic");

  Expect(solver.call_count() == 0,
         "automatic invariant solver handoff should not call the solver when invariant-name "
         "validation fails");
}

void SolveInvariantGeneratedSeriesAutomaticRejectsMissingExplicitRuleForGeneratedTargetTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-missing-generated-target"));
  const std::filesystem::path kira_path =
      layout.root / "bin" / "fake-kira-auto-missing-generated-target.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path,
                        MakeAutoInvariantResultScript(
                            true, MakeAutoInvariantMissingGeneratedTargetRuleFile()));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&layout, &kira_path, &fermat_path, &solver]() {
        static_cast<void>(amflow::SolveInvariantGeneratedSeries(
            MakeAutoInvariantHappyProblemSpec(),
            MakeAutoInvariantHappyMasterBasis(),
            "s",
            MakeKiraReductionOptions(),
            layout,
            kira_path,
            fermat_path,
            solver,
            "s=0",
            "s=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "missing a reduction rule for generated target: toy_auto_family[-1,-1,3]",
      "automatic invariant solver handoff should preserve missing explicit generated-target rule "
      "diagnostics");

  Expect(solver.call_count() == 0,
         "automatic invariant solver handoff should not call the solver when generated-target "
         "assembly fails");
}

void BuildInvariantGeneratedDESystemListAutomaticHappyPathTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantWholeFactorProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantWholeFactorMasterBasis();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const std::vector<std::string> invariant_names = {"t", "s"};

  const amflow::ArtifactLayout baseline_s_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-list-s-baseline"));
  const std::filesystem::path baseline_s_kira_path =
      baseline_s_layout.root / "bin" / "fake-kira-auto-factor-s.sh";
  const std::filesystem::path baseline_s_fermat_path =
      baseline_s_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_s_kira_path.parent_path());
  WriteExecutableScript(baseline_s_kira_path,
                        MakeAutoInvariantWholeFactorResultScript(
                            MakeAutoInvariantWholeFactorSRuleFile()));
  WriteExecutableScript(baseline_s_fermat_path, "#!/bin/sh\nexit 0\n");
  const amflow::DESystem baseline_s_system =
      amflow::BuildInvariantGeneratedDESystem(spec,
                                              master_basis,
                                              "s",
                                              MakeKiraReductionOptions(),
                                              baseline_s_layout,
                                              baseline_s_kira_path,
                                              baseline_s_fermat_path);

  const amflow::ArtifactLayout baseline_t_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-list-t-baseline"));
  const std::filesystem::path baseline_t_kira_path =
      baseline_t_layout.root / "bin" / "fake-kira-auto-factor-t.sh";
  const std::filesystem::path baseline_t_fermat_path =
      baseline_t_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_t_kira_path.parent_path());
  WriteExecutableScript(baseline_t_kira_path,
                        MakeAutoInvariantWholeFactorResultScript(
                            MakeAutoInvariantWholeFactorTRuleFile()));
  WriteExecutableScript(baseline_t_fermat_path, "#!/bin/sh\nexit 0\n");
  const amflow::DESystem baseline_t_system =
      amflow::BuildInvariantGeneratedDESystem(spec,
                                              master_basis,
                                              "t",
                                              MakeKiraReductionOptions(),
                                              baseline_t_layout,
                                              baseline_t_kira_path,
                                              baseline_t_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-list"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-factor-list.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeAutoInvariantWholeFactorListResultScript());
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const std::vector<amflow::DESystem> systems =
      amflow::BuildInvariantGeneratedDESystemList(spec,
                                                  master_basis,
                                                  invariant_names,
                                                  MakeKiraReductionOptions(),
                                                  layout,
                                                  kira_path,
                                                  fermat_path);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "automatic invariant DE list wrapper should not mutate the input problem spec");
  Expect(systems.size() == 2,
         "automatic invariant DE list wrapper should preserve invariant count");
  Expect(SameDESystem(systems[0], baseline_t_system),
         "automatic invariant DE list wrapper should preserve the reviewed single-invariant "
         "system for the first caller-provided invariant");
  Expect(SameDESystem(systems[1], baseline_s_system),
         "automatic invariant DE list wrapper should preserve the reviewed single-invariant "
         "system for the second caller-provided invariant");
  Expect(systems[0].variables.size() == 1 && systems[0].variables.front().name == "t" &&
             systems[1].variables.size() == 1 && systems[1].variables.front().name == "s",
         "automatic invariant DE list wrapper should preserve caller order across invariants");
  Expect(std::filesystem::exists(layout.root / "invariant-0001-t" / "generated-config" / "results" /
                                 "toy_auto_factor_family" / "kira_target.m") &&
             std::filesystem::exists(layout.root / "invariant-0002-s" / "generated-config" / "results" /
                                     "toy_auto_factor_family" / "kira_target.m"),
         "automatic invariant DE list wrapper should isolate per-invariant results under child "
         "artifact roots");
  Expect(!std::filesystem::exists(layout.root / "results" / "toy_auto_factor_family"),
         "automatic invariant DE list wrapper should not collapse per-invariant results into the "
         "parent artifact root");
}

void BuildInvariantGeneratedDESystemListAutomaticRejectsEmptyInvariantListTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-list-empty"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::BuildInvariantGeneratedDESystemList(
            MakeAutoInvariantWholeFactorProblemSpec(),
            MakeAutoInvariantWholeFactorMasterBasis(),
            {},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "automatic invariant DE construction list requires at least one invariant name",
      "automatic invariant DE list wrapper should reject empty invariant lists");
}

void BuildInvariantGeneratedDESystemListAutomaticRejectsUnknownInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-desystem-list-unknown"));

  ExpectRuntimeError(
      [&layout]() {
        static_cast<void>(amflow::BuildInvariantGeneratedDESystemList(
            MakeAutoInvariantWholeFactorProblemSpec(),
            MakeAutoInvariantWholeFactorMasterBasis(),
            {"u"},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh"));
      },
      "requires invariant",
      "automatic invariant DE list wrapper should preserve unknown-invariant diagnostics");
}

void SolveInvariantGeneratedSeriesListAutomaticHappyPathTest() {
  const amflow::ProblemSpec spec = MakeAutoInvariantWholeFactorProblemSpec();
  const amflow::ParsedMasterList master_basis = MakeAutoInvariantWholeFactorMasterBasis();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const std::vector<std::string> invariant_names = {"t", "s"};
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "s=2/7";
  const std::string target_location = "s=5/11";
  const int requested_digits = 89;

  const amflow::ArtifactLayout baseline_s_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-list-s-baseline"));
  const std::filesystem::path baseline_s_kira_path =
      baseline_s_layout.root / "bin" / "fake-kira-auto-factor-s.sh";
  const std::filesystem::path baseline_s_fermat_path =
      baseline_s_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_s_kira_path.parent_path());
  WriteExecutableScript(baseline_s_kira_path,
                        MakeAutoInvariantWholeFactorResultScript(
                            MakeAutoInvariantWholeFactorSRuleFile()));
  WriteExecutableScript(baseline_s_fermat_path, "#!/bin/sh\nexit 0\n");
  const amflow::DESystem baseline_s_system =
      amflow::BuildInvariantGeneratedDESystem(spec,
                                              master_basis,
                                              "s",
                                              MakeKiraReductionOptions(),
                                              baseline_s_layout,
                                              baseline_s_kira_path,
                                              baseline_s_fermat_path);

  const amflow::ArtifactLayout baseline_t_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-list-t-baseline"));
  const std::filesystem::path baseline_t_kira_path =
      baseline_t_layout.root / "bin" / "fake-kira-auto-factor-t.sh";
  const std::filesystem::path baseline_t_fermat_path =
      baseline_t_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_t_kira_path.parent_path());
  WriteExecutableScript(baseline_t_kira_path,
                        MakeAutoInvariantWholeFactorResultScript(
                            MakeAutoInvariantWholeFactorTRuleFile()));
  WriteExecutableScript(baseline_t_fermat_path, "#!/bin/sh\nexit 0\n");
  const amflow::DESystem baseline_t_system =
      amflow::BuildInvariantGeneratedDESystem(spec,
                                              master_basis,
                                              "t",
                                              MakeKiraReductionOptions(),
                                              baseline_t_layout,
                                              baseline_t_kira_path,
                                              baseline_t_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-list"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-factor-list.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeAutoInvariantWholeFactorListResultScript());
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolverSequence solver;
  solver.returned_diagnostics = {
      {true, 0.125, 0.25, "", "recorded invariant list solve t"},
      {true, 0.0625, 0.125, "", "recorded invariant list solve s"},
  };

  const std::vector<amflow::SolverDiagnostics> diagnostics =
      amflow::SolveInvariantGeneratedSeriesList(spec,
                                                master_basis,
                                                invariant_names,
                                                MakeKiraReductionOptions(),
                                                layout,
                                                kira_path,
                                                fermat_path,
                                                solver,
                                                start_location,
                                                target_location,
                                                precision_policy,
                                                requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "automatic invariant solver list wrapper should not mutate the input problem spec");
  Expect(diagnostics.size() == solver.returned_diagnostics.size() &&
             SameSolverDiagnostics(diagnostics[0], solver.returned_diagnostics[0]) &&
             SameSolverDiagnostics(diagnostics[1], solver.returned_diagnostics[1]),
         "automatic invariant solver list wrapper should return diagnostics in caller order");
  Expect(solver.call_count() == 2 && solver.seen_requests().size() == 2,
         "automatic invariant solver list wrapper should call the supplied solver once per "
         "invariant");
  Expect(SameDESystem(solver.seen_requests()[0].system, baseline_t_system) &&
             SameDESystem(solver.seen_requests()[1].system, baseline_s_system),
         "automatic invariant solver list wrapper should preserve the reviewed single-invariant "
         "DESystems in sequence");
  Expect(solver.seen_requests()[0].start_location == start_location &&
             solver.seen_requests()[0].target_location == target_location &&
             SamePrecisionPolicy(solver.seen_requests()[0].precision_policy, precision_policy) &&
             solver.seen_requests()[0].requested_digits == requested_digits &&
             solver.seen_requests()[1].start_location == start_location &&
             solver.seen_requests()[1].target_location == target_location &&
             SamePrecisionPolicy(solver.seen_requests()[1].precision_policy, precision_policy) &&
             solver.seen_requests()[1].requested_digits == requested_digits,
         "automatic invariant solver list wrapper should preserve shared solver request fields "
         "for every invariant");
  Expect(solver.seen_requests()[0].system.variables.size() == 1 &&
             solver.seen_requests()[0].system.variables.front().name == "t" &&
             solver.seen_requests()[1].system.variables.size() == 1 &&
             solver.seen_requests()[1].system.variables.front().name == "s",
         "automatic invariant solver list wrapper should preserve invariant order through "
         "SolveRequest generation");
  Expect(std::filesystem::exists(layout.root / "invariant-0001-t" / "generated-config" / "results" /
                                 "toy_auto_factor_family" / "kira_target.m") &&
             std::filesystem::exists(layout.root / "invariant-0002-s" / "generated-config" / "results" /
                                     "toy_auto_factor_family" / "kira_target.m"),
         "automatic invariant solver list wrapper should isolate reducer artifacts per "
         "invariant");
  Expect(!std::filesystem::exists(layout.root / "results" / "toy_auto_factor_family"),
         "automatic invariant solver list wrapper should not collapse per-invariant results into "
         "the parent artifact root");
}

void SolveInvariantGeneratedSeriesListAutomaticExecutionFailureStopsIterationTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-list-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-auto-factor-list.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeAutoInvariantWholeFactorListResultScript(true));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolverSequence solver;
  solver.returned_diagnostics = {
      {true, 0.125, 0.25, "", "recorded invariant list solve s"},
      {true, 0.0625, 0.125, "", "recorded invariant list solve t"},
      {true, 0.03125, 0.0625, "", "recorded invariant list solve s-again"},
  };

  try {
    static_cast<void>(amflow::SolveInvariantGeneratedSeriesList(
        MakeAutoInvariantWholeFactorProblemSpec(),
        MakeAutoInvariantWholeFactorMasterBasis(),
        {"s", "t", "s"},
        MakeKiraReductionOptions(),
        layout,
        kira_path,
        fermat_path,
        solver,
        "s=0",
        "s=1",
        MakeDistinctPrecisionPolicy(),
        55));
    throw std::runtime_error("automatic invariant solver list wrapper should stop on invariant "
                             "reducer failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("automatic invariant DE construction requires successful reducer "
                        "execution") != std::string::npos,
           "automatic invariant solver list wrapper should preserve reducer failure text");
    Expect(message.find("exit_code=9") != std::string::npos,
           "automatic invariant solver list wrapper should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find((layout.root / "invariant-0002-t" / "logs").string()) !=
                   std::string::npos,
           "automatic invariant solver list wrapper should preserve the failing child artifact "
           "root");
  }

  Expect(solver.call_count() == 1 && solver.seen_requests().size() == 1,
         "automatic invariant solver list wrapper should stop before calling the solver for "
         "later invariants after a hard reducer failure");
  Expect(solver.seen_requests().front().system.variables.size() == 1 &&
             solver.seen_requests().front().system.variables.front().name == "s",
         "automatic invariant solver list wrapper should only record the successful prefix "
         "request before stopping");
  Expect(std::filesystem::exists(layout.root / "invariant-0001-s"),
         "automatic invariant solver list wrapper should preserve artifacts for completed prefix "
         "iterations");
  Expect(std::filesystem::exists(layout.root / "invariant-0002-t"),
         "automatic invariant solver list wrapper should preserve artifacts for the failing "
         "iteration");
  Expect(!std::filesystem::exists(layout.root / "invariant-0003-s"),
         "automatic invariant solver list wrapper should not start later invariants after a hard "
         "failure");
}

void SolveInvariantGeneratedSeriesListAutomaticRejectsEmptyInvariantListTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-list-empty"));
  RecordingSeriesSolverSequence solver;

  ExpectRuntimeError(
      [&layout, &solver]() {
        static_cast<void>(amflow::SolveInvariantGeneratedSeriesList(
            MakeAutoInvariantWholeFactorProblemSpec(),
            MakeAutoInvariantWholeFactorMasterBasis(),
            {},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "s=0",
            "s=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "automatic invariant solver handoff list requires at least one invariant name",
      "automatic invariant solver list wrapper should reject empty invariant lists");

  Expect(solver.call_count() == 0,
         "automatic invariant solver list wrapper should not call the solver when invariant-list "
         "validation fails");
}

void SolveInvariantGeneratedSeriesListAutomaticRejectsUnknownInvariantNameTest() {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-invariant-auto-solver-list-unknown"));
  RecordingSeriesSolverSequence solver;

  ExpectRuntimeError(
      [&layout, &solver]() {
        static_cast<void>(amflow::SolveInvariantGeneratedSeriesList(
            MakeAutoInvariantWholeFactorProblemSpec(),
            MakeAutoInvariantWholeFactorMasterBasis(),
            {"u"},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "s=0",
            "s=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "requires invariant",
      "automatic invariant solver list wrapper should preserve unknown-invariant diagnostics");

  Expect(solver.call_count() == 0,
         "automatic invariant solver list wrapper should not call the solver when invariant-name "
         "validation fails");
}

void SolveEtaGeneratedSeriesHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=1/7";
  const std::string target_location = "eta=5/3";
  const int requested_digits = 67;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-solver-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-solver"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.015625;
  solver.returned_diagnostics.overlap_mismatch = 0.03125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded eta solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveEtaGeneratedSeries(spec,
                                      master_basis,
                                      MakeEtaGeneratedHappyDecision(),
                                      MakeKiraReductionOptions(),
                                      layout,
                                      kira_path,
                                      fermat_path,
                                      solver,
                                      start_location,
                                      target_location,
                                      precision_policy,
                                      requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "eta solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "eta solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "eta solver handoff should forward the direct eta DESystem unchanged into SolveRequest");
  Expect(request.system.coefficient_matrices.at("eta")[0][0] == "(-1)*(2)" &&
             request.system.coefficient_matrices.at("eta")[0][1] ==
                 "(-1)*(1) + (-1)*(s)" &&
             request.system.coefficient_matrices.at("eta")[1][0] == "(-1)*(t)" &&
             request.system.coefficient_matrices.at("eta")[1][1] == "(-1)*(3)",
         "eta solver handoff should preserve the reviewed eta matrix entries");
  Expect(request.start_location == start_location,
         "eta solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "eta solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "eta solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "eta solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "eta solver handoff should return solver diagnostics verbatim");
}

void SolveEtaGeneratedSeriesBootstrapSolverPassthroughTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=-2/3";
  const std::string target_location = "eta=11/13";

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-solver-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::SolveRequest baseline_request;
  baseline_request.system = amflow::BuildEtaGeneratedDESystem(spec,
                                                             master_basis,
                                                             MakeEtaGeneratedHappyDecision(),
                                                             MakeKiraReductionOptions(),
                                                             baseline_layout,
                                                             baseline_kira_path,
                                                             baseline_fermat_path);
  baseline_request.start_location = start_location;
  baseline_request.target_location = target_location;
  baseline_request.precision_policy = precision_policy;
  baseline_request.requested_digits = 59;

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics expected = solver.Solve(baseline_request);

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-solver-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveEtaGeneratedSeries(spec,
                                      master_basis,
                                      MakeEtaGeneratedHappyDecision(),
                                      MakeKiraReductionOptions(),
                                      layout,
                                      kira_path,
                                      fermat_path,
                                      solver,
                                      start_location,
                                      target_location,
                                      precision_policy,
                                      baseline_request.requested_digits);

  Expect(SameSolverDiagnostics(diagnostics, expected),
         "eta solver handoff should return the bootstrap solver result unchanged");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "eta solver handoff should preserve the baseline bootstrap solver diagnostics exactly");
}

void SolveEtaGeneratedSeriesExecutionFailureTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-solver-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-eta-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveEtaGeneratedSeries(amflow::MakeSampleProblemSpec(),
                                                      master_basis,
                                                      MakeEtaGeneratedHappyDecision(),
                                                      MakeKiraReductionOptions(),
                                                      layout,
                                                      kira_path,
                                                      fermat_path,
                                                      solver,
                                                      "eta=0",
                                                      "eta=1",
                                                      MakeDistinctPrecisionPolicy(),
                                                      55));
    throw std::runtime_error("eta solver handoff should propagate upstream "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "eta solver handoff should preserve reducer execution failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "eta solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "eta solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "eta solver handoff should preserve reducer stderr log diagnostics");
  }

  Expect(solver.call_count() == 0,
         "eta solver handoff should not call the solver when eta DE construction fails");
}

void SolveEtaGeneratedSeriesRejectsIdentityFallbackResultsTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path master_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const std::filesystem::path missing_rule_root = TestDataRoot() / "kira-results/missing-rule";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(master_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-solver-missing-rule"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-missing-rule.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(missing_rule_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&master_basis, &layout, &kira_path, &fermat_path, &solver]() {
        static_cast<void>(amflow::SolveEtaGeneratedSeries(amflow::MakeSampleProblemSpec(),
                                                          master_basis,
                                                          MakeEtaGeneratedHappyDecision(),
                                                          MakeKiraReductionOptions(),
                                                          layout,
                                                          kira_path,
                                                          fermat_path,
                                                          solver,
                                                          "eta=0",
                                                          "eta=1",
                                                          MakeDistinctPrecisionPolicy(),
                                                          55));
      },
      "requires explicit reduction rules for generated targets",
      "eta solver handoff should preserve identity-fallback parse and assembly diagnostics");

  Expect(solver.call_count() == 0,
         "eta solver handoff should not call the solver when eta generated-target assembly "
         "fails");
}

void SolveEtaGeneratedSeriesRejectsEmptyGeneratedTargetsTest() {
  amflow::ParsedMasterList master_basis;
  master_basis.family = "planar_double_box";
  master_basis.masters = {
      {"planar_double_box", {0, 1, 1, 1, 1, 1, 0}},
      {"planar_double_box", {0, 1, 1, 1, 1, 1, 0}},
  };
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-solver-empty-targets"));
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();

  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveEtaGeneratedSeries(
            spec,
            master_basis,
            MakeEtaGeneratedHappyDecision(),
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "requires at least one generated reduction target",
      "eta solver handoff should preserve empty generated-target diagnostics");

  Expect(solver.call_count() == 0,
         "eta solver handoff should not call the solver when eta generated-target preparation "
         "fails");
}

void SolveEtaModePlannedSeriesHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=2/9";
  const std::string target_location = "eta=7/5";
  const int requested_digits = 73;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-mode-solver-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-mode-solver"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingEtaMode eta_mode(MakeEtaGeneratedHappyDecision(), "RecordedHappy");
  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.0078125;
  solver.returned_diagnostics.overlap_mismatch = 0.015625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded planned eta solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveEtaModePlannedSeries(spec,
                                        master_basis,
                                        eta_mode,
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "eta-mode-planned solver handoff should not mutate the input problem spec");
  Expect(eta_mode.call_count() == 1,
         "eta-mode-planned solver handoff should plan exactly once");
  Expect(eta_mode.last_planned_spec_yaml() == original_yaml,
         "eta-mode-planned solver handoff should plan against the original problem spec");
  Expect(solver.call_count() == 1,
         "eta-mode-planned solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "eta-mode-planned solver handoff should forward the direct eta DESystem unchanged into "
         "SolveRequest");
  Expect(request.start_location == start_location,
         "eta-mode-planned solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "eta-mode-planned solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "eta-mode-planned solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "eta-mode-planned solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "eta-mode-planned solver handoff should return solver diagnostics verbatim");
}

void SolveEtaModePlannedSeriesBootstrapSolverPassthroughTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=-5/8";
  const std::string target_location = "eta=13/21";

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-mode-solver-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::SolveRequest baseline_request;
  baseline_request.system = amflow::BuildEtaGeneratedDESystem(spec,
                                                             master_basis,
                                                             MakeEtaGeneratedHappyDecision(),
                                                             MakeKiraReductionOptions(),
                                                             baseline_layout,
                                                             baseline_kira_path,
                                                             baseline_fermat_path);
  baseline_request.start_location = start_location;
  baseline_request.target_location = target_location;
  baseline_request.precision_policy = precision_policy;
  baseline_request.requested_digits = 61;

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics expected = solver.Solve(baseline_request);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-mode-solver-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingEtaMode eta_mode(MakeEtaGeneratedHappyDecision(), "RecordedHappy");
  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveEtaModePlannedSeries(spec,
                                        master_basis,
                                        eta_mode,
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        baseline_request.requested_digits);

  Expect(eta_mode.call_count() == 1,
         "eta-mode-planned bootstrap passthrough should plan exactly once");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "eta-mode-planned solver handoff should return the bootstrap solver result unchanged");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "eta-mode-planned solver handoff should preserve the baseline bootstrap solver "
         "diagnostics exactly");
}

void SolveEtaModePlannedSeriesPlanningFailureTest() {
  const amflow::ProblemSpec spec = MakeUnsupportedBranchLoopGrammarSpec();
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-eta-mode-plan-failure"));
  const auto eta_mode = amflow::MakeBuiltinEtaMode("Branch");

  RecordingSeriesSolver solver;

  const std::string message = CaptureRuntimeErrorMessage(
      [&spec, &master_basis, &layout, &eta_mode, &solver]() {
        static_cast<void>(amflow::SolveEtaModePlannedSeries(spec,
                                                            master_basis,
                                                            *eta_mode,
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            layout.root / "bin" / "unused-kira.sh",
                                                            layout.root / "bin" /
                                                                "unused-fermat.sh",
                                                            solver,
                                                            "eta=0",
                                                            "eta=1",
                                                            MakeDistinctPrecisionPolicy(),
                                                            55));
      },
      "eta-mode-planned solver handoff should preserve eta-mode planning failures");
  ExpectBranchLoopBootstrapBlockerMessage(
      message,
      "Branch",
      "eta-mode-planned solver handoff should preserve eta-mode planning failures");

  Expect(solver.call_count() == 0,
         "eta-mode-planned solver handoff should not call the solver when eta planning fails");
}

void SolveEtaModePlannedSeriesExecutionFailureTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-eta-mode-solver-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-eta-mode-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingEtaMode eta_mode(MakeEtaGeneratedHappyDecision(), "RecordedHappy");
  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveEtaModePlannedSeries(amflow::MakeSampleProblemSpec(),
                                                        master_basis,
                                                        eta_mode,
                                                        MakeKiraReductionOptions(),
                                                        layout,
                                                        kira_path,
                                                        fermat_path,
                                                        solver,
                                                        "eta=0",
                                                        "eta=1",
                                                        MakeDistinctPrecisionPolicy(),
                                                        55));
    throw std::runtime_error("eta-mode-planned solver handoff should preserve upstream eta "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "eta-mode-planned solver handoff should preserve reducer execution failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "eta-mode-planned solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "eta-mode-planned solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "eta-mode-planned solver handoff should preserve reducer stderr log diagnostics");
  }

  Expect(eta_mode.call_count() == 1,
         "eta-mode-planned solver handoff should still plan before eta execution fails");
  Expect(solver.call_count() == 0,
         "eta-mode-planned solver handoff should not call the solver when eta DE construction "
         "fails");
}

void SolveBuiltinEtaModeSeriesHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=3/11";
  const std::string target_location = "eta=17/19";
  const int requested_digits = 71;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-solver-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-solver"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.00390625;
  solver.returned_diagnostics.overlap_mismatch = 0.0078125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded builtin eta-mode solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeSeries(spec,
                                        master_basis,
                                        "All",
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode solver handoff should forward the direct eta DESystem unchanged into "
         "SolveRequest");
  Expect(request.system.coefficient_matrices.at("eta")[0][0] == "(-1)*(2)" &&
             request.system.coefficient_matrices.at("eta")[0][1] ==
                 "(-1)*(1) + (-1)*(s)" &&
             request.system.coefficient_matrices.at("eta")[1][0] == "(-1)*(t)" &&
             request.system.coefficient_matrices.at("eta")[1][1] == "(-1)*(3)",
         "builtin eta-mode solver handoff should preserve the reviewed eta matrix entries");
  Expect(request.start_location == start_location,
         "builtin eta-mode solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "builtin eta-mode solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode solver handoff should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeSeriesPrescriptionHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=3/11";
  const std::string target_location = "eta=17/19";
  const int requested_digits = 71;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-prescription-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(
          spec,
          master_basis,
          amflow::MakeBuiltinEtaMode("Prescription")->Plan(spec),
          MakeKiraReductionOptions(),
          baseline_layout,
          baseline_kira_path,
          baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-prescription"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.00390625;
  solver.returned_diagnostics.overlap_mismatch = 0.0078125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded prescription builtin eta-mode solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeSeries(spec,
                                        master_basis,
                                        "Prescription",
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode solver handoff should forward the selected Prescription eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location,
         "builtin eta-mode solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "builtin eta-mode solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode solver handoff should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeSeriesMassHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=3/11";
  const std::string target_location = "eta=17/19";
  const int requested_digits = 71;
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);

  Expect(baseline_decision.selected_propagator_indices ==
             MakeEtaGeneratedHappyDecision().selected_propagator_indices,
         "builtin Mass eta-mode solve should recover the reviewed {0,6} eta-generated subset on "
         "the narrow mixed-mass bootstrap fixture");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-mass-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-mass"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.001953125;
  solver.returned_diagnostics.overlap_mismatch = 0.00390625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded mass builtin eta-mode solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeSeries(spec,
                                        master_basis,
                                        "Mass",
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin Mass eta-mode solve should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin Mass eta-mode solve should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin Mass eta-mode solve should forward the selected Mass eta DESystem unchanged "
         "into SolveRequest");
  Expect(request.start_location == start_location,
         "builtin Mass eta-mode solve should preserve the start location");
  Expect(request.target_location == target_location,
         "builtin Mass eta-mode solve should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin Mass eta-mode solve should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin Mass eta-mode solve should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin Mass eta-mode solve should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeSeriesPropagatorHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root =
      WritePropagatorHappyFixture("amflow-bootstrap-builtin-eta-mode-propagator-fixture");
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinPropagatorMixedSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=3/11";
  const std::string target_location = "eta=17/19";
  const int requested_digits = 71;
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Propagator")->Plan(spec);

  Expect(baseline_decision.selected_propagator_indices ==
             std::vector<std::size_t>{0, 2, 3, 5, 6},
         "builtin Propagator eta-mode solve should use the broader mixed non-auxiliary fixture");
  Expect(baseline_decision.selected_propagator_indices !=
             MakeEtaGeneratedHappyDecision().selected_propagator_indices,
         "builtin Propagator eta-mode solve should not collapse back to the old {0,6} selection");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-propagator-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-propagator"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.00390625;
  solver.returned_diagnostics.overlap_mismatch = 0.0078125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded propagator builtin eta-mode solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeSeries(spec,
                                        master_basis,
                                        "Propagator",
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode solver handoff should forward the selected Propagator eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location,
         "builtin eta-mode solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "builtin eta-mode solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode solver handoff should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeSeriesPropagatorAllowsSelectedNonzeroMassTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root =
      WritePropagatorHappyFixture("amflow-bootstrap-builtin-eta-mode-propagator-mass-fixture");
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinPropagatorMassBoundarySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::EtaInsertionDecision decision =
      amflow::MakeBuiltinEtaMode("Propagator")->Plan(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=5/13";
  const std::string target_location = "eta=19/23";
  const int requested_digits = 59;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-propagator-mass-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-propagator-mass"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");
  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.0009765625;
  solver.returned_diagnostics.overlap_mismatch = 0.001953125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded propagator mixed-mass eta-mode solve";

  Expect(decision.selected_propagator_indices ==
             std::vector<std::size_t>{0, 2, 3, 5, 6},
         "Propagator planning should still succeed structurally on the mixed mass-boundary "
         "fixture");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeSeries(spec,
                                        master_basis,
                                        "Propagator",
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin Propagator eta-mode solve should not mutate the mixed mass-boundary fixture");
  Expect(solver.call_count() == 1,
         "builtin Propagator eta-mode solve should now reach the solver on the reviewed "
         "selected-nonzero-mass path");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin Propagator eta-mode solve should preserve the widened mixed-mass eta "
         "DESystem construction");
  Expect(request.start_location == start_location &&
             request.target_location == target_location,
         "builtin Propagator eta-mode solve should preserve solver locations on the widened "
         "mixed-mass path");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin Propagator eta-mode solve should preserve every precision-policy field after "
         "the mixed-mass widening");
  Expect(request.requested_digits == requested_digits,
         "builtin Propagator eta-mode solve should preserve requested_digits on the widened "
         "path");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin Propagator eta-mode solve should return solver diagnostics verbatim on the "
         "widened mixed-mass path");
}

void SolveBuiltinEtaModeSeriesBootstrapSolverPassthroughTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=-8/15";
  const std::string target_location = "eta=23/29";

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-solver-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::SolveRequest baseline_request;
  baseline_request.system = amflow::BuildEtaGeneratedDESystem(spec,
                                                             master_basis,
                                                             MakeEtaGeneratedHappyDecision(),
                                                             MakeKiraReductionOptions(),
                                                             baseline_layout,
                                                             baseline_kira_path,
                                                             baseline_fermat_path);
  baseline_request.start_location = start_location;
  baseline_request.target_location = target_location;
  baseline_request.precision_policy = precision_policy;
  baseline_request.requested_digits = 79;

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics expected = solver.Solve(baseline_request);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-solver-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeSeries(spec,
                                        master_basis,
                                        "All",
                                        MakeKiraReductionOptions(),
                                        layout,
                                        kira_path,
                                        fermat_path,
                                        solver,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        baseline_request.requested_digits);

  Expect(SameSolverDiagnostics(diagnostics, expected),
         "builtin eta-mode solver handoff should return the bootstrap solver result unchanged");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "builtin eta-mode solver handoff should preserve the baseline bootstrap solver "
         "diagnostics exactly");
}

void SolveBuiltinEtaModeSeriesRejectsUnknownBuiltinNameTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-unknown-name"));
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeSeries(spec,
                                                            master_basis,
                                                            "NotAMode",
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            layout.root / "bin" / "unused-kira.sh",
                                                            layout.root / "bin" /
                                                                "unused-fermat.sh",
                                                            solver,
                                                            "eta=0",
                                                            "eta=1",
                                                            MakeDistinctPrecisionPolicy(),
                                                            55));
      },
      "unknown eta mode: NotAMode",
      "builtin eta-mode solver handoff should preserve unknown builtin-name diagnostics");

  Expect(solver.call_count() == 0,
         "builtin eta-mode solver handoff should not call the solver when builtin-name "
         "resolution fails");
}

void SolveBuiltinEtaModeSeriesUnsupportedBuiltinModesRejectTest() {
  const std::vector<std::string> unsupported_modes = {
      "Branch",
      "Loop",
  };

  for (const auto& mode_name : unsupported_modes) {
    const amflow::ProblemSpec spec = MakeUnsupportedBranchLoopGrammarSpec();
    amflow::ParsedMasterList master_basis;
    const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
        FreshTempDir("amflow-bootstrap-builtin-eta-mode-unsupported-" + mode_name));
    RecordingSeriesSolver solver;

    const std::string message = CaptureRuntimeErrorMessage(
        [&spec, &master_basis, &layout, &mode_name, &solver]() {
          static_cast<void>(amflow::SolveBuiltinEtaModeSeries(
              spec,
              master_basis,
              mode_name,
              MakeKiraReductionOptions(),
              layout,
              layout.root / "bin" / "unused-kira.sh",
              layout.root / "bin" / "unused-fermat.sh",
              solver,
              "eta=0",
              "eta=1",
              MakeDistinctPrecisionPolicy(),
              55));
        },
        "builtin eta-mode solver handoff should preserve unsupported builtin planning "
        "diagnostics");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "builtin eta-mode solver handoff should preserve unsupported builtin planning "
        "diagnostics");

    Expect(solver.call_count() == 0,
           "builtin eta-mode solver handoff should not call the solver when builtin planning "
           "fails");
  }
}

void SolveBuiltinEtaModeListSeriesBootstrapPreflightFailureTest() {
  const amflow::ProblemSpec spec = MakeUnsupportedBranchLoopGrammarSpec();
  amflow::ParsedMasterList master_basis;
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
        FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-" + mode_name + "-preflight-failure"));
    RecordingSeriesSolver solver;

    const std::string message = CaptureRuntimeErrorMessage(
        [&spec, &master_basis, &layout, &solver, &mode_name]() {
          static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(
              spec,
              master_basis,
              {mode_name, "Propagator"},
              MakeKiraReductionOptions(),
              layout,
              layout.root / "bin" / "unused-kira.sh",
              layout.root / "bin" / "unused-fermat.sh",
              solver,
              "eta=0",
              "eta=1",
              MakeDistinctPrecisionPolicy(),
              55));
        },
        "builtin eta-mode-list solver handoff should preserve " + mode_name +
            " preflight failures");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "builtin eta-mode-list solver handoff should preserve " + mode_name +
            " preflight failures");

    Expect(solver.call_count() == 0,
           "builtin eta-mode-list solver handoff should not call the solver when " + mode_name +
               " planning fails");
  }
}

void SolveBuiltinEtaModeSeriesRejectsPropagatorWithoutNonAuxiliaryPropagatorsTest() {
  const amflow::ProblemSpec spec = MakeBuiltinAllAuxiliarySpec();
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-propagator-empty"));
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeSeries(spec,
                                                            master_basis,
                                                            "Propagator",
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            layout.root / "bin" / "unused-kira.sh",
                                                            layout.root / "bin" /
                                                                "unused-fermat.sh",
                                                            solver,
                                                            "eta=0",
                                                            "eta=1",
                                                            MakeDistinctPrecisionPolicy(),
                                                            55));
      },
      "eta mode Propagator found no non-auxiliary propagators in bootstrap",
      "builtin eta-mode solver handoff should preserve empty Propagator-mode diagnostics");

  Expect(solver.call_count() == 0,
         "builtin eta-mode solver handoff should not call the solver when builtin Propagator "
         "selection is empty");
}

void SolveBuiltinEtaModeSeriesRejectsAllWithoutNonAuxiliaryPropagatorsTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  for (auto& propagator : spec.family.propagators) {
    propagator.kind = amflow::PropagatorKind::Auxiliary;
  }

  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-all-empty"));
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeSeries(spec,
                                                            master_basis,
                                                            "All",
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            layout.root / "bin" / "unused-kira.sh",
                                                            layout.root / "bin" /
                                                                "unused-fermat.sh",
                                                            solver,
                                                            "eta=0",
                                                            "eta=1",
                                                            MakeDistinctPrecisionPolicy(),
                                                            55));
      },
      "eta mode All found no non-auxiliary propagators in bootstrap",
      "builtin eta-mode solver handoff should preserve empty All-mode diagnostics");

  Expect(solver.call_count() == 0,
         "builtin eta-mode solver handoff should not call the solver when builtin All "
         "selection is empty");
}

void SolveBuiltinEtaModeSeriesExecutionFailureTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-solver-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-builtin-eta-mode-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveBuiltinEtaModeSeries(spec,
                                                        master_basis,
                                                        "All",
                                                        MakeKiraReductionOptions(),
                                                        layout,
                                                        kira_path,
                                                        fermat_path,
                                                        solver,
                                                        "eta=0",
                                                        "eta=1",
                                                        MakeDistinctPrecisionPolicy(),
                                                        55));
    throw std::runtime_error("builtin eta-mode solver handoff should preserve upstream eta "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "builtin eta-mode solver handoff should preserve reducer execution failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "builtin eta-mode solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "builtin eta-mode solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "builtin eta-mode solver handoff should preserve reducer stderr log diagnostics");
  }

  Expect(solver.call_count() == 0,
         "builtin eta-mode solver handoff should not call the solver when eta DE construction "
         "fails");
}

void SolveBuiltinEtaModeListSeriesHappyPathFallbackTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root =
      WritePropagatorHappyFixture("amflow-bootstrap-builtin-eta-mode-list-propagator-fixture");
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinPropagatorMixedSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=4/9";
  const std::string target_location = "eta=25/27";
  const int requested_digits = 83;
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Propagator")->Plan(spec);

  Expect(baseline_decision.selected_propagator_indices ==
             std::vector<std::size_t>{0, 2, 3, 5, 6},
         "builtin eta-mode-list fallback should plan the broader Propagator mixed fixture");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-solver-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-solver"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.001953125;
  solver.returned_diagnostics.overlap_mismatch = 0.00390625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded builtin eta-mode list solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            {"Propagator"},
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode-list solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode-list solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode-list solver handoff should forward the selected Propagator eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location,
         "builtin eta-mode-list solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "builtin eta-mode-list solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode-list solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode-list solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode-list solver handoff should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeListSeriesMassShortCircuitTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=4/9";
  const std::string target_location = "eta=25/27";
  const int requested_digits = 83;
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-mass-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-mass"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.0009765625;
  solver.returned_diagnostics.overlap_mismatch = 0.001953125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded builtin eta-mode list mass solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            {"Mass", "Propagator"},
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode-list Mass short-circuit should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode-list Mass short-circuit should stop after Mass selects a reviewed "
         "candidate group");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode-list Mass short-circuit should forward the selected Mass eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location &&
             request.target_location == target_location,
         "builtin eta-mode-list Mass short-circuit should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode-list Mass short-circuit should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode-list Mass short-circuit should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode-list Mass short-circuit should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeListSeriesMassDependentFallbackShortCircuitTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinMassDependentOnlySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=-5/9";
  const std::string target_location = "eta=29/31";
  const int requested_digits = 79;
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-mass-dependent-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-mass-dependent"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.00048828125;
  solver.returned_diagnostics.overlap_mismatch = 0.0009765625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded builtin eta-mode list dependent-mass solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            {"Mass", "Propagator"},
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode-list Mass fallback short-circuit should not mutate the input problem "
         "spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode-list Mass fallback short-circuit should still stop after the "
         "planning-successful dependent-only Mass selection");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode-list Mass fallback short-circuit should forward the dependent-only "
         "Mass eta DESystem rather than falling through to Propagator");
  Expect(request.start_location == start_location &&
             request.target_location == target_location,
         "builtin eta-mode-list Mass fallback short-circuit should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode-list Mass fallback short-circuit should preserve every "
         "precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode-list Mass fallback short-circuit should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode-list Mass fallback short-circuit should return solver diagnostics "
         "verbatim");
}

void SolveBuiltinEtaModeListSeriesPrescriptionShortCircuitTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=4/9";
  const std::string target_location = "eta=25/27";
  const int requested_digits = 83;

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-prescription-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(
          spec,
          master_basis,
          amflow::MakeBuiltinEtaMode("Prescription")->Plan(spec),
          MakeKiraReductionOptions(),
          baseline_layout,
          baseline_kira_path,
          baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-prescription"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.001953125;
  solver.returned_diagnostics.overlap_mismatch = 0.00390625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded builtin eta-mode list solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            {"Prescription", "Mass"},
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "builtin eta-mode-list solver handoff should not mutate the input problem spec");
  Expect(solver.call_count() == 1,
         "builtin eta-mode-list solver handoff should stop after Prescription succeeds");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "builtin eta-mode-list solver handoff should forward the selected Prescription eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location,
         "builtin eta-mode-list solver handoff should preserve the start location");
  Expect(request.target_location == target_location,
         "builtin eta-mode-list solver handoff should preserve the target location");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "builtin eta-mode-list solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "builtin eta-mode-list solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "builtin eta-mode-list solver handoff should return solver diagnostics verbatim");
}

void SolveBuiltinEtaModeListSeriesBootstrapSolverStopsAfterFirstCompletedModeTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=-11/14";
  const std::string target_location = "eta=31/37";

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-solver-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::SolveRequest baseline_request;
  baseline_request.system = amflow::BuildEtaGeneratedDESystem(spec,
                                                             master_basis,
                                                             MakeEtaGeneratedHappyDecision(),
                                                             MakeKiraReductionOptions(),
                                                             baseline_layout,
                                                             baseline_kira_path,
                                                             baseline_fermat_path);
  baseline_request.start_location = start_location;
  baseline_request.target_location = target_location;
  baseline_request.precision_policy = precision_policy;
  baseline_request.requested_digits = 87;

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics expected = solver.Solve(baseline_request);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-solver-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            {"All", "Mass"},
                                            MakeKiraReductionOptions(),
                                            layout,
                                            kira_path,
                                            fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            baseline_request.requested_digits);

  Expect(SameSolverDiagnostics(diagnostics, expected),
         "builtin eta-mode-list solver handoff should return the bootstrap solver result "
         "unchanged");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "builtin eta-mode-list solver handoff should preserve the baseline bootstrap solver "
         "diagnostics exactly");
}

void SolveBuiltinEtaModeListSeriesPrescriptionFailureFallsThroughToMassFailureTest() {
  const amflow::ProblemSpec spec = MakeBuiltinAllAuxiliarySpec();
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-prescription-fallthrough"));
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(
            spec,
            master_basis,
            {"Prescription", "Mass"},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "eta mode Mass found no non-auxiliary non-zero-mass propagator group in bootstrap",
      "builtin eta-mode-list solver handoff should fall through from an empty Prescription "
      "planning result and preserve the Mass no-candidate failure");

  Expect(solver.call_count() == 0,
         "builtin eta-mode-list solver handoff should not call the solver when Prescription "
         "fails and Mass has no reviewed candidate group");
}

void SolveBuiltinEtaModeListSeriesRejectsEmptyModeListTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-empty"));
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(
            spec,
            master_basis,
            {},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "builtin eta-mode list must not be empty",
      "builtin eta-mode-list solver handoff should reject empty caller-supplied lists");

  Expect(solver.call_count() == 0,
         "builtin eta-mode-list solver handoff should not call the solver when no builtin "
         "mode names were supplied");
}

void SolveBuiltinEtaModeListSeriesRejectsUnknownBuiltinNameImmediatelyTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-unknown-name"));
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(
            spec,
            master_basis,
            {"NotAMode", "All"},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "unknown eta mode: NotAMode",
      "builtin eta-mode-list solver handoff should preserve unknown builtin-name diagnostics");

  Expect(solver.call_count() == 0,
         "builtin eta-mode-list solver handoff should not call the solver when builtin-name "
         "resolution fails");
}

void SolveBuiltinEtaModeListSeriesExhaustedKnownModesPreservesLastDiagnosticTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  for (auto& propagator : spec.family.propagators) {
    propagator.kind = amflow::PropagatorKind::Auxiliary;
  }

  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-planning-exhaustion"));
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(
            spec,
            master_basis,
            {"Mass", "Propagator"},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "eta mode Propagator found no non-auxiliary propagators in bootstrap",
      "builtin eta-mode-list solver handoff should preserve the final planning diagnostic when "
      "no builtin mode reaches solve selection");

  Expect(solver.call_count() == 0,
         "builtin eta-mode-list solver handoff should not call the solver when every builtin "
         "mode fails during planning");
}

void SolveBuiltinEtaModeListSeriesRejectsAllWithoutNonAuxiliaryPropagatorsTest() {
  amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  for (auto& propagator : spec.family.propagators) {
    propagator.kind = amflow::PropagatorKind::Auxiliary;
  }

  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-all-empty"));
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&spec, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(
            spec,
            master_basis,
            {"All"},
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "eta mode All found no non-auxiliary propagators in bootstrap",
      "builtin eta-mode-list solver handoff should preserve empty All-mode diagnostics");

  Expect(solver.call_count() == 0,
         "builtin eta-mode-list solver handoff should not call the solver when builtin All "
         "selection is empty");
}

void SolveBuiltinEtaModeListSeriesExecutionFailureStopsIterationTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-builtin-eta-mode-list-solver-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-builtin-eta-mode-list-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveBuiltinEtaModeListSeries(spec,
                                                            master_basis,
                                                            {"All", "Mass"},
                                                            MakeKiraReductionOptions(),
                                                            layout,
                                                            kira_path,
                                                            fermat_path,
                                                            solver,
                                                            "eta=0",
                                                            "eta=1",
                                                            MakeDistinctPrecisionPolicy(),
                                                            55));
    throw std::runtime_error("builtin eta-mode-list solver handoff should preserve upstream eta "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "builtin eta-mode-list solver handoff should preserve reducer execution failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "builtin eta-mode-list solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "builtin eta-mode-list solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "builtin eta-mode-list solver handoff should preserve reducer stderr log diagnostics");
  }

  Expect(solver.call_count() == 0,
         "builtin eta-mode-list solver handoff should not call the solver when eta DE "
         "construction fails after selecting a builtin mode");
}

void SolveAmfOptionsEtaModeSeriesHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root =
      WritePropagatorHappyFixture("amflow-bootstrap-amf-options-eta-mode-propagator-fixture");
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinPropagatorMixedSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"Propagator"});
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=4/9";
  const std::string target_location = "rho=25/27";
  const int requested_digits = 83;
  const std::string eta_symbol = "rho";
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Propagator")->Plan(spec);

  Expect(baseline_decision.selected_propagator_indices ==
             std::vector<std::size_t>{0, 2, 3, 5, 6},
         "AmfOptions eta-mode wrapper should plan the broader Propagator mixed fixture on the "
         "stub-first happy path");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-solver-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path,
                                        eta_symbol);

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-amf-options-eta-mode-solver"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver baseline_solver;
  RecordingSeriesSolver solver;
  baseline_solver.returned_diagnostics.success = true;
  baseline_solver.returned_diagnostics.residual_norm = 0.001953125;
  baseline_solver.returned_diagnostics.overlap_mismatch = 0.00390625;
  baseline_solver.returned_diagnostics.failure_code.clear();
  baseline_solver.returned_diagnostics.summary = "recorded amf-options eta-mode solve";
  solver.returned_diagnostics = baseline_solver.returned_diagnostics;

  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            amf_options.amf_modes,
                                            MakeKiraReductionOptions(),
                                            baseline_layout,
                                            baseline_kira_path,
                                            baseline_fermat_path,
                                            baseline_solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits,
                                            eta_symbol);

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "AmfOptions eta-mode solver handoff should not mutate the input problem spec");
  Expect(baseline_solver.call_count() == 1 && solver.call_count() == 1,
         "AmfOptions eta-mode solver handoff should match the direct builtin-list baseline with "
         "one solver call");
  const amflow::SolveRequest& baseline_request = baseline_solver.last_request();
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(baseline_request.system, baseline_system),
         "AmfOptions eta-mode solver handoff should preserve the direct Propagator-planned "
         "eta DESystem on the stub-first builtin-list baseline");
  Expect(SameDESystem(request.system, baseline_request.system),
         "AmfOptions eta-mode solver handoff should forward the same selected Propagator eta "
         "DESystem as the direct builtin-list baseline");
  Expect(request.start_location == baseline_request.start_location &&
             request.start_location == start_location,
         "AmfOptions eta-mode solver handoff should preserve the start location unchanged");
  Expect(request.target_location == baseline_request.target_location &&
             request.target_location == target_location,
         "AmfOptions eta-mode solver handoff should preserve the target location unchanged");
  Expect(SamePrecisionPolicy(request.precision_policy, baseline_request.precision_policy) &&
             SamePrecisionPolicy(request.precision_policy, precision_policy),
         "AmfOptions eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == baseline_request.requested_digits &&
             request.requested_digits == requested_digits,
         "AmfOptions eta-mode solver handoff should preserve requested_digits unchanged");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics) &&
             SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "AmfOptions eta-mode solver handoff should return the same solver diagnostics as the "
         "direct builtin-list baseline");
}

void SolveAmfOptionsEtaModeSeriesMassShortCircuitTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"Mass", "Propagator"});
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=4/9";
  const std::string target_location = "rho=25/27";
  const int requested_digits = 83;
  const std::string eta_symbol = "rho";
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-mass-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path,
                                        eta_symbol);

  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-amf-options-eta-mode-mass"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver baseline_solver;
  RecordingSeriesSolver solver;
  baseline_solver.returned_diagnostics.success = true;
  baseline_solver.returned_diagnostics.residual_norm = 0.0009765625;
  baseline_solver.returned_diagnostics.overlap_mismatch = 0.001953125;
  baseline_solver.returned_diagnostics.failure_code.clear();
  baseline_solver.returned_diagnostics.summary = "recorded amf-options mass solve";
  solver.returned_diagnostics = baseline_solver.returned_diagnostics;

  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            amf_options.amf_modes,
                                            MakeKiraReductionOptions(),
                                            baseline_layout,
                                            baseline_kira_path,
                                            baseline_fermat_path,
                                            baseline_solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits,
                                            eta_symbol);

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "AmfOptions Mass short-circuit should not mutate the input problem spec");
  Expect(baseline_solver.call_count() == 1 && solver.call_count() == 1,
         "AmfOptions Mass short-circuit should stop after the first planning-successful Mass "
         "mode with one solver call");
  const amflow::SolveRequest& baseline_request = baseline_solver.last_request();
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(baseline_request.system, baseline_system),
         "AmfOptions Mass short-circuit should preserve the direct Mass-planned eta DESystem on "
         "the builtin-list baseline");
  Expect(SameDESystem(request.system, baseline_request.system),
         "AmfOptions Mass short-circuit should forward the same selected Mass eta DESystem as "
         "the direct builtin-list baseline");
  Expect(request.start_location == baseline_request.start_location &&
             request.start_location == start_location,
         "AmfOptions Mass short-circuit should preserve the start location unchanged");
  Expect(request.target_location == baseline_request.target_location &&
             request.target_location == target_location,
         "AmfOptions Mass short-circuit should preserve the target location unchanged");
  Expect(SamePrecisionPolicy(request.precision_policy, baseline_request.precision_policy) &&
             SamePrecisionPolicy(request.precision_policy, precision_policy),
         "AmfOptions Mass short-circuit should preserve every precision-policy field");
  Expect(request.requested_digits == baseline_request.requested_digits &&
             request.requested_digits == requested_digits,
         "AmfOptions Mass short-circuit should preserve requested_digits unchanged");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics) &&
             SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "AmfOptions Mass short-circuit should return the same solver diagnostics as the direct "
         "builtin-list baseline");
}

void SolveAmfOptionsEtaModeSeriesBootstrapSolverStopsAfterFirstCompletedModeTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"All", "Mass"});
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=-11/14";
  const std::string target_location = "rho=31/37";
  const int requested_digits = 87;
  const std::string eta_symbol = "rho";

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-solver-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-solver-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::BootstrapSeriesSolver solver;

  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            amf_options.amf_modes,
                                            MakeKiraReductionOptions(),
                                            baseline_layout,
                                            baseline_kira_path,
                                            baseline_fermat_path,
                                            solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits,
                                            eta_symbol);

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics),
         "AmfOptions eta-mode solver handoff should preserve the direct builtin-list bootstrap "
         "diagnostics unchanged");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics),
         "AmfOptions eta-mode solver handoff should stop after the first completed builtin "
         "mode and preserve the baseline bootstrap solver diagnostics exactly");
}

void SolveAmfOptionsEtaModeSeriesUsesDefaultAmfModeListTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::AmfOptions amf_options;
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=4/9";
  const std::string target_location = "rho=25/27";
  const int requested_digits = 83;
  const std::string eta_symbol = "rho";
  const auto baseline_custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-default-modes-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-default-modes"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver baseline_solver;
  RecordingSeriesSolver solver;
  baseline_solver.returned_diagnostics.success = true;
  baseline_solver.returned_diagnostics.residual_norm = 0.001953125;
  baseline_solver.returned_diagnostics.overlap_mismatch = 0.00390625;
  baseline_solver.returned_diagnostics.failure_code.clear();
  baseline_solver.returned_diagnostics.summary = "recorded default amf-mode eta solve";
  solver.returned_diagnostics = baseline_solver.returned_diagnostics;

  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveBuiltinEtaModeListSeries(spec,
                                            master_basis,
                                            amf_options.amf_modes,
                                            MakeKiraReductionOptions(),
                                            baseline_layout,
                                            baseline_kira_path,
                                            baseline_fermat_path,
                                            baseline_solver,
                                            start_location,
                                            target_location,
                                            precision_policy,
                                            requested_digits,
                                            eta_symbol);

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "AmfOptions eta-mode solver handoff should not mutate the input problem spec");
  Expect(amf_options.amf_modes ==
             std::vector<std::string>{"Prescription", "Mass", "Propagator"},
         "AmfOptions eta-mode solver handoff should preserve the default builtin mode order");
  Expect(baseline_solver.call_count() == 1 && solver.call_count() == 1,
         "AmfOptions eta-mode solver handoff should succeed through the default Prescription "
         "builtin with one solver call");
  const amflow::SolveRequest& baseline_request = baseline_solver.last_request();
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_request.system),
         "AmfOptions eta-mode solver handoff should forward the same selected builtin eta "
         "DESystem as the direct builtin-list baseline");
  Expect(request.start_location == baseline_request.start_location &&
             request.start_location == start_location,
         "AmfOptions eta-mode solver handoff should preserve the start location unchanged");
  Expect(request.target_location == baseline_request.target_location &&
             request.target_location == target_location,
         "AmfOptions eta-mode solver handoff should preserve the target location unchanged");
  Expect(SamePrecisionPolicy(request.precision_policy, baseline_request.precision_policy) &&
             SamePrecisionPolicy(request.precision_policy, precision_policy),
         "AmfOptions eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == baseline_request.requested_digits &&
             request.requested_digits == requested_digits,
         "AmfOptions eta-mode solver handoff should preserve requested_digits unchanged");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics) &&
             SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "AmfOptions eta-mode solver handoff should return the same solver diagnostics as the "
         "direct builtin-list baseline");
}

void SolveAmfOptionsEtaModeSeriesRejectsEmptyAmfModeListTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::AmfOptions amf_options = MakePoisonedAmfOptions({});
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-amf-options-eta-mode-empty"));
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &amf_options, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(
            spec,
            master_basis,
            amf_options,
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "builtin eta-mode list must not be empty",
      "AmfOptions eta-mode solver handoff should preserve empty builtin-list diagnostics");

  Expect(solver.call_count() == 0,
         "AmfOptions eta-mode solver handoff should not call the solver when amf_modes is "
         "empty");
}

void SolveAmfOptionsEtaModeSeriesRejectsUnknownBuiltinNameImmediatelyTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"NotAMode", "All"});
  amflow::ParsedMasterList master_basis;
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-unknown-name"));
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &amf_options, &master_basis, &layout, &solver]() {
        static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(
            spec,
            master_basis,
            amf_options,
            MakeKiraReductionOptions(),
            layout,
            layout.root / "bin" / "unused-kira.sh",
            layout.root / "bin" / "unused-fermat.sh",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "unknown eta mode: NotAMode",
      "AmfOptions eta-mode solver handoff should preserve unknown builtin-name diagnostics");

  Expect(solver.call_count() == 0,
         "AmfOptions eta-mode solver handoff should not call the solver when builtin-name "
         "resolution fails");
}

void SolveAmfOptionsEtaModeSeriesExecutionFailureAfterFallbackTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"Propagator"});
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-eta-mode-solver-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-amf-options-eta-mode-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(spec,
                                                           master_basis,
                                                           amf_options,
                                                           MakeKiraReductionOptions(),
                                                           layout,
                                                           kira_path,
                                                           fermat_path,
                                                           solver,
                                                           "eta=0",
                                                           "eta=1",
                                                           MakeDistinctPrecisionPolicy(),
                                                           55));
    throw std::runtime_error("AmfOptions eta-mode solver handoff should preserve upstream eta "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "AmfOptions eta-mode solver handoff should preserve reducer execution failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "AmfOptions eta-mode solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "AmfOptions eta-mode solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "AmfOptions eta-mode solver handoff should preserve reducer stderr log diagnostics");
  }

  Expect(solver.call_count() == 0,
         "AmfOptions eta-mode solver handoff should not call the solver when eta DE "
         "construction fails after builtin selection");
}

void SolveAmfOptionsEtaModeSeriesBootstrapPreflightFailureTest() {
  const amflow::ProblemSpec spec = MakeUnsupportedBranchLoopGrammarSpec();
  amflow::ParsedMasterList master_basis;
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({mode_name, "Propagator"});
    const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
        FreshTempDir("amflow-bootstrap-amf-options-eta-mode-" + mode_name + "-preflight-failure"));
    RecordingSeriesSolver solver;

    const std::string message = CaptureRuntimeErrorMessage(
        [&spec, &amf_options, &master_basis, &layout, &solver]() {
          static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(
              spec,
              master_basis,
              amf_options,
              MakeKiraReductionOptions(),
              layout,
              layout.root / "bin" / "unused-kira.sh",
              layout.root / "bin" / "unused-fermat.sh",
              solver,
              "eta=0",
              "eta=1",
              MakeDistinctPrecisionPolicy(),
              55));
        },
        "AmfOptions eta-mode solver handoff should preserve " + mode_name +
            " preflight failures");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "AmfOptions eta-mode solver handoff should preserve " + mode_name +
            " preflight failures");

    Expect(solver.call_count() == 0,
           "AmfOptions eta-mode solver handoff should not call the solver when " + mode_name +
               " planning fails");
  }
}

void SolveResolvedEtaModeSeriesBuiltinHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "eta=5/12";
  const std::string target_location = "eta=13/17";
  const int requested_digits = 89;

  amflow::EtaInsertionDecision custom_decision;
  custom_decision.mode_name = "CustomMode";
  custom_decision.selected_propagator_indices = {6};
  custom_decision.selected_propagators = {spec.family.propagators[6].expression};
  custom_decision.explanation = "unused custom mode";
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(custom_decision, "CustomMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-builtin-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-builtin"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.0009765625;
  solver.returned_diagnostics.overlap_mismatch = 0.001953125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded resolved builtin eta-mode solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveResolvedEtaModeSeries(spec,
                                         master_basis,
                                         "All",
                                         {custom_mode},
                                         MakeKiraReductionOptions(),
                                         layout,
                                         kira_path,
                                         fermat_path,
                                         solver,
                                         start_location,
                                         target_location,
                                         precision_policy,
                                         requested_digits);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "resolved eta-mode solver handoff should not mutate the input problem spec");
  Expect(custom_mode->call_count() == 0,
         "resolved eta-mode solver handoff should not plan unrelated user-defined modes when a "
         "builtin name resolves cleanly");
  Expect(solver.call_count() == 1,
         "resolved eta-mode solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "resolved eta-mode solver handoff should preserve builtin All DESystem construction");
  Expect(request.start_location == start_location && request.target_location == target_location,
         "resolved eta-mode solver handoff should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "resolved eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "resolved eta-mode solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "resolved eta-mode solver handoff should return solver diagnostics verbatim");
}

void SolveResolvedEtaModeSeriesUserDefinedHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=7/13";
  const std::string target_location = "rho=29/31";
  const int requested_digits = 91;
  const std::string eta_symbol = "rho";

  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-user-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path,
                                        eta_symbol);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-user"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.00048828125;
  solver.returned_diagnostics.overlap_mismatch = 0.0009765625;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded resolved user-defined eta-mode solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveResolvedEtaModeSeries(spec,
                                         master_basis,
                                         "CustomMode",
                                         {custom_mode},
                                         MakeKiraReductionOptions(),
                                         layout,
                                         kira_path,
                                         fermat_path,
                                         solver,
                                         start_location,
                                         target_location,
                                         precision_policy,
                                         requested_digits,
                                         eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "resolved eta-mode solver handoff should not mutate the input problem spec");
  Expect(custom_mode->call_count() == 1,
         "resolved eta-mode solver handoff should plan the selected user-defined mode exactly "
         "once");
  Expect(custom_mode->last_planned_spec_yaml() == original_yaml,
         "resolved eta-mode solver handoff should plan against the original problem spec");
  Expect(solver.call_count() == 1,
         "resolved eta-mode solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "resolved eta-mode solver handoff should forward the selected user-defined eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location && request.target_location == target_location,
         "resolved eta-mode solver handoff should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "resolved eta-mode solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "resolved eta-mode solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "resolved eta-mode solver handoff should return solver diagnostics verbatim");
}

void SolveResolvedEtaModeSeriesRejectsUnknownNameWithUserDefinedRegistryTest() {
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&custom_mode, &solver]() {
        static_cast<void>(amflow::SolveResolvedEtaModeSeries(amflow::MakeSampleProblemSpec(),
                                                             amflow::ParsedMasterList{},
                                                             "MissingMode",
                                                             {custom_mode},
                                                             MakeKiraReductionOptions(),
                                                             amflow::EnsureArtifactLayout(
                                                                 FreshTempDir(
                                                                     "amflow-bootstrap-resolved-eta-mode-missing")),
                                                             "unused-kira",
                                                             "unused-fermat",
                                                             solver,
                                                             "eta=0",
                                                             "eta=1",
                                                             MakeDistinctPrecisionPolicy(),
                                                             55));
      },
      "unknown eta mode: MissingMode",
      "resolved eta-mode solver handoff should preserve unknown-name diagnostics");
  Expect(custom_mode->call_count() == 0,
         "resolved eta-mode solver handoff should not plan user-defined modes when resolution "
         "fails");
  Expect(solver.call_count() == 0,
         "resolved eta-mode solver handoff should not call the solver when resolution fails");
}

void SolveResolvedEtaModeSeriesRejectsRegistryValidationFailureTest() {
  const auto first_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  const auto second_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&first_mode, &second_mode, &solver]() {
        static_cast<void>(amflow::SolveResolvedEtaModeSeries(
            amflow::MakeSampleProblemSpec(),
            amflow::ParsedMasterList{},
            "All",
            {first_mode, second_mode},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-resolved-eta-mode-registry-failure")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "duplicate user-defined eta mode: CustomMode",
      "resolved eta-mode solver handoff should preserve registry-validation diagnostics");
  Expect(first_mode->call_count() == 0 && second_mode->call_count() == 0,
         "resolved eta-mode solver handoff should not plan user-defined modes when registry "
         "validation fails");
  Expect(solver.call_count() == 0,
         "resolved eta-mode solver handoff should not call the solver when registry validation "
         "fails");
}

void SolveResolvedEtaModeSeriesPlanningFailureTest() {
  const auto failing_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(), "RetryMode", "retry eta planning failed");
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&failing_mode, &solver]() {
        static_cast<void>(amflow::SolveResolvedEtaModeSeries(
            amflow::MakeSampleProblemSpec(),
            amflow::ParsedMasterList{},
            "RetryMode",
            {failing_mode},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-resolved-eta-mode-plan-failure")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "retry eta planning failed",
      "resolved eta-mode solver handoff should preserve user-defined planning failures");
  Expect(failing_mode->call_count() == 1,
         "resolved eta-mode solver handoff should plan the selected user-defined mode exactly "
         "once before propagating failure");
  Expect(solver.call_count() == 0,
         "resolved eta-mode solver handoff should not call the solver when planning fails");
}

void SolveResolvedEtaModeListSeriesHappyPathFallbackTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=2/5";
  const std::string target_location = "rho=7/8";
  const int requested_digits = 97;
  const std::string eta_symbol = "rho";

  const auto retry_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(),
      "RetryMode",
      "retry eta planning failed",
      PlanningFailureKind::InvalidArgument);
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        MakeEtaGeneratedHappyDecision(),
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path,
                                        eta_symbol);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.000244140625;
  solver.returned_diagnostics.overlap_mismatch = 0.00048828125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded resolved eta-mode list solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveResolvedEtaModeListSeries(spec,
                                             master_basis,
                                             {"RetryMode", "CustomMode"},
                                             {retry_mode, custom_mode},
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "resolved eta-mode list solver handoff should not mutate the input problem spec");
  Expect(retry_mode->call_count() == 1,
         "resolved eta-mode list solver handoff should plan the failing first mode exactly once");
  Expect(custom_mode->call_count() == 1,
         "resolved eta-mode list solver handoff should plan the selected mode exactly once");
  Expect(custom_mode->last_planned_spec_yaml() == original_yaml,
         "resolved eta-mode list solver handoff should plan the selected mode against the "
         "original problem spec");
  Expect(solver.call_count() == 1,
         "resolved eta-mode list solver handoff should call the supplied solver exactly once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "resolved eta-mode list solver handoff should forward the selected eta DESystem "
         "unchanged into SolveRequest");
  Expect(request.start_location == start_location && request.target_location == target_location,
         "resolved eta-mode list solver handoff should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "resolved eta-mode list solver handoff should preserve every precision-policy field");
  Expect(request.requested_digits == requested_digits,
         "resolved eta-mode list solver handoff should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "resolved eta-mode list solver handoff should return solver diagnostics verbatim");
}

void SolveResolvedEtaModeListSeriesBootstrapPreflightFailureTest() {
  const amflow::ProblemSpec spec = MakeUnsupportedBranchLoopGrammarSpec();
  amflow::ParsedMasterList master_basis;
  for (const std::string& mode_name : std::vector<std::string>{"Branch", "Loop"}) {
    const auto custom_mode =
        std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
    const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
        FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-" + mode_name + "-preflight-failure"));
    RecordingSeriesSolver solver;

    const std::string message = CaptureRuntimeErrorMessage(
        [&spec, &master_basis, &layout, &solver, &mode_name, &custom_mode]() {
          static_cast<void>(amflow::SolveResolvedEtaModeListSeries(
              spec,
              master_basis,
              {mode_name, "CustomMode"},
              {custom_mode},
              MakeKiraReductionOptions(),
              layout,
              layout.root / "bin" / "unused-kira.sh",
              layout.root / "bin" / "unused-fermat.sh",
              solver,
              "eta=0",
              "eta=1",
              MakeDistinctPrecisionPolicy(),
              55));
        },
        "resolved eta-mode-list solver handoff should preserve " + mode_name +
            " preflight failures");
    ExpectBranchLoopBootstrapBlockerMessage(
        message,
        mode_name,
        "resolved eta-mode-list solver handoff should preserve " + mode_name +
            " preflight failures");

    Expect(custom_mode->call_count() == 0,
           "resolved eta-mode-list solver handoff should not call the later selected mode when " +
               mode_name + " planning fails");
    Expect(solver.call_count() == 0,
           "resolved eta-mode-list solver handoff should not call the solver when " + mode_name +
               " planning fails");
  }
}

void SolveResolvedEtaModeListSeriesBuiltinMassShortCircuitTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinMassHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=2/5";
  const std::string target_location = "rho=7/8";
  const int requested_digits = 97;
  const std::string eta_symbol = "rho";

  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  const amflow::EtaInsertionDecision baseline_decision =
      amflow::MakeBuiltinEtaMode("Mass")->Plan(spec);

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-mass-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::DESystem baseline_system =
      amflow::BuildEtaGeneratedDESystem(spec,
                                        master_basis,
                                        baseline_decision,
                                        MakeKiraReductionOptions(),
                                        baseline_layout,
                                        baseline_kira_path,
                                        baseline_fermat_path,
                                        eta_symbol);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-mass"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics.success = true;
  solver.returned_diagnostics.residual_norm = 0.000244140625;
  solver.returned_diagnostics.overlap_mismatch = 0.00048828125;
  solver.returned_diagnostics.failure_code.clear();
  solver.returned_diagnostics.summary = "recorded resolved eta-mode list mass solve";

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveResolvedEtaModeListSeries(spec,
                                             master_basis,
                                             {"Mass", "CustomMode"},
                                             {custom_mode},
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "resolved eta-mode list Mass short-circuit should not mutate the input problem spec");
  Expect(custom_mode->call_count() == 0,
         "resolved eta-mode list Mass short-circuit should not plan later user-defined modes "
         "once builtin Mass selects a reviewed candidate group");
  Expect(solver.call_count() == 1,
         "resolved eta-mode list Mass short-circuit should call the supplied solver exactly "
         "once");
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_system),
         "resolved eta-mode list Mass short-circuit should forward the selected Mass eta "
         "DESystem unchanged into SolveRequest");
  Expect(request.start_location == start_location && request.target_location == target_location,
         "resolved eta-mode list Mass short-circuit should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, precision_policy),
         "resolved eta-mode list Mass short-circuit should preserve every precision-policy "
         "field");
  Expect(request.requested_digits == requested_digits,
         "resolved eta-mode list Mass short-circuit should preserve requested_digits");
  Expect(SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "resolved eta-mode list Mass short-circuit should return solver diagnostics verbatim");
}

void SolveResolvedEtaModeListSeriesStopsAfterFirstCompletedModeTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=-4/7";
  const std::string target_location = "rho=19/23";
  const int requested_digits = 101;
  const std::string eta_symbol = "rho";

  const auto first_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "FirstMode");
  const auto second_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "SecondMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::SolveRequest baseline_request;
  baseline_request.system = amflow::BuildEtaGeneratedDESystem(spec,
                                                             master_basis,
                                                             MakeEtaGeneratedHappyDecision(),
                                                             MakeKiraReductionOptions(),
                                                             baseline_layout,
                                                             baseline_kira_path,
                                                             baseline_fermat_path,
                                                             eta_symbol);
  baseline_request.start_location = start_location;
  baseline_request.target_location = target_location;
  baseline_request.precision_policy = precision_policy;
  baseline_request.requested_digits = requested_digits;

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics expected = solver.Solve(baseline_request);

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveResolvedEtaModeListSeries(spec,
                                             master_basis,
                                             {"FirstMode", "SecondMode"},
                                             {first_mode, second_mode},
                                             MakeKiraReductionOptions(),
                                             layout,
                                             kira_path,
                                             fermat_path,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol);

  Expect(first_mode->call_count() == 1,
         "resolved eta-mode list solver handoff should stop after the first completed mode");
  Expect(second_mode->call_count() == 0,
         "resolved eta-mode list solver handoff should not plan later modes once selection "
         "completes");
  Expect(SameSolverDiagnostics(diagnostics, expected),
         "resolved eta-mode list solver handoff should preserve bootstrap solver passthrough "
         "after the first completed mode");
}

void SolveResolvedEtaModeListSeriesRejectsEmptyModeListTest() {
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&solver]() {
        static_cast<void>(amflow::SolveResolvedEtaModeListSeries(
            amflow::MakeSampleProblemSpec(),
            amflow::ParsedMasterList{},
            {},
            {},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-empty")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "eta-mode list must not be empty",
      "resolved eta-mode list solver handoff should reject empty mode lists deterministically");
  Expect(solver.call_count() == 0,
         "resolved eta-mode list solver handoff should not call the solver when the mode list "
         "is empty");
}

void SolveResolvedEtaModeListSeriesRejectsUnknownNameImmediatelyTest() {
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&custom_mode, &solver]() {
        static_cast<void>(amflow::SolveResolvedEtaModeListSeries(
            amflow::MakeSampleProblemSpec(),
            amflow::ParsedMasterList{},
            {"MissingMode", "CustomMode"},
            {custom_mode},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-missing")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "unknown eta mode: MissingMode",
      "resolved eta-mode list solver handoff should preserve unknown-name diagnostics");
  Expect(custom_mode->call_count() == 0,
         "resolved eta-mode list solver handoff should not plan any mode when resolution fails "
         "immediately");
  Expect(solver.call_count() == 0,
         "resolved eta-mode list solver handoff should not call the solver when resolution "
         "fails immediately");
}

void SolveResolvedEtaModeListSeriesExhaustedKnownModesPreservesLastDiagnosticTest() {
  const auto retry_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(), "RetryMode", "retry eta planning failed");
  const auto last_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(), "LastMode", "last eta planning failed");
  RecordingSeriesSolver solver;

  ExpectRuntimeError(
      [&retry_mode, &last_mode, &solver]() {
        static_cast<void>(amflow::SolveResolvedEtaModeListSeries(
            amflow::MakeSampleProblemSpec(),
            amflow::ParsedMasterList{},
            {"RetryMode", "LastMode"},
            {retry_mode, last_mode},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-exhausted")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "last eta planning failed",
      "resolved eta-mode list solver handoff should preserve the final planning diagnostic when "
      "every mode fails during selection");
  Expect(retry_mode->call_count() == 1 && last_mode->call_count() == 1,
         "resolved eta-mode list solver handoff should plan each failing mode exactly once "
         "before exhausting the list");
  Expect(solver.call_count() == 0,
         "resolved eta-mode list solver handoff should not call the solver when every mode "
         "fails during selection");
}

void SolveResolvedEtaModeListSeriesExecutionFailureStopsIterationTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-resolved-eta-mode-list-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-resolved-eta-mode-list-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const auto first_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "FirstMode");
  const auto second_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "SecondMode");
  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveResolvedEtaModeListSeries(amflow::MakeSampleProblemSpec(),
                                                             master_basis,
                                                             {"FirstMode", "SecondMode"},
                                                             {first_mode, second_mode},
                                                             MakeKiraReductionOptions(),
                                                             layout,
                                                             kira_path,
                                                             fermat_path,
                                                             solver,
                                                             "eta=0",
                                                             "eta=1",
                                                             MakeDistinctPrecisionPolicy(),
                                                             55));
    throw std::runtime_error("resolved eta-mode list solver handoff should preserve upstream eta "
                             "DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "resolved eta-mode list solver handoff should preserve reducer execution failure "
           "text");
    Expect(message.find("status=completed") != std::string::npos,
           "resolved eta-mode list solver handoff should preserve reducer execution status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "resolved eta-mode list solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "resolved eta-mode list solver handoff should preserve reducer stderr log "
           "diagnostics");
  }

  Expect(first_mode->call_count() == 1,
         "resolved eta-mode list solver handoff should not fall through after downstream eta "
         "construction has started");
  Expect(second_mode->call_count() == 0,
         "resolved eta-mode list solver handoff should not plan later modes after the selected "
         "mode reaches downstream eta construction");
  Expect(solver.call_count() == 0,
         "resolved eta-mode list solver handoff should not call the solver when eta DE "
         "construction fails after mode selection");
}

void SolveAmfOptionsEtaModeSeriesWithUserDefinedModesHappyPathTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::AmfOptions amf_options =
      MakePoisonedAmfOptions({"RetryMode", "CustomMode"});
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=9/14";
  const std::string target_location = "rho=41/43";
  const int requested_digits = 103;
  const std::string eta_symbol = "rho";

  const auto baseline_retry_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(),
      "RetryMode",
      "retry eta planning failed",
      PlanningFailureKind::InvalidArgument);
  const auto baseline_custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver baseline_solver;
  baseline_solver.returned_diagnostics.success = true;
  baseline_solver.returned_diagnostics.residual_norm = 0.0001220703125;
  baseline_solver.returned_diagnostics.overlap_mismatch = 0.000244140625;
  baseline_solver.returned_diagnostics.failure_code.clear();
  baseline_solver.returned_diagnostics.summary = "recorded AmfOptions resolved eta-mode solve";

  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveResolvedEtaModeListSeries(spec,
                                             master_basis,
                                             amf_options.amf_modes,
                                             {baseline_retry_mode, baseline_custom_mode},
                                             MakeKiraReductionOptions(),
                                             baseline_layout,
                                             baseline_kira_path,
                                             baseline_fermat_path,
                                             baseline_solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol);

  const auto retry_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(),
      "RetryMode",
      "retry eta planning failed",
      PlanningFailureKind::InvalidArgument);
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  RecordingSeriesSolver solver;
  solver.returned_diagnostics = baseline_solver.returned_diagnostics;

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           {retry_mode, custom_mode},
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "AmfOptions resolved eta-mode solver handoff should not mutate the input problem spec");
  Expect(retry_mode->call_count() == 1 && custom_mode->call_count() == 1,
         "AmfOptions resolved eta-mode solver handoff should preserve ordered fallback with one "
         "planning probe per configured mode");
  Expect(baseline_solver.call_count() == 1 && solver.call_count() == 1,
         "AmfOptions resolved eta-mode solver handoff should match the direct mixed-list "
         "baseline with one solver call");
  const amflow::SolveRequest& baseline_request = baseline_solver.last_request();
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_request.system),
         "AmfOptions resolved eta-mode solver handoff should forward the same selected eta "
         "DESystem as the direct mixed-list baseline");
  Expect(request.start_location == baseline_request.start_location &&
             request.start_location == start_location &&
             request.target_location == baseline_request.target_location &&
             request.target_location == target_location,
         "AmfOptions resolved eta-mode solver handoff should preserve solver locations");
  Expect(SamePrecisionPolicy(request.precision_policy, baseline_request.precision_policy) &&
             SamePrecisionPolicy(request.precision_policy, precision_policy),
         "AmfOptions resolved eta-mode solver handoff should preserve every precision-policy "
         "field");
  Expect(request.requested_digits == baseline_request.requested_digits &&
             request.requested_digits == requested_digits,
         "AmfOptions resolved eta-mode solver handoff should preserve requested_digits "
         "unchanged");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics) &&
             SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "AmfOptions resolved eta-mode solver handoff should return the same solver diagnostics "
         "as the direct mixed-list baseline");
}

void SolveAmfOptionsEtaModeSeriesWithUserDefinedModesBootstrapSolverStopsAfterFirstCompletedModeTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::AmfOptions amf_options =
      MakePoisonedAmfOptions({"FirstMode", "SecondMode"});
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=-17/19";
  const std::string target_location = "rho=47/53";
  const int requested_digits = 107;
  const std::string eta_symbol = "rho";

  const auto baseline_first_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "FirstMode");
  const auto baseline_second_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "SecondMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-bootstrap-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::BootstrapSeriesSolver solver;
  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveResolvedEtaModeListSeries(spec,
                                             master_basis,
                                             amf_options.amf_modes,
                                             {baseline_first_mode, baseline_second_mode},
                                             MakeKiraReductionOptions(),
                                             baseline_layout,
                                             baseline_kira_path,
                                             baseline_fermat_path,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol);

  const auto first_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "FirstMode");
  const auto second_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "SecondMode");

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-bootstrap"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           {first_mode, second_mode},
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(first_mode->call_count() == 1 && second_mode->call_count() == 0,
         "AmfOptions resolved eta-mode solver handoff should stop after the first completed "
         "configured mode");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics),
         "AmfOptions resolved eta-mode solver handoff should preserve direct mixed-list "
         "bootstrap diagnostics unchanged");
}

void SolveAmfOptionsEtaModeSeriesWithUserDefinedModesUsesDefaultAmfModeListTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = MakeBuiltinAllEtaHappySpec();
  const std::string original_yaml = amflow::SerializeProblemSpecYaml(spec);
  const amflow::AmfOptions amf_options;
  const amflow::PrecisionPolicy precision_policy = MakeDistinctPrecisionPolicy();
  const std::string start_location = "rho=4/9";
  const std::string target_location = "rho=25/27";
  const int requested_digits = 83;
  const std::string eta_symbol = "rho";
  const auto baseline_custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");

  const amflow::ArtifactLayout baseline_layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-default-baseline"));
  const std::filesystem::path baseline_kira_path =
      baseline_layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path baseline_fermat_path =
      baseline_layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(baseline_kira_path.parent_path());
  WriteExecutableScript(baseline_kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(baseline_fermat_path, "#!/bin/sh\nexit 0\n");

  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-default"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeFixtureCopyScript(fixture_root));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  const std::vector<std::shared_ptr<amflow::EtaMode>> baseline_user_defined_modes = {
      baseline_custom_mode};
  const std::vector<std::shared_ptr<amflow::EtaMode>> user_defined_modes = {custom_mode};

  RecordingSeriesSolver baseline_solver;
  RecordingSeriesSolver solver;
  baseline_solver.returned_diagnostics.success = true;
  baseline_solver.returned_diagnostics.residual_norm = 0.001953125;
  baseline_solver.returned_diagnostics.overlap_mismatch = 0.00390625;
  baseline_solver.returned_diagnostics.failure_code.clear();
  baseline_solver.returned_diagnostics.summary = "recorded default resolved eta solve";
  solver.returned_diagnostics = baseline_solver.returned_diagnostics;

  const amflow::SolverDiagnostics baseline_diagnostics =
      amflow::SolveResolvedEtaModeListSeries(spec,
                                             master_basis,
                                             amf_options.amf_modes,
                                             baseline_user_defined_modes,
                                             MakeKiraReductionOptions(),
                                             baseline_layout,
                                             baseline_kira_path,
                                             baseline_fermat_path,
                                             baseline_solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol);

  const amflow::SolverDiagnostics diagnostics =
      amflow::SolveAmfOptionsEtaModeSeries(spec,
                                           master_basis,
                                           amf_options,
                                           user_defined_modes,
                                           MakeKiraReductionOptions(),
                                           layout,
                                           kira_path,
                                           fermat_path,
                                           solver,
                                           start_location,
                                           target_location,
                                           precision_policy,
                                           requested_digits,
                                           eta_symbol);

  Expect(amflow::SerializeProblemSpecYaml(spec) == original_yaml,
         "AmfOptions resolved eta-mode solver handoff should not mutate the input problem spec");
  Expect(amf_options.amf_modes ==
             std::vector<std::string>{"Prescription", "Mass", "Propagator"},
         "AmfOptions resolved eta-mode solver handoff should preserve the default amf_modes "
         "list unchanged");
  Expect(baseline_custom_mode->call_count() == 0 && custom_mode->call_count() == 0,
         "AmfOptions resolved eta-mode solver handoff should not consult the user-defined mode "
         "when builtin Prescription succeeds");
  Expect(baseline_solver.call_count() == 1 && solver.call_count() == 1,
         "AmfOptions resolved eta-mode solver handoff should succeed through the default "
         "Prescription builtin with one solver call");
  const amflow::SolveRequest& baseline_request = baseline_solver.last_request();
  const amflow::SolveRequest& request = solver.last_request();
  Expect(SameDESystem(request.system, baseline_request.system),
         "AmfOptions resolved eta-mode solver handoff should forward the same selected eta "
         "DESystem as the direct mixed-list baseline");
  Expect(request.start_location == baseline_request.start_location &&
             request.start_location == start_location,
         "AmfOptions resolved eta-mode solver handoff should preserve the start location "
         "unchanged");
  Expect(request.target_location == baseline_request.target_location &&
             request.target_location == target_location,
         "AmfOptions resolved eta-mode solver handoff should preserve the target location "
         "unchanged");
  Expect(SamePrecisionPolicy(request.precision_policy, baseline_request.precision_policy) &&
             SamePrecisionPolicy(request.precision_policy, precision_policy),
         "AmfOptions resolved eta-mode solver handoff should preserve every precision-policy "
         "field");
  Expect(request.requested_digits == baseline_request.requested_digits &&
             request.requested_digits == requested_digits,
         "AmfOptions resolved eta-mode solver handoff should preserve requested_digits "
         "unchanged");
  Expect(SameSolverDiagnostics(diagnostics, baseline_diagnostics) &&
             SameSolverDiagnostics(diagnostics, solver.returned_diagnostics),
         "AmfOptions resolved eta-mode solver handoff should return the same solver diagnostics "
         "as the direct mixed-list baseline");
}

void SolveAmfOptionsEtaModeSeriesWithUserDefinedModesRejectsEmptyAmfModeListTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({});
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &amf_options, &solver]() {
        static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(
            spec,
            amflow::ParsedMasterList{},
            amf_options,
            {},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-empty")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "eta-mode list must not be empty",
      "AmfOptions resolved eta-mode solver handoff should preserve empty mixed-list diagnostics");
  Expect(solver.call_count() == 0,
         "AmfOptions resolved eta-mode solver handoff should not call the solver when amf_modes "
         "is empty");
}

void SolveAmfOptionsEtaModeSeriesWithUserDefinedModesRejectsUnknownNameImmediatelyTest() {
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::AmfOptions amf_options = MakePoisonedAmfOptions({"MissingMode", "All"});
  RecordingSeriesSolver solver;

  ExpectInvalidArgument(
      [&spec, &amf_options, &solver]() {
        static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(
            spec,
            amflow::ParsedMasterList{},
            amf_options,
            {},
            MakeKiraReductionOptions(),
            amflow::EnsureArtifactLayout(
                FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-missing")),
            "unused-kira",
            "unused-fermat",
            solver,
            "eta=0",
            "eta=1",
            MakeDistinctPrecisionPolicy(),
            55));
      },
      "unknown eta mode: MissingMode",
      "AmfOptions resolved eta-mode solver handoff should preserve unknown-name diagnostics");
  Expect(solver.call_count() == 0,
         "AmfOptions resolved eta-mode solver handoff should not call the solver when mixed "
         "name resolution fails");
}

void SolveAmfOptionsEtaModeSeriesWithUserDefinedModesExecutionFailureAfterFallbackTest() {
  amflow::KiraBackend backend;
  const std::filesystem::path fixture_root = TestDataRoot() / "kira-results/eta-generated-happy";
  const amflow::ParsedMasterList master_basis =
      backend.ParseMasterList(fixture_root, "planar_double_box");
  const amflow::ProblemSpec spec = amflow::MakeSampleProblemSpec();
  const amflow::AmfOptions amf_options =
      MakePoisonedAmfOptions({"RetryMode", "CustomMode"});
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(
      FreshTempDir("amflow-bootstrap-amf-options-resolved-eta-mode-exec-failure"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-amf-options-resolved-eta-mode-solver-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const auto retry_mode = std::make_shared<RecordingEtaMode>(
      MakeEtaGeneratedHappyDecision(),
      "RetryMode",
      "retry eta planning failed",
      PlanningFailureKind::InvalidArgument);
  const auto custom_mode =
      std::make_shared<RecordingEtaMode>(MakeEtaGeneratedHappyDecision(), "CustomMode");
  RecordingSeriesSolver solver;

  try {
    static_cast<void>(amflow::SolveAmfOptionsEtaModeSeries(spec,
                                                           master_basis,
                                                           amf_options,
                                                           {retry_mode, custom_mode},
                                                           MakeKiraReductionOptions(),
                                                           layout,
                                                           kira_path,
                                                           fermat_path,
                                                           solver,
                                                           "eta=0",
                                                           "eta=1",
                                                           MakeDistinctPrecisionPolicy(),
                                                           55));
    throw std::runtime_error("AmfOptions resolved eta-mode solver handoff should preserve "
                             "upstream eta DESystem-construction failure");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    Expect(message.find("eta-generated DE construction requires successful reducer execution") !=
               std::string::npos,
           "AmfOptions resolved eta-mode solver handoff should preserve reducer execution "
           "failure text");
    Expect(message.find("status=completed") != std::string::npos,
           "AmfOptions resolved eta-mode solver handoff should preserve reducer execution "
           "status");
    Expect(message.find("exit_code=9") != std::string::npos,
           "AmfOptions resolved eta-mode solver handoff should preserve reducer exit code");
    Expect(message.find("stderr_log=") != std::string::npos &&
               message.find(layout.logs_dir.string()) != std::string::npos &&
               message.find(".stderr.log") != std::string::npos,
           "AmfOptions resolved eta-mode solver handoff should preserve reducer stderr log "
           "diagnostics");
  }

  Expect(retry_mode->call_count() == 1 && custom_mode->call_count() == 1,
         "AmfOptions resolved eta-mode solver handoff should preserve ordered fallback "
         "semantics before downstream eta execution starts");
  Expect(solver.call_count() == 0,
         "AmfOptions resolved eta-mode solver handoff should not call the solver when eta DE "
         "construction fails after mixed selection");
}

void BootstrapBuiltinSampleManifestTest() {
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-sample-manifest-test"));
  const amflow::ArtifactManifest manifest = amflow::MakeBootstrapManifest();
  const std::string yaml = amflow::SerializeArtifactManifestYaml(manifest);
  const std::filesystem::path path = amflow::WriteArtifactManifest(layout, manifest);

  Expect(manifest.manifest_kind == "sample-demo",
         "write-manifest should stay explicitly sample/demo");
  Expect(manifest.run_id == "sample-demo-manifest",
         "sample/demo manifest should not reuse the K0 packet run id");
  Expect(path == layout.manifests_dir / "sample-demo-manifest.yaml",
         "sample/demo manifest should use its own filename");
  Expect(std::filesystem::exists(path), "sample/demo manifest should be written");
  Expect(ReadFile(path) == yaml, "sample/demo manifest file should match the serialized YAML");
  Expect(yaml.find("manifest_kind: \"sample-demo\"\n") != std::string::npos,
         "sample/demo manifest YAML should declare its sample/demo kind");
  Expect(yaml.find("spec:\n  provenance: \"builtin sample/demo ProblemSpec\"\n") !=
             std::string::npos,
         "sample/demo manifest YAML should say it is builtin sample/demo data");
  Expect(yaml.find("run_id: \"bootstrap-run\"\n") == std::string::npos,
         "sample/demo manifest YAML should not reuse the K0 packet run id");
  Expect(yaml.find("automatic_vs_manual_k0_smoke") == std::string::npos,
         "sample/demo manifest YAML should not leak the K0 smoke packet identity");
  Expect(yaml.find("AMFlow 1.2") == std::string::npos,
         "sample/demo manifest YAML should not overclaim an upstream runtime snapshot");
}

void K0BootstrapManifestSerializationTest() {
  const std::filesystem::path spec_path =
      std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml";
  const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(spec_path);
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-k0-manifest-serialization"));
  const std::filesystem::path results_root =
      layout.generated_config_dir / "results" / "automatic_vs_manual_k0_smoke";
  const std::filesystem::path direct_results_root =
      layout.results_dir / "automatic_vs_manual_k0_smoke";
  std::filesystem::create_directories(results_root);
  std::filesystem::create_directories(direct_results_root);
  {
    std::ofstream stream(results_root / "masters");
    stream << "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0] 0\n";
  }
  {
    std::ofstream stream(results_root / "kira_target.m");
    stream << "{automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0] -> "
              "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0]}\n";
  }
  {
    std::ofstream stream(direct_results_root / "masters");
    stream << "automatic_vs_manual_k0_smoke[1,1,1,1,1,1,1,-3,0] 0\n";
  }

  amflow::ReductionOptions options = MakeKiraReductionOptions();
  const amflow::ArtifactManifest manifest = amflow::MakeFileBackedKiraRunManifest(
      {spec_path,
       "repo-local frozen K0 smoke fixture derived from preserved input",
       amflow::SerializeProblemSpecYaml(spec),
       spec.family.name,
       spec.targets.size(),
       layout.root,
       layout.generated_config_dir,
       layout.root / "bin" / "fake-kira.sh",
       layout.root / "bin" / "fake-fermat.sh",
       "/tmp/fake-kira jobs.yaml",
       "completed",
       0,
       std::filesystem::path(AMFLOW_SOURCE_DIR),
       "9cf233eb8c955961a8c78903d28f6899a40f48e4",
       "clean",
       1,
       {{"IBPReducer", options.ibp_reducer},
        {"IntegralOrder", std::to_string(options.integral_order)},
        {"ReductionMode", amflow::ToString(options.reduction_mode)},
        {"PermutationOption", std::to_string(*options.permutation_option)}},
       {{"PermutationOption", std::to_string(*options.permutation_option)}},
       layout.logs_dir / "kira.attempt-0001.stdout.log",
       layout.logs_dir / "kira.attempt-0001.stderr.log"});
  const std::string yaml = amflow::SerializeArtifactManifestYaml(manifest);
  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult parsed =
      backend.ParseReductionResult(layout.root, "automatic_vs_manual_k0_smoke");

  Expect(manifest.manifest_kind == "file-backed-kira-run",
         "file-backed K0 manifest should declare the file-backed run kind");
  Expect(manifest.run_id == "bootstrap-run",
         "file-backed K0 manifest should keep the retained packet run id");
  Expect(manifest.spec_path == std::filesystem::absolute(spec_path),
         "file-backed K0 manifest should preserve the exact spec path");
  Expect(manifest.family == "automatic_vs_manual_k0_smoke",
         "file-backed K0 manifest should preserve the K0 smoke family name");
  Expect(manifest.target_count == 4,
         "file-backed K0 manifest should preserve the frozen K0 target count");
  Expect(manifest.results_family_root == parsed.master_list.source_path.parent_path() &&
             manifest.results_family_root == results_root,
         "file-backed K0 manifest should preserve the same results-family root the parser "
         "actually resolves");
  Expect(manifest.masters_path == results_root / "masters",
         "file-backed K0 manifest should record the actual masters path");
  Expect(manifest.rule_path == results_root / "kira_target.m",
         "file-backed K0 manifest should record the actual rule path");
  Expect(manifest.non_default_options.size() == 1 &&
             manifest.non_default_options.at("PermutationOption") == "1",
         "file-backed K0 manifest should record only actual non-default reducer options");
  Expect(yaml.find("spec:\n  provenance: \"repo-local frozen K0 smoke fixture derived from "
                   "preserved input\"\n") != std::string::npos,
         "file-backed K0 manifest YAML should preserve the reviewed provenance boundary");
  Expect(yaml.find("results_family_root: \"" + results_root.string() + "\"\n") !=
             std::string::npos,
         "file-backed K0 manifest YAML should preserve the resolved results-family root");
  Expect(yaml.find("PermutationOption: \"1\"\n") != std::string::npos,
         "file-backed K0 manifest YAML should preserve the active non-default permutation "
         "option");
}

void RunKiraFromFileRefreshesManifestFromParsedReducerArtifactsTest() {
  const std::filesystem::path spec_path =
      std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml";
  const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(spec_path);
  const auto layout =
      amflow::EnsureArtifactLayout(FreshTempDir("amflow-bootstrap-k0-manifest-refresh"));
  const std::filesystem::path kira_path = layout.root / "bin" / "fake-kira-k0.sh";
  const std::filesystem::path fermat_path = layout.root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeK0SmokeFixtureCopyScript(true, true, 0));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  amflow::KiraBackend backend;
  const amflow::ReductionOptions options = MakeKiraReductionOptions();
  const auto preparation = backend.Prepare(spec, options, layout);
  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_path, fermat_path);

  const amflow::ArtifactManifest manifest = amflow::MakeFileBackedKiraRunManifest(
      {spec_path,
       "repo-local frozen K0 smoke fixture derived from preserved input",
       amflow::SerializeProblemSpecYaml(spec),
       spec.family.name,
       spec.targets.size(),
       layout.root,
       result.working_directory,
       kira_path,
       fermat_path,
       result.command,
       "completed",
       result.exit_code,
       std::filesystem::path(AMFLOW_SOURCE_DIR),
       "9cf233eb8c955961a8c78903d28f6899a40f48e4",
       "clean",
       1,
       {{"IBPReducer", options.ibp_reducer},
        {"IntegralOrder", std::to_string(options.integral_order)},
        {"ReductionMode", amflow::ToString(options.reduction_mode)},
        {"PermutationOption", std::to_string(*options.permutation_option)}},
       {{"PermutationOption", std::to_string(*options.permutation_option)}},
       result.stdout_log_path,
       result.stderr_log_path});
  const std::filesystem::path manifest_path = amflow::WriteArtifactManifest(layout, manifest);
  const std::string yaml = ReadFile(manifest_path);
  std::size_t manifest_count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(layout.manifests_dir)) {
    static_cast<void>(entry);
    ++manifest_count;
  }

  Expect(result.Succeeded(), "fake K0 file-backed Kira run should complete successfully");
  Expect(manifest_path == layout.manifests_dir / "bootstrap-run.yaml",
         "file-backed K0 run should write the retained bootstrap-run manifest name");
  Expect(manifest_count == 1,
         "file-backed K0 run should leave exactly one authoritative manifest in the packet");
  Expect(manifest.results_family_root ==
             layout.generated_config_dir / "results" / "automatic_vs_manual_k0_smoke",
         "file-backed K0 run should record the exact generated-config results tree used by the "
         "parser contract");
  Expect(yaml.find("stdout_log: \"" + result.stdout_log_path.string() + "\"\n") !=
             std::string::npos &&
             yaml.find("stderr_log: \"" + result.stderr_log_path.string() + "\"\n") !=
                 std::string::npos,
         "file-backed K0 manifest YAML should preserve the actual execution log paths");
  Expect(yaml.find("sample-planar-double-box") == std::string::npos &&
             yaml.find("AMFlow 1.2") == std::string::npos &&
             yaml.find("threads: 8") == std::string::npos,
         "file-backed K0 manifest YAML should not fall back to the old sample placeholder "
         "values");
}

void RunKiraFromFileNonzeroExitStillWritesTruthfulDefaultParseRootTest() {
  const std::filesystem::path cli_path = CurrentBuildBinaryPath("amflow-cli");
  const std::filesystem::path run_root =
      FreshTempDir("amflow-bootstrap-k0-manifest-nonzero-exit-cli");
  const std::filesystem::path kira_path = run_root / "bin" / "fake-kira-fail.sh";
  const std::filesystem::path fermat_path = run_root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(
      kira_path,
      "#!/bin/sh\n"
      "echo \"expected-k0-cli-failure\" 1>&2\n"
      "exit 9\n");
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const std::string command =
      ShellSingleQuote(cli_path.string()) + " run-kira-from-file " +
      ShellSingleQuote((std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml")
                           .string()) +
      " " + ShellSingleQuote(kira_path.string()) + " " + ShellSingleQuote(fermat_path.string()) +
      " " + ShellSingleQuote(run_root.string()) + " >/dev/null 2>&1";

  Expect(RunShellCommand(command) != 0,
         "nonzero fake Kira exit should keep the CLI run non-successful");

  const std::filesystem::path manifest_path = run_root / "manifests/bootstrap-run.yaml";
  const std::string yaml = ReadFile(manifest_path);
  const std::filesystem::path expected_results_root =
      run_root / "results/automatic_vs_manual_k0_smoke";

  Expect(std::filesystem::exists(manifest_path),
         "nonzero file-backed CLI run should still write the bootstrap manifest");
  Expect(yaml.find("status: \"completed\"\n") != std::string::npos &&
             yaml.find("exit_code: 9\n") != std::string::npos,
         "nonzero file-backed CLI run should preserve the real reducer exit status");
  Expect(yaml.find("results_family_root: \"" + expected_results_root.string() + "\"\n") !=
             std::string::npos,
         "nonzero file-backed CLI run should still record the parser-contract default "
         "results-family root");
  Expect(yaml.find("masters_path: ") == std::string::npos &&
             yaml.find("rule_path: ") == std::string::npos,
         "nonzero file-backed CLI run should not invent result-file paths when no outputs exist");
}

void RepoLocalSpecCopyDoesNotReceiveFrozenFixtureProvenanceTest() {
  const std::filesystem::path cli_path = CurrentBuildBinaryPath("amflow-cli");
  const std::filesystem::path temp_repo = FreshTempDir("amflow-bootstrap-k0-spec-copy-repo");
  std::filesystem::create_directories(temp_repo / ".git");
  const std::filesystem::path spec_copy = temp_repo / "other/problem-spec.k0-smoke.yaml";
  std::filesystem::create_directories(spec_copy.parent_path());
  {
    std::ofstream stream(spec_copy);
    stream << ReadFile(std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml");
  }
  const std::filesystem::path kira_path = temp_repo / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = temp_repo / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeK0SmokeFixtureCopyScript(true, false, 0));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const std::string command =
      ShellSingleQuote(cli_path.string()) + " run-kira-from-file " +
      ShellSingleQuote(spec_copy.string()) + " " + ShellSingleQuote(kira_path.string()) + " " +
      ShellSingleQuote(fermat_path.string()) + " " +
      ShellSingleQuote((temp_repo / "artifacts").string()) + " >/dev/null 2>&1";

  Expect(RunShellCommand(command) == 0,
         "repo-local spec copy should still be runnable through the CLI");

  const std::string yaml =
      ReadFile(temp_repo / "artifacts/manifests/bootstrap-run.yaml");
  Expect(yaml.find("provenance: \"repo-local frozen K0 smoke fixture derived from preserved "
                   "input\"\n") == std::string::npos,
         "repo-local copies of the K0 smoke spec should not inherit the reviewed frozen-fixture "
         "provenance label");
  Expect(yaml.find("provenance: \"repo-local file-backed ProblemSpec\"\n") != std::string::npos,
         "repo-local copies of the K0 smoke spec should use the generic repo-local provenance");
}

void ExternalSpecDoesNotClaimCleanRepoStatusWhenGitProbeUnavailableTest() {
  const std::filesystem::path cli_path = CurrentBuildBinaryPath("amflow-cli");
  const std::filesystem::path external_root =
      FreshTempDir("amflow-bootstrap-k0-external-spec-no-git");
  const std::filesystem::path spec_copy = external_root / "problem-spec.k0-smoke.yaml";
  {
    std::ofstream stream(spec_copy);
    stream << ReadFile(std::filesystem::path(AMFLOW_SOURCE_DIR) / "specs/problem-spec.k0-smoke.yaml");
  }
  const std::filesystem::path kira_path = external_root / "bin" / "fake-kira-copy.sh";
  const std::filesystem::path fermat_path = external_root / "bin" / "fake-fermat.sh";
  std::filesystem::create_directories(kira_path.parent_path());
  WriteExecutableScript(kira_path, MakeK0SmokeFixtureCopyScript(true, false, 0));
  WriteExecutableScript(fermat_path, "#!/bin/sh\nexit 0\n");

  const std::string command =
      ShellSingleQuote(cli_path.string()) + " run-kira-from-file " +
      ShellSingleQuote(spec_copy.string()) + " " + ShellSingleQuote(kira_path.string()) + " " +
      ShellSingleQuote(fermat_path.string()) + " " +
      ShellSingleQuote((external_root / "artifacts").string()) + " >/dev/null 2>&1";

  Expect(RunShellCommand(command) == 0,
         "external file-backed spec should still be runnable through the CLI");

  const std::string yaml =
      ReadFile(external_root / "artifacts/manifests/bootstrap-run.yaml");
  Expect(yaml.find("git_status_short: \"clean\"\n") == std::string::npos,
         "external file-backed specs should not claim a synthetic clean git status");
  Expect(yaml.find("repository:\n  root: ") == std::string::npos &&
             yaml.find("repository:\n  commit: ") == std::string::npos,
         "external file-backed specs should omit repository root/commit facts when the git "
         "probe is unavailable");
}

void OptionDefaultsTest() {
  const auto amf_yaml = amflow::SerializeAmfOptionsYaml(amflow::AmfOptions{});
  const auto reduction_yaml = amflow::SerializeReductionOptionsYaml(amflow::ReductionOptions{});

  Expect(amf_yaml.find("WorkingPre: 100") != std::string::npos,
         "AMF defaults should expose WorkingPre");
  Expect(reduction_yaml.find("ReductionMode: \"Kira\"") != std::string::npos,
         "reduction defaults should expose Kira mode");
}

}  // namespace

int main() {
  try {
    SampleProblemValidationTest();
    ProblemSpecRoundTripTest();
    ProblemSpecExampleFileTest();
    K0SmokeProblemSpecFileTest();
    LoadedSpecValidationRejectsMalformedTargetsTest();
    UnknownFieldsAreIgnoredTest();
    DuplicateKeysAreRejectedTest();
    EtaInsertionHappyPathTest();
    EtaInsertionLeavesOriginalSpecUnchangedTest();
    EtaInsertionAppendsEtaOnceOnlyTest();
    EtaInsertionKiraEmissionTest();
    EtaInsertionTrimsSelectedMassLiteralsForCoherentMassSurfaceTest();
    EtaInsertionKiraEmissionTrimsSelectedMassLiteralsTest();
    EtaInsertionRejectsDuplicateIndicesTest();
    EtaInsertionRejectsOutOfRangeIndicesTest();
    EtaInsertionRejectsEmptySelectionTest();
    EtaInsertionRejectsAuxiliaryPropagatorsTest();
    EtaInsertionAllowsNonzeroMassPropagatorsTest();
    AllEtaModeSelectsAllNonAuxiliaryPropagatorsTest();
    AllEtaModeSkipsAuxiliaryPropagatorsTest();
    PrescriptionEtaModeSelectsAllNonAuxiliaryPropagatorsTest();
    PrescriptionEtaModeRejectsAllAuxiliaryPropagatorsTest();
    PropagatorEtaModeSelectsAllNonAuxiliaryPropagatorsTest();
    PropagatorEtaModeRejectsAllAuxiliaryPropagatorsTest();
    PropagatorEtaModeDoesNotMutateInputProblemSpecTest();
    MassEtaModeSelectsEqualNonzeroMassGroupTest();
    MassEtaModeTreatsTrimmedExactMassLabelsAsOneGroupTest();
    MassEtaModePrefersScalarProductRuleIndependentGroupTest();
    MassEtaModeFallsBackToFirstDependentGroupTest();
    MassEtaModeRejectsAllZeroMassSpecTest();
    MassEtaModeRejectsAuxiliaryOnlyNonzeroMassSpecTest();
    MassEtaModeDoesNotOverSelectAcrossDistinctMassGroupsTest();
    MassEtaModeSkipsAuxiliaryMembersInsideChosenMassGroupTest();
    BranchEtaModeHappyPathTest();
    LoopEtaModeHappyPathTest();
    UnsupportedBuiltinEtaModesRejectTest();
    UnsupportedBuiltinEtaModesRejectRepeatedSignGrammarTest();
    UnsupportedBuiltinEtaModesRejectTooManyTopSectorPropagatorsTest();
    UnsupportedBuiltinEtaModesRejectMissingTopLevelSectorTest();
    UnsupportedBuiltinEtaModesRejectMultipleTopLevelSectorsTest();
    UnsupportedBuiltinEtaModesRejectInactiveTopLevelSectorMaskTest();
    UnsupportedBuiltinEtaModesRejectAllCutActiveCandidatesTest();
    UnsupportedBuiltinEtaModesRejectEmptyFirstSymanzikSupportTest();
    UnsupportedBuiltinEtaModesAllAuxiliaryVarProxyMissingTest();
    ResolveEtaModeResolvesBuiltinNameWithoutUserDefinedOverrideTest();
    ResolveEtaModeResolvesUniqueUserDefinedModeTest();
    ResolveEtaModeRejectsUnknownNameWithUserDefinedRegistryTest();
    ResolveEtaModeRejectsDuplicateMatchingUserDefinedNamesTest();
    ResolveEtaModeRejectsDuplicateUserDefinedNamesUnrelatedToQueryTest();
    ResolveEtaModeRejectsBuiltinNameCollisionsTest();
    ResolveEtaModeRejectsPrescriptionBuiltinNameCollisionsTest();
    ResolveEtaModeRejectsBuiltinNameCollisionsUnrelatedToQueryTest();
    ResolveEtaModeRejectsPrescriptionBuiltinNameCollisionsUnrelatedToQueryTest();
    ResolveEtaModeRejectsNullRegistryEntriesTest();
    ResolveEndingSchemeResolvesBuiltinNameWithoutUserDefinedOverrideTest();
    ResolveEndingSchemeResolvesUniqueUserDefinedSchemeTest();
    ResolveEndingSchemeRejectsUnknownNameWithUserDefinedRegistryTest();
    ResolveEndingSchemeRejectsDuplicateMatchingUserDefinedNamesTest();
    ResolveEndingSchemeRejectsDuplicateUserDefinedNamesUnrelatedToQueryTest();
    ResolveEndingSchemeRejectsBuiltinNameCollisionsTest();
    ResolveEndingSchemeRejectsBuiltinNameCollisionsUnrelatedToQueryTest();
    ResolveEndingSchemeRejectsNullRegistryEntriesTest();
    PlanEndingSchemeBuiltinHappyPathTest();
    PlanEndingSchemeUserDefinedHappyPathTest();
    PlanEndingSchemeRejectsUnknownNameWithUserDefinedRegistryTest();
    PlanEndingSchemeRejectsRegistryValidationFailureTest();
    PlanEndingSchemePlanningFailureTest();
    PlanEndingSchemeListHappyPathFallbackTest();
    PlanEndingSchemeListStopsAfterFirstCompletedSchemeTest();
    PlanEndingSchemeListRejectsEmptySchemeListTest();
    PlanEndingSchemeListRejectsUnknownNameImmediatelyTest();
    PlanEndingSchemeListExhaustedKnownModesPreservesLastDiagnosticTest();
    PlanAmfOptionsEndingSchemeHappyPathTest();
    PlanAmfOptionsEndingSchemeUsesDefaultEndingSchemeListTest();
    PlanAmfOptionsEndingSchemeRejectsEmptyEndingSchemeListTest();
    PlanAmfOptionsEndingSchemeRejectsUnknownNameImmediatelyTest();
    PlanAmfOptionsEndingSchemeExhaustedKnownModesPreservesLastDiagnosticTest();
    KiraPreparationTest();
    KiraPreparationEmitsKira31CompatibleYamlFragmentsTest();
    KiraPreparationFromFileSpecTest();
    K0SmokeKiraPreparationFromFileSpecTest();
    KiraPrepareForTargetsHappyPathTest();
    KiraPrepareStillUsesProblemSpecTargetsTest();
    KiraPrepareForTargetsRejectsEmptyTargetListTest();
    KiraPrepareForTargetsRejectsDuplicateTargetsTest();
    KiraPrepareForTargetsRejectsFamilyMismatchTest();
    KiraPrepareForTargetsRejectsArityMismatchTest();
    KiraExecutionCommandTest();
    KiraExecutionSuccessTest();
    KiraExecutionFailureTest();
    KiraExecutionExit127RemainsCompletedTest();
    KiraExecutionUsesUniqueAttemptLogsTest();
    KiraExecutionRejectsInvalidConfigurationTest();
    KiraExecutionRejectsMissingExecutableWithLogsTest();
    KiraExecutionRejectsMissingPreparedFilesTest();
    KiraExecutionExecFailureIsFailedToStartTest();
    KiraParsedResultsHappyPathTest();
    KiraParsedResultsResolveGeneratedConfigReducerRootTest();
    KiraParsedResultsPreferCompleteGeneratedConfigOverCompleteDirectTest();
    KiraParsedResultsCompleteDirectBeatsGeneratedConfigMastersOnlyTest();
    KiraParsedResultsCompleteDirectBeatsGeneratedConfigRuleOnlyTest();
    KiraParsedResultsMissingRuleFallsBackToIdentityTest();
    KiraParsedResultsRejectMalformedMastersTest();
    KiraParsedResultsRejectMalformedRulesTest();
    KiraParsedResultsRejectNonlinearRulesTest();
    KiraParsedResultsRejectInconsistentMastersTest();
    KiraParsedResultsCanonicalizeDuplicateTermsTest();
    ReductionAssemblyHappyPathTest();
    ReductionAssemblyRejectsEmptyMasterBasisTest();
    ReductionAssemblyRejectsDerivativeTargetArityMismatchTest();
    ReductionAssemblyRejectsMissingReductionRuleTest();
    ReductionAssemblyRejectsDuplicateDerivativeTargetsTest();
    ReductionAssemblyRejectsPermutedRowBindingsTest();
    ReductionAssemblyIgnoresIdentityFallbackRulesForDerivativeLookupTest();
    ReductionAssemblyRejectsMasterBasisMismatchTest();
    ValidateDESystemRejectsMalformedCoefficientMatrixShapeTest();
    ValidateDESystemRejectsCoefficientMatrixRowMismatchTest();
    ValidateBoundaryRequestHappyPathTest();
    ValidateBoundaryRequestRejectsEmptyVariableTest();
    ValidateBoundaryRequestRejectsEmptyLocationTest();
    ValidateBoundaryRequestRejectsEmptyStrategyTest();
    ValidateBoundaryRequestRejectsUnknownVariableTest();
    ValidateBoundaryRequestPropagatesInvalidDESystemTest();
    DescribeDESystemOmitsBoundariesTest();
    SampleBoundaryAttachmentSurfaceHappyPathTest();
    AttachManualBoundaryConditionsHappyPathTest();
    AttachManualBoundaryConditionsPreservesExplicitOrderTest();
    AttachManualBoundaryConditionsRejectsEmptyExplicitBoundaryListTest();
    AttachManualBoundaryConditionsRejectsUnknownRequestVariableTest();
    AttachManualBoundaryConditionsRejectsDuplicateExplicitBoundaryConditionTest();
    AttachManualBoundaryConditionsRejectsPreexistingBoundaryConditionsTest();
    AttachManualBoundaryConditionsRejectsBoundaryValueCountMismatchTest();
    AttachManualBoundaryConditionsRejectsMissingStartLocationCoverageTest();
    AttachBoundaryConditionsFromProviderHappyPathTest();
    AttachBoundaryConditionsFromProviderPreservesRequestOrderTest();
    AttachBoundaryConditionsFromProviderRejectsInvalidRequestBeforeProviderCallTest();
    AttachBoundaryConditionsFromProviderRejectsStrategyMismatchTest();
    AttachBoundaryConditionsFromProviderPropagatesProviderBoundaryUnsolvedTest();
    AttachBoundaryConditionsFromProviderRejectsWrongVariableOutputTest();
    AttachBoundaryConditionsFromProviderRejectsWrongLocationOutputTest();
    AttachBoundaryConditionsFromProviderRejectsWrongStrategyOutputTest();
    AttachBoundaryConditionsFromProviderRejectsEmptyValuesOutputTest();
    AttachBoundaryConditionsFromProviderRejectsWrongValueCountOutputTest();
    AttachBoundaryConditionsFromProviderRejectsDuplicateLociOutputTest();
    AttachBoundaryConditionsFromProviderRejectsPreexistingBoundaryConditionsTest();
    AttachBoundaryConditionsFromProviderIsDeterministicAndNonMutatingTest();
    GeneratePlannedEtaInfinityBoundaryRequestBuiltinHappyPathTest();
    GeneratePlannedEtaInfinityBoundaryRequestUserDefinedSingletonHappyPathTest();
    GeneratePlannedEtaInfinityBoundaryRequestBuiltinTrivialRejectsExtraNodeTest();
    GeneratePlannedEtaInfinityBoundaryRequestRejectsUserDefinedExtraNodeTest();
    GeneratePlannedEtaInfinityBoundaryRequestRejectsWrongFamilyInfinityNodeTest();
    GeneratePlannedEtaInfinityBoundaryRequestRejectsDuplicateSupportedNodeTest();
    GeneratePlannedEtaInfinityBoundaryRequestPreservesUnknownEndingDiagnosticTest();
    GeneratePlannedEtaInfinityBoundaryRequestPreservesPlanningFailureDiagnosticTest();
    GeneratePlannedEtaInfinityBoundaryRequestPropagatesBatch45SubsetDiagnosticTest();
    GeneratePlannedEtaInfinityBoundaryRequestPreservesEmptyEtaSymbolDiagnosticTest();
    GenerateBuiltinEtaInfinityBoundaryRequestSampleSpecHappyPathTest();
    GenerateBuiltinEtaInfinityBoundaryRequestSupportsEtaSymbolOverrideTest();
    GenerateBuiltinEtaInfinityBoundaryRequestIgnoresNonBoundaryProblemSpecFieldsTest();
    GenerateBuiltinEtaInfinityBoundaryRequestDoesNotMutateProblemSpecTest();
    GenerateBuiltinEtaInfinityBoundaryRequestRejectsMalformedProblemSpecTest();
    GenerateBuiltinEtaInfinityBoundaryRequestValidatesProblemSpecBeforeEtaSymbolTest();
    GenerateBuiltinEtaInfinityBoundaryRequestRejectsEmptyEtaSymbolTest();
    GenerateBuiltinEtaInfinityBoundaryRequestRejectsNonStandardPropagatorsTest();
    GenerateBuiltinEtaInfinityBoundaryRequestRejectsNonZeroMassTest();
    GenerateBuiltinEtaInfinityBoundaryRequestAttachesThroughProviderSeamTest();
    Batch47BuiltinTraditionEtaInfinityBoundaryEquivalenceTest();
    Batch47UserDefinedEtaInfinitySingletonEquivalenceTest();
    Batch47UnsupportedTerminalNodePreservesDiagnosticTest();
    Batch47PlanningFailurePreservesDiagnosticTest();
    Batch47UnsupportedBatch45SubsetPreservesDiagnosticTest();
    Batch47ProviderStrategyMismatchPreservesDiagnosticTest();
    Batch47ProviderBoundaryUnsolvedPreservesDiagnosticTest();
    Batch47ProviderWrongVariableOutputPreservesDiagnosticTest();
    Batch47ProviderWrongLocationOutputPreservesDiagnosticTest();
    Batch47ProviderWrongStrategyOutputPreservesDiagnosticTest();
    Batch47ProviderEmptyValuesOutputPreservesDiagnosticTest();
    Batch47ProviderWrongValueCountOutputPreservesDiagnosticTest();
    Batch47ProviderDuplicateLociOutputPreservesDiagnosticTest();
    Batch47ConflictingReattachmentPreservesDiagnosticTest();
    BootstrapSeriesSolverReturnsBoundaryUnsolvedForIncompleteManualAttachmentTest();
    BootstrapSeriesSolverReturnsBoundaryUnsolvedWithoutExplicitStartBoundaryTest();
    BootstrapSeriesSolverExactScalarOneHopHappyPathTest();
    BootstrapSeriesSolverExactUpperTriangularOneHopHappyPathTest();
    BootstrapSeriesSolverExactMixedScalarOneHopHappyPathTest();
    BootstrapSeriesSolverExactMixedUpperTriangularDiagonalHappyPathTest();
    BootstrapSeriesSolverExactMixedUpperTriangularZeroForcingResonanceHappyPathTest();
    BootstrapSeriesSolverRejectsInexactOneHopPathTest();
    BootstrapSeriesSolverRejectsFractionalExponentMixedPathTest();
    BootstrapSeriesSolverRejectsForcedLogarithmicMixedPathTest();
    BootstrapSeriesSolverRejectsSingularStartMixedPathTest();
    BootstrapSeriesSolverRejectsInexactMixedPathTest();
    BootstrapSeriesSolverRejectsLowerTriangularSystemTest();
    BootstrapSeriesSolverExactPathIgnoresPrecisionPolicyAndRequestedDigitsTest();
    BootstrapSeriesSolverRejectsMalformedBoundaryValueExpressionTest();
    SolveDifferentialEquationExactScalarHappyPathMatchesBootstrapSolverTest();
    SolveDifferentialEquationExactUpperTriangularHappyPathMatchesBootstrapSolverTest();
    SolveDifferentialEquationExactMixedHappyPathMatchesBootstrapSolverTest();
    SolveDifferentialEquationBoundaryUnsolvedPassthroughTest();
    SolveDifferentialEquationUnsupportedSolverPathPassthroughTest();
    SolveDifferentialEquationMixedUnsupportedSolverPathPassthroughTest();
    SolveDifferentialEquationRejectsMalformedBoundaryValueExpressionLikeBootstrapSolverTest();
    SolveDifferentialEquationExactPathIgnoresPrecisionPolicyAndRequestedDigitsTest();
    SolveDifferentialEquationDoesNotMutateRequestTest();
    EvaluateCoefficientMatrixSampleSMatrixTest();
    EvaluateCoefficientMatrixSampleEtaMatrixTest();
    EvaluateCoefficientMatrixGeneratedEtaFixtureTest();
    EvaluateCoefficientMatrixAutomaticInvariantFixtureTest();
    EvaluateCoefficientExpressionConstantOnlyTest();
    EvaluateCoefficientMatrixIgnoresUnusedSubstitutionsTest();
    EvaluateCoefficientMatrixRejectsMissingSymbolTest();
    EvaluateCoefficientMatrixRejectsDivisionByZeroTest();
    EvaluateCoefficientMatrixRejectsMalformedExpressionTest();
    EvaluateCoefficientMatrixRejectsUnknownVariableTest();
    EvaluateCoefficientMatrixIsDeterministicAndNonMutatingTest();
    DetectFiniteSingularPointsSampleEtaMatrixTest();
    ClassifyFinitePointSampleEtaMatrixTest();
    DetectFiniteSingularPointsSampleSMatrixTest();
    DetectFiniteSingularPointsGeneratedEtaFixtureIsEmptyTest();
    DetectFiniteSingularPointsAutomaticInvariantFixtureIsEmptyTest();
    DetectFiniteSingularPointsCancelsMatchedFactorsTest();
    DetectFiniteSingularPointsPreservesDistinctSimplePoleSumsTest();
    DetectFiniteSingularPointsDropsZeroMaskedDeadBranchesTest();
    DetectFiniteSingularPointsShortCircuitsZeroNumeratorDivisionTest();
    DetectFiniteSingularPointsCancelsGroupedSameDenominatorTermsTest();
    DetectFiniteSingularPointsPreservesZeroDivisorFailuresTest();
    DetectFiniteSingularPointsAllowsLiteralDirectSimpleDifferenceDivisorsTest();
    DetectFiniteSingularPointsRejectsNegatedConstantDifferenceDivisorsTest();
    DetectFiniteSingularPointsRejectsGroupedConstantDifferenceDivisorsTest();
    DetectFiniteSingularPointsRejectsNormalizedMultiTermDivisorsTest();
    DetectFiniteSingularPointsRejectsRegularMultiTermDivisorsTest();
    DetectFiniteSingularPointsRejectsDirectLinearMultiTermDivisorsTest();
    DetectFiniteSingularPointsPreservesDivisionByZeroForNormalizedMultiTermDivisorsTest();
    DetectFiniteSingularPointsPreservesDivisionByZeroForZeroNumeratorNormalizedDivisorsTest();
    DetectFiniteSingularPointsCancelsUnsupportedHigherOrderDeadBranchesTest();
    DetectFiniteSingularPointsCancelsUnsupportedMultiFactorDeadBranchesTest();
    ClassifyFinitePointValidatesEveryCellBeforeReturningTest();
    DetectFiniteSingularPointsAllowsRegularPolynomialNumeratorProductsTest();
    DetectFiniteSingularPointsAllowsSimplePoleWithPolynomialNumeratorTest();
    DetectFiniteSingularPointsAcceptsDuplicateNonlinearNumeratorsAfterCanonicalCombinationTest();
    ClassifyFinitePointAcceptsDuplicateNonlinearNumeratorsAfterCanonicalCombinationTest();
    DetectFiniteSingularPointsRejectsGroupedNonlinearNumeratorsUnderSharedDenominatorTest();
    ClassifyFinitePointRejectsGroupedNonlinearNumeratorsUnderSharedDenominatorTest();
    DetectFiniteSingularPointsShortCircuitsIdenticalMultiTermQuotientsToRegularTest();
    DetectFiniteSingularPointsPreservesDivisionByZeroForIdenticalZeroQuotientsTest();
    DetectFiniteSingularPointsRejectsIdenticalHigherOrderQuotientsOutsideSupportedShapesTest();
    DetectFiniteSingularPointsRejectsIdenticalMultiFactorQuotientsOutsideSupportedShapesTest();
    DetectFiniteSingularPointsRejectsMissingPassiveBindingTest();
    DetectFiniteSingularPointsRejectsMalformedExpressionTest();
    DetectFiniteSingularPointsRejectsUnsupportedSingularFormTest();
    DetectFiniteSingularPointsRejectsUnsupportedMultiFactorSingularFormTest();
    DetectFiniteSingularPointsRejectsUnknownVariableTest();
    ClassifyFinitePointRejectsMalformedPointExpressionTest();
    DetectFiniteSingularPointsIsDeterministicAndNonMutatingTest();
    GenerateScalarRegularPointSeriesPatchZeroCoefficientTest();
    GenerateScalarRegularPointSeriesPatchConstantCoefficientTest();
    GenerateScalarRegularPointSeriesPatchLinearCoefficientTest();
    GenerateScalarRegularPointSeriesPatchShiftedRegularPointTest();
    GenerateScalarRegularPointSeriesPatchPassiveBindingCenterTest();
    GenerateScalarRegularPointSeriesPatchRejectsNonScalarSystemTest();
    GenerateScalarRegularPointSeriesPatchRejectsSingularCenterTest();
    GenerateScalarRegularPointSeriesPatchRejectsFallbackSingularCenterTest();
    GenerateScalarRegularPointSeriesPatchRejectsNegativeOrderTest();
    GenerateScalarRegularPointSeriesPatchRejectsMalformedCenterExpressionTest();
    GenerateScalarRegularPointSeriesPatchRejectsMissingPassiveBindingTest();
    GenerateScalarRegularPointSeriesPatchRejectsUnknownVariableTest();
    GenerateScalarRegularPointSeriesPatchRejectsUnsupportedMatrixShapeTest();
    GenerateScalarRegularPointSeriesPatchIsDeterministicAndNonMutatingTest();
    GenerateScalarFrobeniusSeriesPatchPurePoleHappyPathTest();
    GenerateScalarFrobeniusSeriesPatchPolePlusConstantHappyPathTest();
    GenerateScalarFrobeniusSeriesPatchPolePlusLinearHappyPathTest();
    GenerateScalarFrobeniusSeriesPatchHalfIntegerExponentHappyPathTest();
    GenerateScalarFrobeniusSeriesPatchPassiveBindingCenterHappyPathTest();
    GenerateScalarFrobeniusSeriesPatchRejectsRegularCenterTest();
    GenerateScalarFrobeniusSeriesPatchRejectsHigherOrderPoleTest();
    GenerateScalarFrobeniusSeriesPatchRejectsNonScalarSystemTest();
    GenerateScalarFrobeniusSeriesPatchRejectsNegativeOrderTest();
    GenerateScalarFrobeniusSeriesPatchRejectsMalformedCenterExpressionTest();
    GenerateScalarFrobeniusSeriesPatchRejectsMissingPassiveBindingTest();
    GenerateScalarFrobeniusSeriesPatchRejectsUnsupportedSingularShapeTest();
    GenerateScalarFrobeniusSeriesPatchRejectsWhitespaceVariantUnsupportedSingularShapeTest();
    GenerateScalarFrobeniusSeriesPatchIsDeterministicAndNonMutatingTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchNonResonantHappyPathTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchCompatibleZeroForcingResonanceHappyPathTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchDiagonalDirectSumHappyPathTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchPassiveBindingCenterHappyPathTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsForcedLogarithmicResonanceTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsOffDiagonalResiduePoleTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsRegularCenterTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsHigherOrderPoleTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsLowerTriangularTailSupportTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsNonSquareMatrixTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsDimensionMismatchedMatrixTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsNegativeOrderTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsMalformedCenterExpressionTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsMissingPassiveBindingTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsUnsupportedSingularShapeTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchRejectsWhitespaceVariantUnsupportedSingularShapeTest();
    GenerateUpperTriangularMatrixFrobeniusSeriesPatchIsDeterministicAndNonMutatingTest();
    GenerateUpperTriangularRegularPointSeriesPatchDiagonalHappyPathTest();
    GenerateUpperTriangularRegularPointSeriesPatchConstantUpperTriangularHappyPathTest();
    GenerateUpperTriangularRegularPointSeriesPatchNilpotentChainHappyPathTest();
    GenerateUpperTriangularRegularPointSeriesPatchPassiveBindingHappyPathTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsSingularCenterTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsNonSquareMatrixTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsDimensionMismatchedMatrixTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsLowerTriangularSupportTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsNegativeOrderTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsMalformedCenterExpressionTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsMissingPassiveBindingTest();
    GenerateUpperTriangularRegularPointSeriesPatchRejectsUnknownVariableTest();
    GenerateUpperTriangularRegularPointSeriesPatchIsDeterministicAndNonMutatingTest();
    ScalarSeriesPatchExactZeroOverlapAndResidualTest();
    ScalarSeriesPatchTruncationMismatchAndResidualTest();
    ScalarSeriesPatchOverlapRejectsZeroMatchValueTest();
    ScalarSeriesPatchOverlapRejectsIdenticalResolvedPointsTest();
    ScalarSeriesPatchResidualRejectsMalformedPointExpressionTest();
    ScalarSeriesPatchResidualRejectsMissingPassiveBindingTest();
    ScalarSeriesPatchResidualRejectsUnknownVariableTest();
    ScalarSeriesPatchResidualRejectsMalformedPatchCenterTest();
    ScalarSeriesPatchResidualRejectsBadStorageSizeTest();
    ScalarSeriesPatchResidualPropagatesSingularPointDivisionByZeroTest();
    UpperTriangularMatrixSeriesPatchExactZeroOverlapAndResidualTest();
    UpperTriangularMatrixSeriesPatchTruncationMismatchAndResidualTest();
    UpperTriangularMatrixSeriesPatchCoupledResidualTest();
    UpperTriangularMatrixSeriesPatchOverlapRejectsSingularMatchMatrixTest();
    UpperTriangularMatrixSeriesPatchOverlapRejectsDimensionMismatchTest();
    UpperTriangularMatrixSeriesPatchResidualRejectsNonSquareStoredCoefficientsTest();
    EtaDerivativeGenerationHappyPathTest();
    EtaDerivativeGenerationNegativeExponentTest();
    EtaDerivativeGenerationRejectsArityMismatchTest();
    EtaDerivativeGenerationRejectsFamilyMismatchTest();
    EtaDerivativeGenerationRejectsInconsistentParsedMasterListFamilyTest();
    InvariantDerivativeGenerationHappyPathTest();
    InvariantDerivativeGenerationNegativeExponentTest();
    InvariantDerivativeGenerationRejectsVariableKindMismatchTest();
    InvariantDerivativeGenerationRejectsEmptyVariableNameTest();
    InvariantDerivativeGenerationRejectsEtaVariableNameTest();
    InvariantDerivativeGenerationRejectsFactorArityMismatchTest();
    InvariantDerivativeGenerationRejectsParsedMasterListFamilyMismatchTest();
    InvariantDerivativeGenerationRejectsMasterArityMismatchTest();
    InvariantDerivativeGenerationRejectsPropagatorDerivativeTableSizeMismatchTest();
    InvariantDerivativeGenerationRejectsMasterFamilyMismatchTest();
    InvariantDerivativeGenerationDropsZeroNetCollectedTermsTest();
    BuildInvariantDerivativeSeedHappyPathTest();
    BuildInvariantDerivativeSeedCompositionTest();
    BuildInvariantDerivativeSeedAllZeroCaseTest();
    BuildInvariantDerivativeSeedMatchesWholePropagatorFactorTest();
    BuildInvariantDerivativeSeedRejectsEmptyInvariantNameTest();
    BuildInvariantDerivativeSeedRejectsEtaInvariantNameTest();
    BuildInvariantDerivativeSeedRejectsUnknownInvariantNameTest();
    BuildInvariantDerivativeSeedRejectsMissingScalarProductRuleTest();
    BuildInvariantDerivativeSeedRejectsUnknownMomentumSymbolTest();
    BuildInvariantDerivativeSeedRejectsUnsupportedScalarRuleGrammarTest();
    BuildInvariantDerivativeSeedRejectsUnsupportedPropagatorKindTest();
    BuildInvariantDerivativeSeedRejectsNonzeroMassPropagatorTest();
    BuildInvariantDerivativeSeedRejectsNonRepresentableDerivativeTermsTest();
    BuildInvariantDerivativeSeedRejectsNormalizedDuplicatePropagatorsTest();
    GeneratedDerivativeAssemblyHappyPathTest();
    GeneratedDerivativeAssemblyRejectsMissingExplicitRuleTest();
    GeneratedDerivativeAssemblyRejectsIdentityFallbackTest();
    GeneratedDerivativeAssemblyRejectsRowCountMismatchTest();
    GeneratedDerivativeAssemblyRejectsDuplicateVariableNamesTest();
    PrepareEtaGeneratedReductionHappyPathTest();
    PrepareEtaGeneratedReductionRejectsEmptyGeneratedTargetsTest();
    PrepareEtaGeneratedReductionFakeExecutionSmokeTest();
    PrepareInvariantGeneratedReductionAutomaticHappyPathTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsEmptyGeneratedTargetsTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsEmptyInvariantNameTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsEtaInvariantNameTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsUnknownInvariantNameTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsUnsupportedScalarRuleGrammarTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsNonzeroMassPropagatorTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsMasterBasisFamilyMismatchTest();
    PrepareInvariantGeneratedReductionAutomaticRejectsMasterBasisArityMismatchTest();
    PrepareInvariantGeneratedReductionAutomaticFakeExecutionSmokeTest();
    PrepareInvariantGeneratedReductionHappyPathTest();
    PrepareInvariantGeneratedReductionRejectsEmptyGeneratedTargetsTest();
    PrepareInvariantGeneratedReductionRejectsSpecSeedFamilyMismatchTest();
    PrepareInvariantGeneratedReductionRejectsSpecSeedArityMismatchTest();
    PrepareInvariantGeneratedReductionFakeExecutionSmokeTest();
    RunEtaGeneratedReductionHappyPathTest();
    RunEtaGeneratedReductionExecutionFailureTest();
    RunEtaGeneratedReductionRejectsIdentityFallbackResultsTest();
    BuildEtaGeneratedDESystemHappyPathTest();
    BuildEtaGeneratedDESystemExecutionFailureTest();
    BuildEtaGeneratedDESystemRejectsIdentityFallbackResultsTest();
    BuildEtaGeneratedDESystemRejectsEmptyGeneratedTargetsTest();
    RunInvariantGeneratedReductionHappyPathTest();
    RunInvariantGeneratedReductionExecutionFailureTest();
    RunInvariantGeneratedReductionRejectsIdentityFallbackResultsTest();
    RunInvariantGeneratedReductionRejectsMissingExplicitRuleForMasterTargetTest();
    RunInvariantGeneratedReductionAutomaticHappyPathTest();
    RunInvariantGeneratedReductionAutomaticExecutionFailureTest();
    RunInvariantGeneratedReductionAutomaticRejectsEmptyInvariantNameTest();
    RunInvariantGeneratedReductionAutomaticRejectsUnsupportedScalarRuleGrammarTest();
    RunInvariantGeneratedReductionAutomaticRejectsMasterBasisFamilyMismatchTest();
    RunInvariantGeneratedReductionAutomaticRejectsEmptyGeneratedTargetsTest();
    RunInvariantGeneratedReductionAutomaticRejectsIdentityFallbackResultsTest();
    RunInvariantGeneratedReductionAutomaticRejectsMissingExplicitRuleForGeneratedTargetTest();
    BuildInvariantGeneratedDESystemAutomaticHappyPathTest();
    BuildInvariantGeneratedDESystemAutomaticExecutionFailureTest();
    BuildInvariantGeneratedDESystemAutomaticRejectsEmptyInvariantNameTest();
    BuildInvariantGeneratedDESystemAutomaticRejectsMissingExplicitRuleForGeneratedTargetTest();
    SolveInvariantGeneratedSeriesAutomaticHappyPathTest();
    SolveInvariantGeneratedSeriesAutomaticBootstrapSolverPassthroughTest();
    SolveInvariantGeneratedSeriesAutomaticExecutionFailureTest();
    SolveInvariantGeneratedSeriesAutomaticRejectsEmptyInvariantNameTest();
    SolveInvariantGeneratedSeriesAutomaticRejectsMissingExplicitRuleForGeneratedTargetTest();
    BuildInvariantGeneratedDESystemListAutomaticHappyPathTest();
    BuildInvariantGeneratedDESystemListAutomaticRejectsEmptyInvariantListTest();
    BuildInvariantGeneratedDESystemListAutomaticRejectsUnknownInvariantNameTest();
    SolveInvariantGeneratedSeriesListAutomaticHappyPathTest();
    SolveInvariantGeneratedSeriesListAutomaticExecutionFailureStopsIterationTest();
    SolveInvariantGeneratedSeriesListAutomaticRejectsEmptyInvariantListTest();
    SolveInvariantGeneratedSeriesListAutomaticRejectsUnknownInvariantNameTest();
    SolveEtaGeneratedSeriesHappyPathTest();
    SolveEtaGeneratedSeriesBootstrapSolverPassthroughTest();
    SolveEtaGeneratedSeriesExecutionFailureTest();
    SolveEtaGeneratedSeriesRejectsIdentityFallbackResultsTest();
    SolveEtaGeneratedSeriesRejectsEmptyGeneratedTargetsTest();
    SolveEtaModePlannedSeriesHappyPathTest();
    SolveEtaModePlannedSeriesBootstrapSolverPassthroughTest();
    SolveEtaModePlannedSeriesPlanningFailureTest();
    SolveEtaModePlannedSeriesExecutionFailureTest();
    SolveBuiltinEtaModeSeriesHappyPathTest();
    SolveBuiltinEtaModeSeriesPrescriptionHappyPathTest();
    SolveBuiltinEtaModeSeriesMassHappyPathTest();
    SolveBuiltinEtaModeSeriesPropagatorHappyPathTest();
    SolveBuiltinEtaModeSeriesPropagatorAllowsSelectedNonzeroMassTest();
    SolveBuiltinEtaModeSeriesBootstrapSolverPassthroughTest();
    SolveBuiltinEtaModeSeriesRejectsUnknownBuiltinNameTest();
    SolveBuiltinEtaModeSeriesUnsupportedBuiltinModesRejectTest();
    SolveBuiltinEtaModeListSeriesBootstrapPreflightFailureTest();
    SolveBuiltinEtaModeSeriesRejectsPropagatorWithoutNonAuxiliaryPropagatorsTest();
    SolveBuiltinEtaModeSeriesRejectsAllWithoutNonAuxiliaryPropagatorsTest();
    SolveBuiltinEtaModeSeriesExecutionFailureTest();
    SolveBuiltinEtaModeListSeriesHappyPathFallbackTest();
    SolveBuiltinEtaModeListSeriesMassShortCircuitTest();
    SolveBuiltinEtaModeListSeriesMassDependentFallbackShortCircuitTest();
    SolveBuiltinEtaModeListSeriesPrescriptionShortCircuitTest();
    SolveBuiltinEtaModeListSeriesBootstrapSolverStopsAfterFirstCompletedModeTest();
    SolveBuiltinEtaModeListSeriesRejectsEmptyModeListTest();
    SolveBuiltinEtaModeListSeriesRejectsUnknownBuiltinNameImmediatelyTest();
    SolveBuiltinEtaModeListSeriesExhaustedKnownModesPreservesLastDiagnosticTest();
    SolveBuiltinEtaModeListSeriesPrescriptionFailureFallsThroughToMassFailureTest();
    SolveBuiltinEtaModeListSeriesRejectsAllWithoutNonAuxiliaryPropagatorsTest();
    SolveBuiltinEtaModeListSeriesExecutionFailureStopsIterationTest();
    SolveAmfOptionsEtaModeSeriesHappyPathTest();
    SolveAmfOptionsEtaModeSeriesMassShortCircuitTest();
    SolveAmfOptionsEtaModeSeriesBootstrapSolverStopsAfterFirstCompletedModeTest();
    SolveAmfOptionsEtaModeSeriesUsesDefaultAmfModeListTest();
    SolveAmfOptionsEtaModeSeriesRejectsEmptyAmfModeListTest();
    SolveAmfOptionsEtaModeSeriesRejectsUnknownBuiltinNameImmediatelyTest();
    SolveAmfOptionsEtaModeSeriesExecutionFailureAfterFallbackTest();
    SolveAmfOptionsEtaModeSeriesBootstrapPreflightFailureTest();
    SolveResolvedEtaModeSeriesBuiltinHappyPathTest();
    SolveResolvedEtaModeSeriesUserDefinedHappyPathTest();
    SolveResolvedEtaModeSeriesRejectsUnknownNameWithUserDefinedRegistryTest();
    SolveResolvedEtaModeSeriesRejectsRegistryValidationFailureTest();
    SolveResolvedEtaModeSeriesPlanningFailureTest();
    SolveResolvedEtaModeListSeriesHappyPathFallbackTest();
    SolveResolvedEtaModeListSeriesBootstrapPreflightFailureTest();
    SolveResolvedEtaModeListSeriesBuiltinMassShortCircuitTest();
    SolveResolvedEtaModeListSeriesStopsAfterFirstCompletedModeTest();
    SolveResolvedEtaModeListSeriesRejectsEmptyModeListTest();
    SolveResolvedEtaModeListSeriesRejectsUnknownNameImmediatelyTest();
    SolveResolvedEtaModeListSeriesExhaustedKnownModesPreservesLastDiagnosticTest();
    SolveResolvedEtaModeListSeriesExecutionFailureStopsIterationTest();
    SolveAmfOptionsEtaModeSeriesWithUserDefinedModesHappyPathTest();
    SolveAmfOptionsEtaModeSeriesWithUserDefinedModesBootstrapSolverStopsAfterFirstCompletedModeTest();
    SolveAmfOptionsEtaModeSeriesWithUserDefinedModesUsesDefaultAmfModeListTest();
    SolveAmfOptionsEtaModeSeriesWithUserDefinedModesRejectsEmptyAmfModeListTest();
    SolveAmfOptionsEtaModeSeriesWithUserDefinedModesRejectsUnknownNameImmediatelyTest();
    SolveAmfOptionsEtaModeSeriesWithUserDefinedModesExecutionFailureAfterFallbackTest();
    BootstrapBuiltinSampleManifestTest();
    K0BootstrapManifestSerializationTest();
    RunKiraFromFileRefreshesManifestFromParsedReducerArtifactsTest();
    RunKiraFromFileNonzeroExitStillWritesTruthfulDefaultParseRootTest();
    RepoLocalSpecCopyDoesNotReceiveFrozenFixtureProvenanceTest();
    ExternalSpecDoesNotClaimCleanRepoStatusWhenGitProbeUnavailableTest();
    OptionDefaultsTest();
    std::cout << "amflow bootstrap tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test failure: " << error.what() << "\n";
    return 1;
  }
}
