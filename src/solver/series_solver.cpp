#include "amflow/solver/series_solver.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <typeinfo>
#include <utility>
#include <vector>

#include "amflow/core/options.hpp"
#include "amflow/de/eta_reduction_execution.hpp"
#include "amflow/de/invariant_reduction_execution.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/boundary_generation.hpp"
#include "amflow/runtime/ending_scheme.hpp"
#include "amflow/runtime/eta_mode.hpp"
#include "amflow/solver/boundary_provider.hpp"
#include "amflow/solver/singular_point_analysis.hpp"

namespace amflow {

namespace {

constexpr char kScalarPatchPrefix[] = "scalar regular-point series patch generation";
constexpr char kScalarFrobeniusPatchPrefix[] =
    "scalar Frobenius series patch generation";
constexpr char kScalarResidualPrefix[] = "scalar series patch residual evaluation";
constexpr char kScalarOverlapPrefix[] = "scalar series patch overlap diagnostics";
constexpr char kMatrixPatchPrefix[] =
    "upper-triangular regular-point matrix series patch generation";
constexpr char kMatrixFrobeniusPatchPrefix[] =
    "upper-triangular matrix Frobenius series patch generation";
constexpr char kMatrixResidualPrefix[] =
    "upper-triangular matrix series patch residual evaluation";
constexpr char kMatrixOverlapPrefix[] =
    "upper-triangular matrix series patch overlap diagnostics";
constexpr char kBootstrapSolverPrefix[] = "bootstrap regular-point continuation solver";
constexpr char kUnsupportedSolverPathCode[] = "unsupported_solver_path";
constexpr char kInsufficientPrecisionCode[] = "insufficient_precision";
constexpr char kMasterSetInstabilityCode[] = "master_set_instability";
constexpr char kContinuationBudgetExhaustedCode[] = "continuation_budget_exhausted";
constexpr char kSkipReductionUnavailablePrefix[] =
    "skip_reduction requested but no matching eta-generated reduction state is available";
constexpr char kWrapperExactDimensionOverrideStateFile[] =
    "amflow-wrapper-exact-dimension-override.txt";
constexpr int kBootstrapContinuationOrder = 4;
constexpr int kSolvedPathCacheSchemaVersion = 1;

ExactRational ZeroRational() {
  return {"0", "1"};
}

ExactRational OneRational() {
  return {"1", "1"};
}

ExactRational IntegerRational(const std::size_t value) {
  return {std::to_string(value), "1"};
}

std::string Parenthesize(const ExactRational& value) {
  return "(" + value.ToString() + ")";
}

ExactRational ExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, NumericEvaluationPoint{});
}

NumericEvaluationPoint BuildBootstrapPassiveBindings(const SolveRequest& request) {
  NumericEvaluationPoint passive_bindings;
  std::optional<ExactRational> exact_dimension;
  if (request.amf_requested_dimension_expression.has_value()) {
    try {
      exact_dimension = EvaluateCoefficientExpression(*request.amf_requested_dimension_expression,
                                                      NumericEvaluationPoint{});
      passive_bindings.emplace("dimension", exact_dimension->ToString());
    } catch (const std::exception&) {
      // Non-exact dimension expressions stay inert on the reviewed exact solver path.
    }
  }

  if (!request.amf_requested_d0.has_value() || !exact_dimension.has_value()) {
    return passive_bindings;
  }

  try {
    const ExactRational exact_d0 =
        EvaluateCoefficientExpression(*request.amf_requested_d0, NumericEvaluationPoint{});
    const ExactRational exact_eps = EvaluateCoefficientExpression(
        "((" + exact_d0.ToString() + ")-(" + exact_dimension->ToString() + "))/2",
        NumericEvaluationPoint{});
    passive_bindings.emplace("eps", exact_eps.ToString());
  } catch (const std::exception&) {
    // Non-exact D0 expressions keep eps inert on the reviewed exact solver path.
  }
  return passive_bindings;
}

ExactRational NegateRational(const ExactRational& value) {
  return ExactArithmetic("-" + Parenthesize(value));
}

ExactRational AddRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "+" + Parenthesize(rhs));
}

ExactRational SubtractRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "-" + Parenthesize(rhs));
}

ExactRational MultiplyRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "*" + Parenthesize(rhs));
}

ExactRational DivideRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic(Parenthesize(lhs) + "/" + Parenthesize(rhs));
}

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(start, end - start);
}

std::string RemoveWhitespace(const std::string& value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (const char ch : value) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      normalized.push_back(ch);
    }
  }
  return normalized;
}

std::filesystem::path AbsoluteOrEmpty(const std::filesystem::path& path) {
  return path.empty() ? std::filesystem::path{} : std::filesystem::absolute(path);
}

std::string CommandExecutionStatusToString(const CommandExecutionStatus status) {
  switch (status) {
    case CommandExecutionStatus::NotRun:
      return "not-run";
    case CommandExecutionStatus::Completed:
      return "completed";
    case CommandExecutionStatus::FailedToStart:
      return "failed-to-start";
    case CommandExecutionStatus::InvalidConfiguration:
      return "invalid-configuration";
    case CommandExecutionStatus::Signaled:
      return "signaled";
  }
  return "unknown";
}

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

[[noreturn]] void ThrowSkipReductionUnavailable(const std::string& detail) {
  throw std::runtime_error(std::string(kSkipReductionUnavailablePrefix) + ": " + detail);
}

bool IsDimensionIdentifierStart(const char character) {
  return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool IsDimensionIdentifierContinuation(const char character) {
  return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

class DimensionExpressionSyntaxValidator {
 public:
  explicit DimensionExpressionSyntaxValidator(std::string expression)
      : expression_(std::move(expression)) {}

  void Validate() {
    SkipWhitespace();
    ParseExpression();
    SkipWhitespace();
    if (position_ != expression_.size()) {
      throw Malformed(std::string("unexpected trailing token \"") + expression_[position_] + "\"");
    }
  }

 private:
  std::invalid_argument Malformed(const std::string& detail) const {
    return std::invalid_argument("eta-generated dimension expression is malformed: " + detail +
                                 " in \"" + expression_ + "\"");
  }

  void SkipWhitespace() {
    while (position_ < expression_.size() &&
           std::isspace(static_cast<unsigned char>(expression_[position_])) != 0) {
      ++position_;
    }
  }

  bool Match(const char token) {
    SkipWhitespace();
    if (position_ >= expression_.size() || expression_[position_] != token) {
      return false;
    }
    ++position_;
    return true;
  }

  void ParseExpression() {
    ParseTerm();
    while (true) {
      SkipWhitespace();
      if (position_ >= expression_.size() ||
          (expression_[position_] != '+' && expression_[position_] != '-')) {
        break;
      }
      ++position_;
      ParseTerm();
    }
  }

  void ParseTerm() {
    ParseUnary();
    while (true) {
      SkipWhitespace();
      if (position_ >= expression_.size() ||
          (expression_[position_] != '*' && expression_[position_] != '/')) {
        break;
      }
      ++position_;
      ParseUnary();
    }
  }

  void ParseUnary() {
    SkipWhitespace();
    while (position_ < expression_.size() &&
           (expression_[position_] == '+' || expression_[position_] == '-')) {
      ++position_;
      SkipWhitespace();
    }
    ParsePrimary();
  }

  void ParsePrimary() {
    SkipWhitespace();
    if (position_ >= expression_.size()) {
      throw Malformed("unexpected end of expression");
    }

    if (Match('(')) {
      ParseExpression();
      if (!Match(')')) {
        throw Malformed("expected ')'");
      }
      return;
    }

    if (std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
      while (position_ < expression_.size() &&
             std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
        ++position_;
      }
      return;
    }

    if (IsDimensionIdentifierStart(expression_[position_])) {
      ++position_;
      while (position_ < expression_.size() &&
             IsDimensionIdentifierContinuation(expression_[position_])) {
        ++position_;
      }
      return;
    }

    throw Malformed(std::string("unexpected token \"") + expression_[position_] + "\"");
  }

  std::string expression_;
  std::size_t position_ = 0;
};

bool IsMissingNumericBindingError(const std::invalid_argument& error) {
  return std::string(error.what()).find("requires a numeric binding for symbol") !=
         std::string::npos;
}

std::optional<std::string> ResolveExactDimensionOverride(
    const std::optional<std::string>& amf_requested_dimension_expression) {
  if (!amf_requested_dimension_expression.has_value()) {
    return std::nullopt;
  }

  try {
    return EvaluateCoefficientExpression(*amf_requested_dimension_expression,
                                         NumericEvaluationPoint{})
        .ToString();
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<std::string> NormalizePublicDimensionExpression(
    const std::optional<std::string>& dimension_expression) {
  if (!dimension_expression.has_value()) {
    return std::nullopt;
  }

  const std::string trimmed_expression = Trim(*dimension_expression);
  if (trimmed_expression.empty()) {
    throw std::invalid_argument("eta-generated dimension expression must not be empty");
  }
  DimensionExpressionSyntaxValidator(trimmed_expression).Validate();
  const std::string normalized_expression = RemoveWhitespace(trimmed_expression);

  try {
    return EvaluateCoefficientExpression(normalized_expression, NumericEvaluationPoint{})
        .ToString();
  } catch (const std::invalid_argument& error) {
    if (!IsMissingNumericBindingError(error)) {
      throw;
    }
    return normalized_expression;
  }
}

std::string SerializeWrapperExactDimensionOverrideState(
    const std::optional<std::string>& exact_dimension_override) {
  std::ostringstream out;
  out << "present=" << (exact_dimension_override.has_value() ? "true" : "false") << "\n";
  if (exact_dimension_override.has_value()) {
    out << "value=" << *exact_dimension_override << "\n";
  }
  return out.str();
}

void ApplyWrapperExactDimensionOverride(
    BackendPreparation& preparation,
    const ArtifactLayout& layout,
    const std::optional<std::string>& exact_dimension_override) {
  preparation.generated_files[kWrapperExactDimensionOverrideStateFile] =
      SerializeWrapperExactDimensionOverrideState(exact_dimension_override);
  preparation.command_arguments.clear();
  if (exact_dimension_override.has_value()) {
    preparation.command_arguments.push_back("-sd=" + *exact_dimension_override);
  }

  std::ostringstream command;
  command << "FERMATPATH=<fermat-executable> kira";
  for (const std::string& argument : preparation.command_arguments) {
    command << " " << argument;
  }
  command << " " << (layout.generated_config_dir / "jobs.yaml").string();
  preparation.commands = {command.str()};
}

std::string ReadPreparedFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::in | std::ios::binary);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open prepared Kira file: " + path.string());
  }

  std::ostringstream content;
  content << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    throw std::runtime_error("failed to read prepared Kira file: " + path.string());
  }
  return content.str();
}

std::vector<std::string> ValidatePreparedFilesAgainstLayout(
    const BackendPreparation& preparation,
    const ArtifactLayout& layout) {
  std::vector<std::string> messages;
  if (!preparation.validation_messages.empty()) {
    messages.emplace_back("current eta-generated preparation is invalid: " +
                          JoinMessages(preparation.validation_messages));
  }

  const auto jobs_it = preparation.generated_files.find("jobs.yaml");
  const bool jobs_require_xints =
      jobs_it != preparation.generated_files.end() &&
      jobs_it->second.find("insert_prefactors: [xints]") != std::string::npos;
  if (jobs_require_xints &&
      preparation.generated_files.find("xints") == preparation.generated_files.end()) {
    messages.emplace_back("prepared Kira file was not generated: xints");
  }

  for (const auto& [relative_path, expected_content] : preparation.generated_files) {
    const std::filesystem::path prepared_file_path = layout.generated_config_dir / relative_path;
    if (!std::filesystem::exists(prepared_file_path)) {
      messages.emplace_back("prepared Kira file is missing: " + prepared_file_path.string());
      continue;
    }
    if (!std::filesystem::is_regular_file(prepared_file_path)) {
      messages.emplace_back("prepared Kira file is not a regular file: " +
                            prepared_file_path.string());
      continue;
    }

    try {
      const std::string actual_content = ReadPreparedFile(prepared_file_path);
      if (actual_content != expected_content) {
        messages.emplace_back("prepared Kira file content does not match current eta-generated "
                              "preparation: " +
                              prepared_file_path.string());
      }
    } catch (const std::exception& error) {
      messages.emplace_back(error.what());
    }
  }

  return messages;
}

std::filesystem::path BuildFamilyResultsShadowRoot(const ArtifactLayout& layout,
                                                   const std::string& family,
                                                   const std::filesystem::path& masters_source_path,
                                                   const std::filesystem::path& rules_source_path) {
  const std::filesystem::path shadow_root =
      layout.cache_dir / "skip-reduction-wrapper-owned-results-shadow";
  const std::filesystem::path shadow_family_results_dir = shadow_root / "results" / family;

  std::filesystem::remove_all(shadow_root);
  std::filesystem::create_directories(shadow_family_results_dir);

  if (!masters_source_path.empty() && std::filesystem::exists(masters_source_path)) {
    std::filesystem::copy_file(masters_source_path,
                               shadow_family_results_dir / "masters",
                               std::filesystem::copy_options::overwrite_existing);
  }
  if (!rules_source_path.empty() && std::filesystem::exists(rules_source_path)) {
    std::filesystem::copy_file(rules_source_path,
                               shadow_family_results_dir / "kira_target.m",
                               std::filesystem::copy_options::overwrite_existing);
  }

  return shadow_root;
}

ParsedReductionResult ParseWrapperOwnedReductionResult(const ArtifactLayout& layout,
                                                       const std::string& family) {
  const std::filesystem::path generated_family_results_dir =
      layout.generated_config_dir / "results" / family;
  const std::filesystem::path direct_family_results_dir = layout.results_dir / family;
  const std::filesystem::path direct_masters_path = direct_family_results_dir / "masters";
  const std::filesystem::path generated_masters_path =
      generated_family_results_dir / "masters";
  const std::filesystem::path direct_rules_path = direct_family_results_dir / "kira_target.m";
  const std::filesystem::path generated_rules_path =
      generated_family_results_dir / "kira_target.m";

  KiraBackend backend;
  if (std::filesystem::exists(direct_masters_path) || std::filesystem::exists(direct_rules_path)) {
    const std::filesystem::path masters_source_path =
        std::filesystem::exists(direct_masters_path) ? direct_masters_path : generated_masters_path;
    const std::filesystem::path rules_source_path =
        std::filesystem::exists(direct_rules_path) ? direct_rules_path : generated_rules_path;
    return backend.ParseReductionResult(
        BuildFamilyResultsShadowRoot(layout, family, masters_source_path, rules_source_path),
        family);
  }
  return backend.ParseReductionResult(layout.root, family);
}

std::string VariableContext(const GeneratedDerivativeVariable& generated_variable,
                            const std::size_t index) {
  if (!generated_variable.variable.name.empty()) {
    return "variable \"" + generated_variable.variable.name + "\"";
  }
  return "variable[" + std::to_string(index) + "]";
}

void ThrowIfParsedMasterSetDrift(const ParsedMasterList& master_basis,
                                 const ParsedReductionResult& reduction_result,
                                 const std::string& context) {
  const ParsedMasterList& reduced_master_basis = reduction_result.master_list;
  if (reduced_master_basis.masters.size() != master_basis.masters.size()) {
    throw MasterSetInstabilityError(context +
                                    " reduction master basis size does not match assembly "
                                    "master basis");
  }

  for (std::size_t index = 0; index < master_basis.masters.size(); ++index) {
    const std::string expected_label = master_basis.masters[index].Label();
    const std::string actual_label = reduced_master_basis.masters[index].Label();
    if (actual_label != expected_label) {
      throw MasterSetInstabilityError(
          context + " reduction master basis does not match assembly master basis at position " +
          std::to_string(index) + ": expected " + expected_label + ", found " + actual_label);
    }
  }
}

struct ValidatedWrapperEtaGeneratedReductionState {
  GeneratedDerivativeVariable generated_variable;
  ParsedReductionResult reduction_result;
};

ValidatedWrapperEtaGeneratedReductionState LoadValidatedWrapperEtaGeneratedReductionState(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::optional<std::string>& exact_dimension_override,
    const std::string& eta_symbol) {
  EtaGeneratedReductionPreparation preparation =
      PrepareEtaGeneratedReduction(spec, master_basis, decision, options, layout, eta_symbol);
  ApplyWrapperExactDimensionOverride(
      preparation.backend_preparation, layout, exact_dimension_override);
  const std::vector<std::string> prepared_file_messages =
      ValidatePreparedFilesAgainstLayout(preparation.backend_preparation, layout);
  if (!prepared_file_messages.empty()) {
    ThrowSkipReductionUnavailable(JoinMessages(prepared_file_messages));
  }

  ParsedReductionResult reduction_result;
  try {
    reduction_result = ParseWrapperOwnedReductionResult(
        layout, preparation.auxiliary_family.transformed_spec.family.name);
    ThrowIfParsedMasterSetDrift(master_basis,
                                reduction_result,
                                VariableContext(preparation.generated_variable, 0));
  } catch (const std::exception& error) {
    ThrowSkipReductionUnavailable(error.what());
  }

  return {preparation.generated_variable, reduction_result};
}

EtaGeneratedReductionExecution RunWrapperEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::optional<std::string>& exact_dimension_override,
    const std::string& eta_symbol) {
  EtaGeneratedReductionExecution execution;
  execution.preparation =
      PrepareEtaGeneratedReduction(spec, master_basis, decision, options, layout, eta_symbol);
  ApplyWrapperExactDimensionOverride(
      execution.preparation.backend_preparation, layout, exact_dimension_override);

  KiraBackend backend;
  execution.execution_result = backend.ExecutePrepared(execution.preparation.backend_preparation,
                                                       layout,
                                                       kira_executable,
                                                       fermat_executable);
  if (!execution.execution_result.Succeeded()) {
    return execution;
  }

  if (execution.execution_result.working_directory.empty()) {
    throw std::runtime_error("successful eta-generated reduction execution did not record a "
                             "working-directory artifact root");
  }

  execution.parsed_reduction_result =
      backend.ParseReductionResult(execution.execution_result.working_directory,
                                   execution.preparation.auxiliary_family.transformed_spec.family
                                       .name);
  ThrowIfParsedMasterSetDrift(master_basis,
                              *execution.parsed_reduction_result,
                              VariableContext(execution.preparation.generated_variable, 0));

  GeneratedDerivativeVariableReductionInput variable_input;
  variable_input.generated_variable = execution.preparation.generated_variable;
  variable_input.reduction_result = *execution.parsed_reduction_result;
  execution.assembled_system = AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
  return execution;
}

DESystem AssembleWrapperEtaGeneratedDESystem(
    const ParsedMasterList& master_basis,
    const ValidatedWrapperEtaGeneratedReductionState& prepared_state) {
  try {
    GeneratedDerivativeVariableReductionInput variable_input;
    variable_input.generated_variable = prepared_state.generated_variable;
    variable_input.reduction_result = prepared_state.reduction_result;
    return AssembleGeneratedDerivativeDESystem(master_basis, {variable_input});
  } catch (const std::exception& error) {
    ThrowSkipReductionUnavailable(error.what());
  }
}

DESystem BuildWrapperEtaGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::optional<std::string>& exact_dimension_override,
    const std::string& eta_symbol,
    const bool skip_reduction) {
  if (!skip_reduction) {
    EtaGeneratedReductionExecution execution =
        RunWrapperEtaGeneratedReduction(spec,
                                        master_basis,
                                        decision,
                                        options,
                                        layout,
                                        kira_executable,
                                        fermat_executable,
                                        exact_dimension_override,
                                        eta_symbol);
    if (!execution.execution_result.Succeeded()) {
      std::ostringstream message;
      message << "eta-generated DE construction requires successful reducer execution; "
              << "status=" << CommandExecutionStatusToString(execution.execution_result.status)
              << "; exit_code=" << execution.execution_result.exit_code
              << "; stderr_log=" << execution.execution_result.stderr_log_path.string();
      if (!execution.execution_result.error_message.empty()) {
        message << "; error=" << execution.execution_result.error_message;
      }
      throw std::runtime_error(message.str());
    }
    if (!execution.assembled_system.has_value()) {
      throw std::runtime_error("eta-generated DE construction completed without an assembled "
                               "DESystem");
    }
    return *execution.assembled_system;
  }

  return AssembleWrapperEtaGeneratedDESystem(
      master_basis,
      LoadValidatedWrapperEtaGeneratedReductionState(
          spec, master_basis, decision, options, layout, exact_dimension_override, eta_symbol));
}

bool HasDeclaredVariable(const DESystem& system, const std::string& variable_name) {
  return std::any_of(system.variables.begin(),
                     system.variables.end(),
                     [&variable_name](const DifferentiationVariable& variable) {
                       return variable.name == variable_name;
                     });
}

const DifferentiationVariable& ResolveSelectedVariable(const DESystem& system,
                                                       const std::string& variable_name,
                                                       const char* patch_prefix) {
  const auto variable_it =
      std::find_if(system.variables.begin(),
                   system.variables.end(),
                   [&variable_name](const DifferentiationVariable& variable) {
                     return variable.name == variable_name;
                   });
  if (variable_it == system.variables.end()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires a declared coefficient matrix for variable \"" +
                                variable_name + "\"");
  }
  return *variable_it;
}

const std::vector<std::vector<std::string>>& ResolveSelectedMatrix(const DESystem& system,
                                                                   const std::string& variable_name,
                                                                   const char* patch_prefix) {
  const auto matrix_it = system.coefficient_matrices.find(variable_name);
  if (!HasDeclaredVariable(system, variable_name) || matrix_it == system.coefficient_matrices.end()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires a declared coefficient matrix for variable \"" +
                                variable_name + "\"");
  }
  return matrix_it->second;
}

ExactRational ParseCenterValue(const std::string& variable_name,
                               const std::string& center_expression,
                               const NumericEvaluationPoint& passive_bindings,
                               const char* patch_prefix) {
  const std::string trimmed = Trim(center_expression);
  if (trimmed.empty()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed center expression: empty input");
  }

  const std::size_t separator = trimmed.find('=');
  if (separator == std::string::npos) {
    return EvaluateCoefficientExpression(trimmed, passive_bindings);
  }

  if (trimmed.find('=', separator + 1) != std::string::npos) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed center expression: \"" +
                                center_expression + "\"");
  }

  const std::string lhs = Trim(trimmed.substr(0, separator));
  const std::string rhs = Trim(trimmed.substr(separator + 1));
  if (lhs != variable_name || rhs.empty()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed center expression: \"" +
                                center_expression + "\"");
  }

  return EvaluateCoefficientExpression(rhs, passive_bindings);
}

ExactRational ParsePointValue(const std::string& variable_name,
                              const std::string& point_expression,
                              const NumericEvaluationPoint& passive_bindings,
                              const char* patch_prefix) {
  const std::string trimmed = Trim(point_expression);
  if (trimmed.empty()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed point expression: empty input");
  }

  const std::size_t separator = trimmed.find('=');
  if (separator == std::string::npos) {
    return EvaluateCoefficientExpression(trimmed, passive_bindings);
  }

  if (trimmed.find('=', separator + 1) != std::string::npos) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed point expression: \"" +
                                point_expression + "\"");
  }

  const std::string lhs = Trim(trimmed.substr(0, separator));
  const std::string rhs = Trim(trimmed.substr(separator + 1));
  if (lhs != variable_name || rhs.empty()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed point expression: \"" +
                                point_expression + "\"");
  }

  return EvaluateCoefficientExpression(rhs, passive_bindings);
}

ExactRational ParseSeriesPatchCenterValue(const std::string& variable_name,
                                          const std::string& center_expression,
                                          const NumericEvaluationPoint& passive_bindings,
                                          const char* patch_prefix) {
  const std::string trimmed = Trim(center_expression);
  if (trimmed.empty()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed patch center: empty input");
  }

  const std::size_t separator = trimmed.find('=');
  if (separator == std::string::npos) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed patch center: \"" +
                                center_expression + "\"");
  }

  if (trimmed.find('=', separator + 1) != std::string::npos) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed patch center: \"" +
                                center_expression + "\"");
  }

  const std::string lhs = Trim(trimmed.substr(0, separator));
  const std::string rhs = Trim(trimmed.substr(separator + 1));
  if (lhs != variable_name || rhs.empty()) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " encountered malformed patch center: \"" +
                                center_expression + "\"");
  }

  return EvaluateCoefficientExpression(rhs, passive_bindings);
}

bool IsUnsupportedSingularFormError(const std::invalid_argument& error) {
  return std::string(error.what()).find("unsupported singular-form analysis") != std::string::npos;
}

bool ContainsParenthesizedDirectDifference(const std::string& expression,
                                           const std::string& variable_name) {
  const std::string normalized = RemoveWhitespace(expression);
  return normalized.find(variable_name + "-(") != std::string::npos;
}

using ExactSeries = std::vector<ExactRational>;
using ExactRationalVector = std::vector<ExactRational>;
using ExactPassiveBindings = std::map<std::string, ExactRational>;

struct NormalizedUpperTriangularMatrixFrobeniusPatchData {
  std::vector<ExactRational> indicial_exponents;
  std::vector<long long> integral_exponents;
  std::vector<ExactRationalMatrix> coefficient_matrices;
};

ExactSeries MakeZeroSeries(const int max_order) {
  return ExactSeries(static_cast<std::size_t>(max_order + 1), ZeroRational());
}

ExactSeries MakeConstantSeries(const ExactRational& value, const int max_order) {
  ExactSeries series = MakeZeroSeries(max_order);
  series[0] = value;
  return series;
}

ExactSeries MakeActiveVariableSeries(const ExactRational& center_value, const int max_order) {
  ExactSeries series = MakeZeroSeries(max_order);
  series[0] = center_value;
  if (max_order >= 1) {
    series[1] = OneRational();
  }
  return series;
}

ExactSeries AddSeries(const ExactSeries& lhs, const ExactSeries& rhs) {
  ExactSeries result(lhs.size(), ZeroRational());
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    result[index] = AddRational(lhs[index], rhs[index]);
  }
  return result;
}

ExactSeries SubtractSeries(const ExactSeries& lhs, const ExactSeries& rhs) {
  ExactSeries result(lhs.size(), ZeroRational());
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    result[index] = SubtractRational(lhs[index], rhs[index]);
  }
  return result;
}

ExactSeries NegateSeries(const ExactSeries& value) {
  ExactSeries result(value.size(), ZeroRational());
  for (std::size_t index = 0; index < value.size(); ++index) {
    result[index] = NegateRational(value[index]);
  }
  return result;
}

ExactSeries MultiplySeries(const ExactSeries& lhs, const ExactSeries& rhs) {
  ExactSeries result(lhs.size(), ZeroRational());
  for (std::size_t lhs_index = 0; lhs_index < lhs.size(); ++lhs_index) {
    if (lhs[lhs_index].IsZero()) {
      continue;
    }
    for (std::size_t rhs_index = 0; rhs_index + lhs_index < rhs.size(); ++rhs_index) {
      if (rhs[rhs_index].IsZero()) {
        continue;
      }
      const std::size_t result_index = lhs_index + rhs_index;
      result[result_index] =
          AddRational(result[result_index], MultiplyRational(lhs[lhs_index], rhs[rhs_index]));
    }
  }
  return result;
}

std::optional<std::size_t> LeadingOrder(const ExactSeries& series) {
  for (std::size_t index = 0; index < series.size(); ++index) {
    if (!series[index].IsZero()) {
      return index;
    }
  }
  return std::nullopt;
}

std::invalid_argument UnsupportedCoefficientShape(const std::string& expression,
                                                  const std::string& detail,
                                                  const char* patch_prefix) {
  return std::invalid_argument(std::string(patch_prefix) +
                               " encountered unsupported coefficient shape: " + detail +
                               " in \"" + expression + "\"");
}

ExactSeries DivideSeries(const ExactSeries& numerator,
                         const ExactSeries& denominator,
                         const std::string& expression,
                         const char* patch_prefix) {
  const int max_order = static_cast<int>(numerator.size()) - 1;
  const std::optional<std::size_t> denominator_leading_order = LeadingOrder(denominator);
  if (!denominator_leading_order.has_value()) {
    throw UnsupportedCoefficientShape(
        expression,
        "division denominator has no nonzero local-series term through the requested order",
        patch_prefix);
  }

  const std::optional<std::size_t> numerator_leading_order = LeadingOrder(numerator);
  if (!numerator_leading_order.has_value()) {
    return MakeZeroSeries(max_order);
  }

  if (*numerator_leading_order < *denominator_leading_order) {
    throw UnsupportedCoefficientShape(expression,
                                      "quotient introduces negative powers at the center",
                                      patch_prefix);
  }

  const std::size_t shift = *numerator_leading_order - *denominator_leading_order;
  ExactSeries quotient = MakeZeroSeries(max_order);
  if (shift >= quotient.size()) {
    return quotient;
  }

  const ExactRational denominator_leading = denominator[*denominator_leading_order];
  for (std::size_t quotient_index = shift; quotient_index < quotient.size(); ++quotient_index) {
    const std::size_t normalized_index = quotient_index - shift;
    ExactRational numerator_term = ZeroRational();
    const std::size_t numerator_index = *numerator_leading_order + normalized_index;
    if (numerator_index < numerator.size()) {
      numerator_term = numerator[numerator_index];
    }

    ExactRational remainder = numerator_term;
    for (std::size_t divisor_offset = 1;
         divisor_offset <= normalized_index &&
         *denominator_leading_order + divisor_offset < denominator.size();
         ++divisor_offset) {
      const ExactRational contribution =
          MultiplyRational(denominator[*denominator_leading_order + divisor_offset],
                           quotient[quotient_index - divisor_offset]);
      remainder = SubtractRational(remainder, contribution);
    }

    quotient[quotient_index] = DivideRational(remainder, denominator_leading);
  }

  return quotient;
}

enum class TokenKind {
  Identifier,
  Number,
  Plus,
  Minus,
  Star,
  Slash,
  LeftParen,
  RightParen,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
};

std::vector<Token> Tokenize(const std::string& expression, const char* patch_prefix) {
  std::vector<Token> tokens;
  std::size_t index = 0;
  while (index < expression.size()) {
    const char ch = expression[index];
    if (std::isspace(static_cast<unsigned char>(ch))) {
      ++index;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(ch))) {
      const std::size_t start = index;
      while (index < expression.size() &&
             std::isdigit(static_cast<unsigned char>(expression[index]))) {
        ++index;
      }
      tokens.push_back({TokenKind::Number, expression.substr(start, index - start)});
      continue;
    }

    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
      const std::size_t start = index;
      while (index < expression.size() &&
             (std::isalnum(static_cast<unsigned char>(expression[index])) ||
              expression[index] == '_')) {
        ++index;
      }
      tokens.push_back({TokenKind::Identifier, expression.substr(start, index - start)});
      continue;
    }

    switch (ch) {
      case '+':
        tokens.push_back({TokenKind::Plus, "+"});
        break;
      case '-':
        tokens.push_back({TokenKind::Minus, "-"});
        break;
      case '*':
        tokens.push_back({TokenKind::Star, "*"});
        break;
      case '/':
        tokens.push_back({TokenKind::Slash, "/"});
        break;
      case '(':
        tokens.push_back({TokenKind::LeftParen, "("});
        break;
      case ')':
        tokens.push_back({TokenKind::RightParen, ")"});
        break;
      default:
        throw std::invalid_argument(std::string(patch_prefix) +
                                    " encountered malformed coefficient expression: "
                                    "unexpected character \"" +
                                    std::string(1, ch) + "\" in \"" + expression + "\"");
    }
    ++index;
  }
  tokens.push_back({TokenKind::End, ""});
  return tokens;
}

class SeriesExpressionParser {
 public:
  SeriesExpressionParser(std::string expression,
                         std::string active_variable,
                         ExactRational center_value,
                         ExactPassiveBindings passive_bindings,
                         const int max_order,
                         const char* patch_prefix)
      : expression_(std::move(expression)),
        active_variable_(std::move(active_variable)),
        center_value_(std::move(center_value)),
        passive_bindings_(std::move(passive_bindings)),
        tokens_(Tokenize(expression_, patch_prefix)),
        max_order_(max_order),
        patch_prefix_(patch_prefix) {}

  ExactSeries Parse() {
    const ExactSeries value = ParseExpression();
    if (Current().kind != TokenKind::End) {
      throw Malformed("unexpected trailing token \"" + Current().text + "\"");
    }
    return value;
  }

 private:
  const Token& Current() const { return tokens_[position_]; }

  const Token& Advance() {
    const Token& current = Current();
    if (position_ < tokens_.size()) {
      ++position_;
    }
    return current;
  }

  bool Match(const TokenKind kind) {
    if (Current().kind != kind) {
      return false;
    }
    Advance();
    return true;
  }

  std::invalid_argument Malformed(const std::string& detail) const {
    return std::invalid_argument(std::string(patch_prefix_) +
                                 " encountered malformed coefficient expression: " + detail +
                                 " in \"" + expression_ + "\"");
  }

  ExactSeries ParseExpression() {
    ExactSeries value = ParseTerm();
    while (true) {
      if (Match(TokenKind::Plus)) {
        value = AddSeries(value, ParseTerm());
        continue;
      }
      if (Match(TokenKind::Minus)) {
        value = SubtractSeries(value, ParseTerm());
        continue;
      }
      break;
    }
    return value;
  }

  ExactSeries ParseTerm() {
    ExactSeries value = ParseUnary();
    while (true) {
      if (Match(TokenKind::Star)) {
        value = MultiplySeries(value, ParseUnary());
        continue;
      }
      if (Match(TokenKind::Slash)) {
        value = DivideSeries(value, ParseUnary(), expression_, patch_prefix_);
        continue;
      }
      break;
    }
    return value;
  }

  ExactSeries ParseUnary() {
    if (Match(TokenKind::Plus)) {
      return ParseUnary();
    }
    if (Match(TokenKind::Minus)) {
      return NegateSeries(ParseUnary());
    }
    return ParsePrimary();
  }

  ExactSeries ParsePrimary() {
    if (Current().kind == TokenKind::Number) {
      const Token token = Advance();
      return MakeConstantSeries({token.text, "1"}, max_order_);
    }

    if (Current().kind == TokenKind::Identifier) {
      const Token token = Advance();
      if (token.text == active_variable_) {
        return MakeActiveVariableSeries(center_value_, max_order_);
      }

      const auto binding_it = passive_bindings_.find(token.text);
      if (binding_it == passive_bindings_.end()) {
        throw std::invalid_argument(std::string(patch_prefix_) +
                                    " requires a numeric binding for symbol \"" +
                                    token.text + "\"");
      }
      return MakeConstantSeries(binding_it->second, max_order_);
    }

    if (Match(TokenKind::LeftParen)) {
      const ExactSeries value = ParseExpression();
      if (!Match(TokenKind::RightParen)) {
        throw Malformed("expected ')'");
      }
      return value;
    }

    if (Current().kind == TokenKind::End) {
      throw Malformed("unexpected end of expression");
    }
    throw Malformed("unexpected token \"" + Current().text + "\"");
  }

  std::string expression_;
  std::string active_variable_;
  ExactRational center_value_;
  ExactPassiveBindings passive_bindings_;
  std::vector<Token> tokens_;
  std::size_t position_ = 0;
  int max_order_ = 0;
  const char* patch_prefix_ = kScalarPatchPrefix;
};

using LaurentSeries = std::map<int, ExactRational>;

std::optional<int> LeadingLaurentOrder(const LaurentSeries& series) {
  if (series.empty()) {
    return std::nullopt;
  }
  return series.begin()->first;
}

ExactRational LaurentCoefficient(const LaurentSeries& series, const int order) {
  const auto term_it = series.find(order);
  if (term_it == series.end()) {
    return ZeroRational();
  }
  return term_it->second;
}

void SetLaurentCoefficient(LaurentSeries& series,
                           const int order,
                           const ExactRational& value) {
  if (value.IsZero()) {
    series.erase(order);
    return;
  }
  series[order] = value;
}

void AddLaurentContribution(LaurentSeries& series,
                            const int order,
                            const ExactRational& value) {
  if (value.IsZero()) {
    return;
  }

  const auto term_it = series.find(order);
  if (term_it == series.end()) {
    series.emplace(order, value);
    return;
  }

  const ExactRational sum = AddRational(term_it->second, value);
  if (sum.IsZero()) {
    series.erase(term_it);
    return;
  }
  term_it->second = sum;
}

LaurentSeries MakeConstantLaurentSeries(const ExactRational& value) {
  LaurentSeries series;
  SetLaurentCoefficient(series, 0, value);
  return series;
}

LaurentSeries MakeActiveVariableLaurentSeries(const ExactRational& center_value) {
  LaurentSeries series;
  SetLaurentCoefficient(series, 0, center_value);
  SetLaurentCoefficient(series, 1, OneRational());
  return series;
}

LaurentSeries AddLaurentSeries(const LaurentSeries& lhs, const LaurentSeries& rhs) {
  LaurentSeries result = lhs;
  for (const auto& [order, coefficient] : rhs) {
    AddLaurentContribution(result, order, coefficient);
  }
  return result;
}

LaurentSeries SubtractLaurentSeries(const LaurentSeries& lhs, const LaurentSeries& rhs) {
  LaurentSeries result = lhs;
  for (const auto& [order, coefficient] : rhs) {
    AddLaurentContribution(result, order, NegateRational(coefficient));
  }
  return result;
}

LaurentSeries NegateLaurentSeries(const LaurentSeries& value) {
  LaurentSeries result;
  for (const auto& [order, coefficient] : value) {
    SetLaurentCoefficient(result, order, NegateRational(coefficient));
  }
  return result;
}

LaurentSeries MultiplyLaurentSeries(const LaurentSeries& lhs, const LaurentSeries& rhs) {
  LaurentSeries result;
  for (const auto& [lhs_order, lhs_coefficient] : lhs) {
    if (lhs_coefficient.IsZero()) {
      continue;
    }
    for (const auto& [rhs_order, rhs_coefficient] : rhs) {
      if (rhs_coefficient.IsZero()) {
        continue;
      }
      AddLaurentContribution(result,
                             lhs_order + rhs_order,
                             MultiplyRational(lhs_coefficient, rhs_coefficient));
    }
  }
  return result;
}

LaurentSeries DivideLaurentSeries(const LaurentSeries& numerator,
                                  const LaurentSeries& denominator,
                                  const std::string& expression,
                                  const int max_order,
                                  const char* patch_prefix) {
  const std::optional<int> denominator_leading_order = LeadingLaurentOrder(denominator);
  if (!denominator_leading_order.has_value()) {
    throw UnsupportedCoefficientShape(
        expression,
        "division denominator has no nonzero local-series term through the requested order",
        patch_prefix);
  }

  const std::optional<int> numerator_leading_order = LeadingLaurentOrder(numerator);
  if (!numerator_leading_order.has_value()) {
    return LaurentSeries{};
  }

  const int shift = *numerator_leading_order - *denominator_leading_order;
  if (shift > max_order) {
    return LaurentSeries{};
  }

  LaurentSeries quotient;
  const ExactRational denominator_leading =
      LaurentCoefficient(denominator, *denominator_leading_order);
  for (int normalized_index = 0; normalized_index <= max_order - shift; ++normalized_index) {
    ExactRational remainder =
        LaurentCoefficient(numerator, *numerator_leading_order + normalized_index);
    for (int divisor_offset = 1; divisor_offset <= normalized_index; ++divisor_offset) {
      const ExactRational divisor_term =
          LaurentCoefficient(denominator, *denominator_leading_order + divisor_offset);
      const ExactRational quotient_term =
          LaurentCoefficient(quotient, shift + normalized_index - divisor_offset);
      if (divisor_term.IsZero() || quotient_term.IsZero()) {
        continue;
      }
      remainder =
          SubtractRational(remainder, MultiplyRational(divisor_term, quotient_term));
    }

    SetLaurentCoefficient(quotient,
                          shift + normalized_index,
                          DivideRational(remainder, denominator_leading));
  }

  return quotient;
}

class LaurentSeriesExpressionParser {
 public:
  LaurentSeriesExpressionParser(std::string expression,
                                std::string active_variable,
                                ExactRational center_value,
                                ExactPassiveBindings passive_bindings,
                                const int max_order,
                                const char* patch_prefix)
      : expression_(std::move(expression)),
        active_variable_(std::move(active_variable)),
        center_value_(std::move(center_value)),
        passive_bindings_(std::move(passive_bindings)),
        tokens_(Tokenize(expression_, patch_prefix)),
        max_order_(max_order),
        patch_prefix_(patch_prefix) {}

  LaurentSeries Parse() {
    const LaurentSeries value = ParseExpression();
    if (Current().kind != TokenKind::End) {
      throw Malformed("unexpected trailing token \"" + Current().text + "\"");
    }
    return value;
  }

 private:
  const Token& Current() const { return tokens_[position_]; }

  const Token& Advance() {
    const Token& current = Current();
    if (position_ < tokens_.size()) {
      ++position_;
    }
    return current;
  }

  bool Match(const TokenKind kind) {
    if (Current().kind != kind) {
      return false;
    }
    Advance();
    return true;
  }

  std::invalid_argument Malformed(const std::string& detail) const {
    return std::invalid_argument(std::string(patch_prefix_) +
                                 " encountered malformed coefficient expression: " + detail +
                                 " in \"" + expression_ + "\"");
  }

  LaurentSeries ParseExpression() {
    LaurentSeries value = ParseTerm();
    while (true) {
      if (Match(TokenKind::Plus)) {
        value = AddLaurentSeries(value, ParseTerm());
        continue;
      }
      if (Match(TokenKind::Minus)) {
        value = SubtractLaurentSeries(value, ParseTerm());
        continue;
      }
      break;
    }
    return value;
  }

  LaurentSeries ParseTerm() {
    LaurentSeries value = ParseUnary();
    while (true) {
      if (Match(TokenKind::Star)) {
        value = MultiplyLaurentSeries(value, ParseUnary());
        continue;
      }
      if (Match(TokenKind::Slash)) {
        value =
            DivideLaurentSeries(value, ParseUnary(), expression_, max_order_, patch_prefix_);
        continue;
      }
      break;
    }
    return value;
  }

  LaurentSeries ParseUnary() {
    if (Match(TokenKind::Plus)) {
      return ParseUnary();
    }
    if (Match(TokenKind::Minus)) {
      return NegateLaurentSeries(ParseUnary());
    }
    return ParsePrimary();
  }

  LaurentSeries ParsePrimary() {
    if (Current().kind == TokenKind::Number) {
      const Token token = Advance();
      return MakeConstantLaurentSeries({token.text, "1"});
    }

    if (Current().kind == TokenKind::Identifier) {
      const Token token = Advance();
      if (token.text == active_variable_) {
        return MakeActiveVariableLaurentSeries(center_value_);
      }

      const auto binding_it = passive_bindings_.find(token.text);
      if (binding_it == passive_bindings_.end()) {
        throw std::invalid_argument(std::string(patch_prefix_) +
                                    " requires a numeric binding for symbol \"" +
                                    token.text + "\"");
      }
      return MakeConstantLaurentSeries(binding_it->second);
    }

    if (Match(TokenKind::LeftParen)) {
      const LaurentSeries value = ParseExpression();
      if (!Match(TokenKind::RightParen)) {
        throw Malformed("expected ')'");
      }
      return value;
    }

    if (Current().kind == TokenKind::End) {
      throw Malformed("unexpected end of expression");
    }
    throw Malformed("unexpected token \"" + Current().text + "\"");
  }

  std::string expression_;
  std::string active_variable_;
  ExactRational center_value_;
  ExactPassiveBindings passive_bindings_;
  std::vector<Token> tokens_;
  std::size_t position_ = 0;
  int max_order_ = 0;
  const char* patch_prefix_ = kScalarFrobeniusPatchPrefix;
};

ExactPassiveBindings ResolvePassiveBindingsExactly(const NumericEvaluationPoint& passive_bindings) {
  ExactPassiveBindings exact_bindings;
  for (const auto& [symbol, value] : passive_bindings) {
    exact_bindings.emplace(symbol, EvaluateCoefficientExpression(value, NumericEvaluationPoint{}));
  }
  return exact_bindings;
}

ExactRationalMatrix MakeZeroMatrix(const std::size_t dimension) {
  return ExactRationalMatrix(dimension, std::vector<ExactRational>(dimension, ZeroRational()));
}

ExactRationalMatrix MakeIdentityMatrix(const std::size_t dimension) {
  ExactRationalMatrix matrix = MakeZeroMatrix(dimension);
  for (std::size_t index = 0; index < dimension; ++index) {
    matrix[index][index] = OneRational();
  }
  return matrix;
}

ExactRationalMatrix AddMatrices(const ExactRationalMatrix& lhs, const ExactRationalMatrix& rhs) {
  ExactRationalMatrix result = MakeZeroMatrix(lhs.size());
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    for (std::size_t column = 0; column < lhs[row].size(); ++column) {
      result[row][column] = AddRational(lhs[row][column], rhs[row][column]);
    }
  }
  return result;
}

ExactRationalMatrix SubtractMatrices(const ExactRationalMatrix& lhs,
                                     const ExactRationalMatrix& rhs) {
  ExactRationalMatrix result = MakeZeroMatrix(lhs.size());
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    for (std::size_t column = 0; column < lhs[row].size(); ++column) {
      result[row][column] = SubtractRational(lhs[row][column], rhs[row][column]);
    }
  }
  return result;
}

ExactRationalMatrix ScaleMatrix(const ExactRationalMatrix& matrix, const ExactRational& factor) {
  ExactRationalMatrix result = MakeZeroMatrix(matrix.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      result[row][column] = MultiplyRational(matrix[row][column], factor);
    }
  }
  return result;
}

ExactRationalMatrix DivideMatrix(const ExactRationalMatrix& matrix, const ExactRational& divisor) {
  ExactRationalMatrix result = MakeZeroMatrix(matrix.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      result[row][column] = DivideRational(matrix[row][column], divisor);
    }
  }
  return result;
}

ExactRationalMatrix MultiplyMatrices(const ExactRationalMatrix& lhs, const ExactRationalMatrix& rhs) {
  ExactRationalMatrix result = MakeZeroMatrix(lhs.size());
  for (std::size_t row = 0; row < lhs.size(); ++row) {
    for (std::size_t inner = 0; inner < lhs[row].size(); ++inner) {
      if (lhs[row][inner].IsZero()) {
        continue;
      }
      for (std::size_t column = 0; column < rhs[inner].size(); ++column) {
        if (rhs[inner][column].IsZero()) {
          continue;
        }
        result[row][column] =
            AddRational(result[row][column],
                        MultiplyRational(lhs[row][inner], rhs[inner][column]));
      }
    }
  }
  return result;
}

bool MatricesEqual(const ExactRationalMatrix& lhs, const ExactRationalMatrix& rhs) {
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

bool IsZeroMatrix(const ExactRationalMatrix& matrix) {
  for (const auto& row : matrix) {
    for (const ExactRational& value : row) {
      if (!value.IsZero()) {
        return false;
      }
    }
  }
  return true;
}

ExactRationalVector MultiplyMatrixVector(const ExactRationalMatrix& matrix,
                                         const ExactRationalVector& vector) {
  ExactRationalVector result(matrix.size(), ZeroRational());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      if (matrix[row][column].IsZero() || vector[column].IsZero()) {
        continue;
      }
      result[row] = AddRational(result[row],
                                MultiplyRational(matrix[row][column], vector[column]));
    }
  }
  return result;
}

using ExactSeriesMatrix = std::vector<std::vector<ExactSeries>>;
using LaurentSeriesMatrix = std::vector<std::vector<LaurentSeries>>;

ExactSeriesMatrix ExpandMatrixSeriesLocally(const std::vector<std::vector<std::string>>& matrix,
                                            const std::string& variable_name,
                                            const ExactRational& center_value,
                                            const ExactPassiveBindings& passive_bindings,
                                            const int order,
                                            const char* patch_prefix) {
  ExactSeriesMatrix local_series;
  local_series.reserve(matrix.size());
  for (const auto& row : matrix) {
    std::vector<ExactSeries> local_row;
    local_row.reserve(row.size());
    for (const auto& cell : row) {
      local_row.push_back(SeriesExpressionParser(cell,
                                                 variable_name,
                                                 center_value,
                                                 passive_bindings,
                                                 order,
                                                 patch_prefix)
                              .Parse());
    }
    local_series.push_back(std::move(local_row));
  }
  return local_series;
}

LaurentSeriesMatrix ExpandMatrixLaurentSeriesLocally(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name,
    const ExactRational& center_value,
    const ExactPassiveBindings& passive_bindings,
    const int order,
    const char* patch_prefix) {
  LaurentSeriesMatrix local_series;
  local_series.reserve(matrix.size());
  for (const auto& row : matrix) {
    std::vector<LaurentSeries> local_row;
    local_row.reserve(row.size());
    for (const auto& cell : row) {
      local_row.push_back(LaurentSeriesExpressionParser(cell,
                                                        variable_name,
                                                        center_value,
                                                        passive_bindings,
                                                        order,
                                                        patch_prefix)
                              .Parse());
    }
    local_series.push_back(std::move(local_row));
  }
  return local_series;
}

std::vector<ExactRationalMatrix> BuildDegreeMatrices(const ExactSeriesMatrix& local_series,
                                                     const int order) {
  const std::size_t dimension = local_series.size();
  std::vector<ExactRationalMatrix> degree_matrices(static_cast<std::size_t>(order + 1),
                                                   MakeZeroMatrix(dimension));
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < local_series[row].size(); ++column) {
      for (int degree = 0; degree <= order; ++degree) {
        degree_matrices[static_cast<std::size_t>(degree)][row][column] =
            local_series[row][column][static_cast<std::size_t>(degree)];
      }
    }
  }
  return degree_matrices;
}

std::vector<ExactRationalMatrix> BuildLaurentDegreeMatrices(
    const LaurentSeriesMatrix& local_series,
    const int min_degree,
    const int max_degree) {
  const std::size_t dimension = local_series.size();
  std::vector<ExactRationalMatrix> degree_matrices(
      static_cast<std::size_t>(max_degree - min_degree + 1), MakeZeroMatrix(dimension));
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < local_series[row].size(); ++column) {
      for (int degree = min_degree; degree <= max_degree; ++degree) {
        degree_matrices[static_cast<std::size_t>(degree - min_degree)][row][column] =
            LaurentCoefficient(local_series[row][column], degree);
      }
    }
  }
  return degree_matrices;
}

void RequireUpperTriangularLocalSupport(const std::vector<ExactRationalMatrix>& degree_matrices,
                                        const int order,
                                        const char* patch_prefix) {
  if (degree_matrices.empty()) {
    return;
  }

  const std::size_t dimension = degree_matrices.front().size();
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < row; ++column) {
      for (int degree = 0; degree <= order; ++degree) {
        if (!degree_matrices[static_cast<std::size_t>(degree)][row][column].IsZero()) {
          throw std::invalid_argument(std::string(patch_prefix) +
                                      " requires no strictly lower-triangular local support "
                                      "through order " +
                                      std::to_string(order) + " but entry (" +
                                      std::to_string(row) + "," + std::to_string(column) +
                                      ") survives at degree " + std::to_string(degree));
        }
      }
    }
  }
}

bool MatrixContainsParenthesizedDirectDifference(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name) {
  for (const auto& row : matrix) {
    for (const std::string& cell : row) {
      if (ContainsParenthesizedDirectDifference(cell, variable_name)) {
        return true;
      }
    }
  }
  return false;
}

bool HasNegativeLaurentSupport(const LaurentSeriesMatrix& local_series) {
  for (const auto& row : local_series) {
    for (const LaurentSeries& cell : row) {
      const std::optional<int> leading_order = LeadingLaurentOrder(cell);
      if (leading_order.has_value() && *leading_order < 0) {
        return true;
      }
    }
  }
  return false;
}

bool HasHigherOrderPole(const LaurentSeriesMatrix& local_series) {
  for (const auto& row : local_series) {
    for (const LaurentSeries& cell : row) {
      const std::optional<int> leading_order = LeadingLaurentOrder(cell);
      if (leading_order.has_value() && *leading_order < -1) {
        return true;
      }
    }
  }
  return false;
}

void RequireDiagonalResidueMatrix(const ExactRationalMatrix& residue_matrix,
                                  const char* patch_prefix) {
  for (std::size_t row = 0; row < residue_matrix.size(); ++row) {
    for (std::size_t column = 0; column < residue_matrix[row].size(); ++column) {
      if (row == column || residue_matrix[row][column].IsZero()) {
        continue;
      }
      throw std::invalid_argument(std::string(patch_prefix) +
                                  " requires a diagonal simple-pole residue matrix in "
                                  "declared master order but off-diagonal residue entry (" +
                                  std::to_string(row) + "," + std::to_string(column) +
                                  ") survives");
    }
  }
}

std::vector<ExactRational> ExtractDiagonalEntries(const ExactRationalMatrix& matrix) {
  std::vector<ExactRational> diagonal_entries;
  diagonal_entries.reserve(matrix.size());
  for (std::size_t index = 0; index < matrix.size(); ++index) {
    diagonal_entries.push_back(matrix[index][index]);
  }
  return diagonal_entries;
}

std::vector<ExactRationalMatrix> ComputeMatrixPatchCoefficients(
    const std::vector<ExactRationalMatrix>& degree_matrices) {
  const std::size_t dimension =
      degree_matrices.empty() ? 0 : degree_matrices.front().size();
  std::vector<ExactRationalMatrix> coefficients(
      degree_matrices.size(), MakeZeroMatrix(dimension));
  coefficients.front() = MakeIdentityMatrix(dimension);

  for (std::size_t degree = 0; degree + 1 < coefficients.size(); ++degree) {
    ExactRationalMatrix numerator = MakeZeroMatrix(dimension);
    for (std::size_t coefficient_degree = 0; coefficient_degree <= degree; ++coefficient_degree) {
      numerator = AddMatrices(
          numerator,
          MultiplyMatrices(degree_matrices[coefficient_degree],
                           coefficients[degree - coefficient_degree]));
    }
    coefficients[degree + 1] =
        DivideMatrix(numerator, IntegerRational(degree + 1));
  }

  return coefficients;
}

std::vector<ExactRationalMatrix> ComputeMatrixFrobeniusPatchCoefficients(
    const std::vector<ExactRationalMatrix>& regular_tail_degree_matrices,
    const std::vector<ExactRational>& indicial_exponents,
    const char* patch_prefix) {
  const std::size_t dimension =
      regular_tail_degree_matrices.empty() ? 0 : regular_tail_degree_matrices.front().size();
  std::vector<ExactRationalMatrix> coefficients(
      regular_tail_degree_matrices.size(), MakeZeroMatrix(dimension));
  coefficients.front() = MakeIdentityMatrix(dimension);

  for (std::size_t degree = 0; degree + 1 < coefficients.size(); ++degree) {
    ExactRationalMatrix recurrence_rhs = MakeZeroMatrix(dimension);
    for (std::size_t coefficient_degree = 0; coefficient_degree <= degree; ++coefficient_degree) {
      recurrence_rhs = AddMatrices(
          recurrence_rhs,
          MultiplyMatrices(regular_tail_degree_matrices[coefficient_degree],
                           coefficients[degree - coefficient_degree]));
    }

    ExactRationalMatrix next_coefficient = MakeZeroMatrix(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
      for (std::size_t column = 0; column < dimension; ++column) {
        const ExactRational denominator = AddRational(
            IntegerRational(degree + 1),
            SubtractRational(indicial_exponents[column], indicial_exponents[row]));
        if (!denominator.IsZero()) {
          next_coefficient[row][column] =
              DivideRational(recurrence_rhs[row][column], denominator);
          continue;
        }
        if (recurrence_rhs[row][column].IsZero()) {
          next_coefficient[row][column] = ZeroRational();
          continue;
        }
        throw std::invalid_argument(std::string(patch_prefix) +
                                    " requires logarithmic resonance handling/logarithmic "
                                    "Frobenius terms because coefficient entry (" +
                                    std::to_string(row) + "," + std::to_string(column) +
                                    ") at degree " + std::to_string(degree + 1) +
                                    " has zero recurrence denominator with nonzero right-hand "
                                    "side");
      }
    }
    coefficients[degree + 1] = std::move(next_coefficient);
  }

  return coefficients;
}

void VerifyMatrixResidual(const std::vector<ExactRationalMatrix>& degree_matrices,
                          const std::vector<ExactRationalMatrix>& patch_coefficients) {
  if (patch_coefficients.empty()) {
    return;
  }

  const std::size_t dimension = patch_coefficients.front().size();
  for (std::size_t degree = 0; degree + 1 < patch_coefficients.size(); ++degree) {
    ExactRationalMatrix product_term = MakeZeroMatrix(dimension);
    for (std::size_t coefficient_degree = 0; coefficient_degree <= degree; ++coefficient_degree) {
      product_term = AddMatrices(
          product_term,
          MultiplyMatrices(degree_matrices[coefficient_degree],
                           patch_coefficients[degree - coefficient_degree]));
    }

    const ExactRationalMatrix derivative_term =
        ScaleMatrix(patch_coefficients[degree + 1], IntegerRational(degree + 1));
    if (!MatricesEqual(derivative_term, product_term)) {
      throw std::runtime_error(std::string(kMatrixPatchPrefix) +
                               " internal residual self-check failed at degree " +
                               std::to_string(degree));
    }
  }
}

void VerifyMatrixFrobeniusReducedResidual(
    const std::vector<ExactRationalMatrix>& regular_tail_degree_matrices,
    const std::vector<ExactRational>& indicial_exponents,
    const std::vector<ExactRationalMatrix>& patch_coefficients,
    const char* patch_prefix) {
  if (patch_coefficients.empty()) {
    return;
  }

  const std::size_t dimension = patch_coefficients.front().size();
  for (std::size_t degree = 0; degree + 1 < patch_coefficients.size(); ++degree) {
    ExactRationalMatrix recurrence_rhs = MakeZeroMatrix(dimension);
    for (std::size_t coefficient_degree = 0; coefficient_degree <= degree; ++coefficient_degree) {
      recurrence_rhs = AddMatrices(
          recurrence_rhs,
          MultiplyMatrices(regular_tail_degree_matrices[coefficient_degree],
                           patch_coefficients[degree - coefficient_degree]));
    }

    ExactRationalMatrix recurrence_lhs = MakeZeroMatrix(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
      for (std::size_t column = 0; column < dimension; ++column) {
        const ExactRational denominator = AddRational(
            IntegerRational(degree + 1),
            SubtractRational(indicial_exponents[column], indicial_exponents[row]));
        recurrence_lhs[row][column] =
            MultiplyRational(denominator, patch_coefficients[degree + 1][row][column]);
      }
    }

    if (!MatricesEqual(recurrence_lhs, recurrence_rhs)) {
      throw std::runtime_error(std::string(patch_prefix) +
                               " internal reduced-equation self-check failed at degree " +
                               std::to_string(degree));
    }
  }
}

std::vector<ExactRational> ComputeScalarPatchCoefficients(const ExactSeries& coefficient_series,
                                                          const int order) {
  std::vector<ExactRational> coefficients(static_cast<std::size_t>(order + 1), ZeroRational());
  coefficients[0] = OneRational();

  for (int term = 0; term < order; ++term) {
    ExactRational numerator = ZeroRational();
    for (int coefficient_index = 0; coefficient_index <= term; ++coefficient_index) {
      numerator = AddRational(
          numerator,
          MultiplyRational(coefficient_series[static_cast<std::size_t>(coefficient_index)],
                           coefficients[static_cast<std::size_t>(term - coefficient_index)]));
    }
    coefficients[static_cast<std::size_t>(term + 1)] =
        DivideRational(numerator, IntegerRational(static_cast<std::size_t>(term + 1)));
  }

  return coefficients;
}

void VerifyScalarResidual(const ExactSeries& coefficient_series,
                          const std::vector<ExactRational>& patch_coefficients) {
  for (std::size_t degree = 0; degree + 1 < patch_coefficients.size(); ++degree) {
    ExactRational product_term = ZeroRational();
    for (std::size_t coefficient_index = 0; coefficient_index <= degree; ++coefficient_index) {
      product_term = AddRational(
          product_term,
          MultiplyRational(coefficient_series[coefficient_index],
                           patch_coefficients[degree - coefficient_index]));
    }

    const ExactRational derivative_term = MultiplyRational(
        IntegerRational(degree + 1), patch_coefficients[degree + 1]);
    if (derivative_term != product_term) {
      throw std::runtime_error(std::string(kScalarPatchPrefix) +
                               " internal residual self-check failed at degree " +
                               std::to_string(degree));
    }
  }
}

ExactSeries BuildFrobeniusReducedSeries(const LaurentSeries& coefficient_series, const int order) {
  ExactSeries reduced_series = MakeZeroSeries(order);
  for (int degree = 0; degree < order; ++degree) {
    reduced_series[static_cast<std::size_t>(degree)] =
        LaurentCoefficient(coefficient_series, degree);
  }
  return reduced_series;
}

std::vector<ExactRational> NormalizeScalarCoefficients(const SeriesPatch& patch,
                                                       const NumericEvaluationPoint& passive_bindings,
                                                       const char* patch_prefix) {
  if (patch.order < 0) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires SeriesPatch.order to be non-negative");
  }

  const std::size_t expected_size = static_cast<std::size_t>(patch.order + 1);
  if (patch.basis_functions.size() != expected_size) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires SeriesPatch.basis_functions.size() to equal "
                                "SeriesPatch.order + 1");
  }
  if (patch.coefficients.size() != expected_size) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires SeriesPatch.coefficients.size() to equal "
                                "SeriesPatch.order + 1");
  }

  std::vector<ExactRational> coefficients;
  coefficients.reserve(expected_size);
  for (const std::string& coefficient : patch.coefficients) {
    coefficients.push_back(EvaluateCoefficientExpression(coefficient, passive_bindings));
  }
  return coefficients;
}

std::vector<ExactRationalMatrix> NormalizeMatrixPatchCoefficients(
    const UpperTriangularMatrixSeriesPatch& patch,
    const NumericEvaluationPoint& passive_bindings,
    const char* patch_prefix) {
  if (patch.order < 0) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires UpperTriangularMatrixSeriesPatch.order to be "
                                "non-negative");
  }

  const std::size_t expected_size = static_cast<std::size_t>(patch.order + 1);
  if (patch.basis_functions.size() != expected_size) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires UpperTriangularMatrixSeriesPatch.basis_functions."
                                "size() to equal UpperTriangularMatrixSeriesPatch.order + 1");
  }
  if (patch.coefficient_matrices.size() != expected_size) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires UpperTriangularMatrixSeriesPatch.coefficient_"
                                "matrices.size() to equal UpperTriangularMatrixSeriesPatch."
                                "order + 1");
  }

  const std::size_t dimension =
      patch.coefficient_matrices.empty() ? 0 : patch.coefficient_matrices.front().size();
  std::vector<ExactRationalMatrix> normalized_matrices;
  normalized_matrices.reserve(expected_size);
  for (const ExactRationalMatrix& matrix : patch.coefficient_matrices) {
    if (matrix.size() != dimension) {
      throw std::invalid_argument(std::string(patch_prefix) +
                                  " requires square stored matrix coefficients");
    }

    ExactRationalMatrix normalized_matrix = MakeZeroMatrix(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
      if (matrix[row].size() != dimension) {
        throw std::invalid_argument(std::string(patch_prefix) +
                                    " requires square stored matrix coefficients");
      }
      for (std::size_t column = 0; column < dimension; ++column) {
        normalized_matrix[row][column] =
            EvaluateCoefficientExpression(matrix[row][column].ToString(), passive_bindings);
        if (column < row && !normalized_matrix[row][column].IsZero()) {
          throw std::invalid_argument(std::string(patch_prefix) +
                                      " requires stored upper-triangular coefficient matrices");
        }
      }
    }
    normalized_matrices.push_back(std::move(normalized_matrix));
  }

  return normalized_matrices;
}

std::optional<long long> ParseExactSignedInteger(const ExactRational& value) {
  const ExactRational normalized = ExactArithmetic(value.ToString());
  if (normalized.denominator != "1") {
    return std::nullopt;
  }

  try {
    return std::stoll(normalized.numerator);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

ExactRational RaiseRationalToIntegerPower(const ExactRational& base, const long long exponent) {
  if (exponent == 0) {
    return OneRational();
  }

  ExactRational factor = base;
  unsigned long long remaining = static_cast<unsigned long long>(exponent);
  if (exponent < 0) {
    factor = DivideRational(OneRational(), base);
    remaining = static_cast<unsigned long long>(-(exponent + 1));
    ++remaining;
  }

  ExactRational result = OneRational();
  while (remaining > 0) {
    if ((remaining & 1ULL) != 0U) {
      result = MultiplyRational(result, factor);
    }
    remaining >>= 1U;
    if (remaining > 0) {
      factor = MultiplyRational(factor, factor);
    }
  }
  return result;
}

NormalizedUpperTriangularMatrixFrobeniusPatchData NormalizeMatrixFrobeniusPatchCoefficients(
    const UpperTriangularMatrixFrobeniusSeriesPatch& patch,
    const char* patch_prefix) {
  UpperTriangularMatrixSeriesPatch regularized_patch;
  regularized_patch.center = patch.center;
  regularized_patch.order = patch.order;
  regularized_patch.basis_functions = patch.basis_functions;
  regularized_patch.coefficient_matrices = patch.coefficient_matrices;

  NormalizedUpperTriangularMatrixFrobeniusPatchData normalized_patch;
  normalized_patch.coefficient_matrices =
      NormalizeMatrixPatchCoefficients(regularized_patch, {}, patch_prefix);
  const std::size_t dimension = normalized_patch.coefficient_matrices.empty()
                                    ? 0
                                    : normalized_patch.coefficient_matrices.front().size();
  if (patch.indicial_exponents.size() != dimension) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires Frobenius indicial exponent count to match "
                                "matrix dimension");
  }

  normalized_patch.indicial_exponents.reserve(patch.indicial_exponents.size());
  normalized_patch.integral_exponents.reserve(patch.indicial_exponents.size());
  for (const std::string& exponent_expression : patch.indicial_exponents) {
    const ExactRational exponent = ExactArithmetic(exponent_expression);
    const std::optional<long long> integral_exponent = ParseExactSignedInteger(exponent);
    if (!integral_exponent.has_value()) {
      throw std::invalid_argument(
          std::string(patch_prefix) +
          " currently requires integral/integer Frobenius exponents on the mixed "
          "regular/regular-singular path");
    }
    normalized_patch.indicial_exponents.push_back(exponent);
    normalized_patch.integral_exponents.push_back(*integral_exponent);
  }

  return normalized_patch;
}

ExactRational EvaluateScalarPolynomial(const std::vector<ExactRational>& coefficients,
                                       const ExactRational& center_value,
                                       const ExactRational& point_value) {
  ExactRational value = ZeroRational();
  ExactRational power = OneRational();
  const ExactRational shift = SubtractRational(point_value, center_value);
  for (const ExactRational& coefficient : coefficients) {
    value = AddRational(value, MultiplyRational(coefficient, power));
    power = MultiplyRational(power, shift);
  }
  return value;
}

ExactRational EvaluateScalarPolynomialDerivative(const std::vector<ExactRational>& coefficients,
                                                 const ExactRational& center_value,
                                                 const ExactRational& point_value) {
  ExactRational derivative = ZeroRational();
  ExactRational power = OneRational();
  const ExactRational shift = SubtractRational(point_value, center_value);
  for (std::size_t degree = 1; degree < coefficients.size(); ++degree) {
    derivative =
        AddRational(derivative,
                    MultiplyRational(MultiplyRational(IntegerRational(degree),
                                                      coefficients[degree]),
                                     power));
    power = MultiplyRational(power, shift);
  }
  return derivative;
}

ExactRationalMatrix EvaluateMatrixPolynomial(
    const std::vector<ExactRationalMatrix>& coefficient_matrices,
    const ExactRational& center_value,
    const ExactRational& point_value) {
  const std::size_t dimension =
      coefficient_matrices.empty() ? 0 : coefficient_matrices.front().size();
  ExactRationalMatrix value = MakeZeroMatrix(dimension);
  ExactRational power = OneRational();
  const ExactRational shift = SubtractRational(point_value, center_value);
  for (const ExactRationalMatrix& coefficient_matrix : coefficient_matrices) {
    value = AddMatrices(value, ScaleMatrix(coefficient_matrix, power));
    power = MultiplyRational(power, shift);
  }
  return value;
}

ExactRationalMatrix EvaluateMatrixPolynomialDerivative(
    const std::vector<ExactRationalMatrix>& coefficient_matrices,
    const ExactRational& center_value,
    const ExactRational& point_value) {
  const std::size_t dimension =
      coefficient_matrices.empty() ? 0 : coefficient_matrices.front().size();
  ExactRationalMatrix derivative = MakeZeroMatrix(dimension);
  ExactRational power = OneRational();
  const ExactRational shift = SubtractRational(point_value, center_value);
  for (std::size_t degree = 1; degree < coefficient_matrices.size(); ++degree) {
    derivative = AddMatrices(
        derivative,
        ScaleMatrix(coefficient_matrices[degree],
                    MultiplyRational(IntegerRational(degree), power)));
    power = MultiplyRational(power, shift);
  }
  return derivative;
}

ExactRationalMatrix InvertUpperTriangularMatrix(const ExactRationalMatrix& matrix) {
  const std::size_t dimension = matrix.size();
  ExactRationalMatrix inverse = MakeZeroMatrix(dimension);
  for (std::size_t row_offset = 0; row_offset < dimension; ++row_offset) {
    const std::size_t row = dimension - 1 - row_offset;
    inverse[row][row] = DivideRational(OneRational(), matrix[row][row]);
    for (std::size_t column = row + 1; column < dimension; ++column) {
      ExactRational sum = ZeroRational();
      for (std::size_t inner = row + 1; inner <= column; ++inner) {
        sum = AddRational(sum,
                          MultiplyRational(matrix[row][inner], inverse[inner][column]));
      }
      inverse[row][column] =
          NegateRational(DivideRational(sum, matrix[row][row]));
    }
  }
  return inverse;
}

ExactRationalMatrix RightScaleMatrixColumns(const ExactRationalMatrix& matrix,
                                            const ExactRationalVector& column_scales) {
  ExactRationalMatrix result = MakeZeroMatrix(matrix.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      result[row][column] = MultiplyRational(matrix[row][column], column_scales[column]);
    }
  }
  return result;
}

ExactRationalVector ComputeFrobeniusColumnScales(
    const NormalizedUpperTriangularMatrixFrobeniusPatchData& patch,
    const ExactRational& shift_value) {
  ExactRationalVector column_scales;
  column_scales.reserve(patch.integral_exponents.size());
  for (const long long exponent : patch.integral_exponents) {
    column_scales.push_back(RaiseRationalToIntegerPower(shift_value, exponent));
  }
  return column_scales;
}

ExactRationalVector ComputeFrobeniusColumnDerivativeScales(
    const NormalizedUpperTriangularMatrixFrobeniusPatchData& patch,
    const ExactRational& shift_value) {
  ExactRationalVector derivative_scales;
  derivative_scales.reserve(patch.integral_exponents.size());
  for (std::size_t index = 0; index < patch.integral_exponents.size(); ++index) {
    if (patch.integral_exponents[index] == 0) {
      derivative_scales.push_back(ZeroRational());
      continue;
    }
    derivative_scales.push_back(MultiplyRational(
        patch.indicial_exponents[index],
        RaiseRationalToIntegerPower(shift_value, patch.integral_exponents[index] - 1)));
  }
  return derivative_scales;
}

ExactRationalMatrix EvaluateMatrixFrobeniusFundamentalMatrix(
    const NormalizedUpperTriangularMatrixFrobeniusPatchData& patch,
    const ExactRational& center_value,
    const ExactRational& point_value) {
  const ExactRational shift_value = SubtractRational(point_value, center_value);
  const ExactRationalMatrix regular_factor =
      EvaluateMatrixPolynomial(patch.coefficient_matrices, center_value, point_value);
  return RightScaleMatrixColumns(regular_factor,
                                 ComputeFrobeniusColumnScales(patch, shift_value));
}

ExactRationalMatrix EvaluateMatrixFrobeniusFundamentalMatrixDerivative(
    const NormalizedUpperTriangularMatrixFrobeniusPatchData& patch,
    const ExactRational& center_value,
    const ExactRational& point_value) {
  const ExactRational shift_value = SubtractRational(point_value, center_value);
  const ExactRationalMatrix regular_factor =
      EvaluateMatrixPolynomial(patch.coefficient_matrices, center_value, point_value);
  const ExactRationalMatrix regular_factor_derivative =
      EvaluateMatrixPolynomialDerivative(patch.coefficient_matrices, center_value, point_value);
  return AddMatrices(
      RightScaleMatrixColumns(regular_factor_derivative,
                              ComputeFrobeniusColumnScales(patch, shift_value)),
      RightScaleMatrixColumns(regular_factor,
                              ComputeFrobeniusColumnDerivativeScales(patch, shift_value)));
}

ExactRationalMatrix EvaluateUpperTriangularMatrixFrobeniusPatchResidual(
    const DESystem& system,
    const std::string& variable_name,
    const ExactRational& center_value,
    const ExactRational& point_value,
    const NormalizedUpperTriangularMatrixFrobeniusPatchData& patch,
    const NumericEvaluationPoint& passive_bindings) {
  NumericEvaluationPoint evaluation_point = passive_bindings;
  evaluation_point[variable_name] = point_value.ToString();
  const ExactRationalMatrix coefficient_matrix =
      EvaluateCoefficientMatrix(system, variable_name, evaluation_point);
  const ExactRationalMatrix patch_value =
      EvaluateMatrixFrobeniusFundamentalMatrix(patch, center_value, point_value);
  const ExactRationalMatrix patch_derivative =
      EvaluateMatrixFrobeniusFundamentalMatrixDerivative(patch, center_value, point_value);
  return SubtractMatrices(patch_derivative, MultiplyMatrices(coefficient_matrix, patch_value));
}

void RequireDistinctPoints(const ExactRational& match_point,
                           const ExactRational& check_point,
                           const char* patch_prefix) {
  if (match_point == check_point) {
    throw std::invalid_argument(std::string(patch_prefix) +
                                " requires distinct match and check points after exact "
                                "resolution");
  }
}

std::vector<std::string> BuildBasisFunctions(const std::string& variable_name,
                                             const ExactRational& center_value,
                                             const int order) {
  std::vector<std::string> basis_functions;
  basis_functions.reserve(static_cast<std::size_t>(order + 1));
  basis_functions.push_back("1");

  if (order == 0) {
    return basis_functions;
  }

  const std::string shift = "(" + variable_name + "-(" + center_value.ToString() + "))";
  std::string monomial = shift;
  basis_functions.push_back(monomial);
  for (int degree = 2; degree <= order; ++degree) {
    monomial = "(" + monomial + "*" + shift + ")";
    basis_functions.push_back(monomial);
  }
  return basis_functions;
}

PointClassification ClassifyRegularCenterWithFallback(const DESystem& system,
                                                      const std::string& variable_name,
                                                      const std::string& resolved_center_expression,
                                                      const ExactRational& center_value,
                                                      const NumericEvaluationPoint& passive_bindings,
                                                      const char* patch_prefix) {
  try {
    return ClassifyFinitePoint(system, variable_name, resolved_center_expression, passive_bindings);
  } catch (const std::invalid_argument& error) {
    if (!IsUnsupportedSingularFormError(error)) {
      throw;
    }

    NumericEvaluationPoint center_evaluation_point = passive_bindings;
    center_evaluation_point[variable_name] = center_value.ToString();
    ExactRationalMatrix center_matrix;
    try {
      center_matrix = EvaluateCoefficientMatrix(system, variable_name, center_evaluation_point);
    } catch (const std::runtime_error& runtime_error) {
      if (std::string(runtime_error.what()).find("coefficient evaluation encountered division by zero") !=
          std::string::npos) {
        return PointClassification::Singular;
      }
      throw;
    }

    DESystem regularity_probe;
    regularity_probe.masters = system.masters;
    regularity_probe.variables = {ResolveSelectedVariable(system, variable_name, patch_prefix)};
    std::vector<std::vector<std::string>> probe_matrix;
    probe_matrix.reserve(center_matrix.size());
    for (const auto& row : center_matrix) {
      std::vector<std::string> probe_row;
      probe_row.reserve(row.size());
      for (const ExactRational& cell : row) {
        probe_row.push_back(cell.ToString());
      }
      probe_matrix.push_back(std::move(probe_row));
    }
    regularity_probe.coefficient_matrices[variable_name] = std::move(probe_matrix);
    return ClassifyFinitePoint(regularity_probe,
                               variable_name,
                               resolved_center_expression,
                               passive_bindings);
  }
}

std::string SelectBuiltinEtaModeName(const ProblemSpec& spec,
                                     const std::vector<std::string>& eta_mode_names) {
  if (eta_mode_names.empty()) {
    throw std::invalid_argument("builtin eta-mode list must not be empty");
  }

  for (std::size_t index = 0; index < eta_mode_names.size(); ++index) {
    const std::string& eta_mode_name = eta_mode_names[index];
    const std::shared_ptr<EtaMode> eta_mode = MakeBuiltinEtaMode(eta_mode_name);
    try {
      static_cast<void>(eta_mode->Plan(spec));
      return eta_mode_name;
    } catch (const std::runtime_error&) {
      if (eta_mode_name == "Branch" || eta_mode_name == "Loop" ||
          index + 1 == eta_mode_names.size()) {
        throw;
      }
    }
  }

  throw std::runtime_error("failed to select a builtin eta mode");
}

EtaInsertionDecision SelectResolvedEtaModeDecision(
    const ProblemSpec& spec,
    const std::vector<std::string>& eta_mode_names,
    const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes) {
  if (eta_mode_names.empty()) {
    throw std::invalid_argument("eta-mode list must not be empty");
  }

  for (std::size_t index = 0; index < eta_mode_names.size(); ++index) {
    const std::string& eta_mode_name = eta_mode_names[index];
    const std::shared_ptr<EtaMode> eta_mode =
        ResolveEtaMode(eta_mode_name, user_defined_modes);
    try {
      return eta_mode->Plan(spec);
    } catch (const std::exception&) {
      if (eta_mode_name == "Branch" || eta_mode_name == "Loop" ||
          index + 1 == eta_mode_names.size()) {
        throw;
      }
    }
  }

  throw std::runtime_error("failed to select an eta mode");
}

PrecisionPolicy BuildAmfOptionsPrecisionPolicy(const PrecisionPolicy& base_policy,
                                               const AmfOptions& amf_options) {
  PrecisionPolicy live_policy = base_policy;
  live_policy.working_precision = amf_options.working_precision;
  live_policy.chop_precision = amf_options.chop_precision;
  live_policy.x_order = amf_options.x_order;
  live_policy.rationalize_precision = amf_options.rationalize_precision;
  return live_policy;
}

AmfSolveRuntimePolicy BuildAmfOptionsRuntimePolicy(const AmfOptions& amf_options) {
  AmfSolveRuntimePolicy live_policy;
  live_policy.extra_x_order = amf_options.extra_x_order;
  live_policy.learn_x_order = amf_options.learn_x_order;
  live_policy.test_x_order = amf_options.test_x_order;
  live_policy.run_length = amf_options.run_length;
  return live_policy;
}

std::optional<std::string> BuildAmfRequestedDimensionExpression(
    const std::optional<std::string>& amf_requested_d0,
    const std::optional<std::string>& fixed_eps) {
  if (!amf_requested_d0.has_value()) {
    return std::nullopt;
  }

  const std::string d0_expression = "(" + Trim(*amf_requested_d0) + ")";
  if (!fixed_eps.has_value()) {
    return d0_expression + "-2*eps";
  }

  const std::string dimension_expression =
      d0_expression + "-2*(" + Trim(*fixed_eps) + ")";
  try {
    return EvaluateCoefficientExpression(dimension_expression, NumericEvaluationPoint{})
        .ToString();
  } catch (const std::exception&) {
    return dimension_expression;
  }
}

const char* ToString(DifferentiationVariableKind kind) {
  switch (kind) {
    case DifferentiationVariableKind::Eta:
      return "eta";
    case DifferentiationVariableKind::Invariant:
      return "invariant";
    case DifferentiationVariableKind::Dimension:
      return "dimension";
    case DifferentiationVariableKind::Auxiliary:
      return "auxiliary";
  }
  return "unknown";
}

std::string SerializeTargetIntegralForFingerprint(const TargetIntegral& integral) {
  return integral.Label();
}

std::string SerializeMasterIntegralForFingerprint(const MasterIntegral& integral) {
  std::ostringstream out;
  out << "family=" << integral.family << ";label=" << integral.label << ";indices=";
  for (std::size_t index = 0; index < integral.indices.size(); ++index) {
    if (index > 0) {
      out << ",";
    }
    out << integral.indices[index];
  }
  return out.str();
}

std::string SerializeParsedMasterListForFingerprint(const ParsedMasterList& master_basis) {
  std::ostringstream out;
  out << "family=" << master_basis.family << "\n";
  out << "masters=" << master_basis.masters.size() << "\n";
  for (std::size_t index = 0; index < master_basis.masters.size(); ++index) {
    out << "master[" << index << "]="
        << SerializeTargetIntegralForFingerprint(master_basis.masters[index]) << "\n";
  }
  return out.str();
}

std::string SerializeEtaInsertionDecisionForFingerprint(const EtaInsertionDecision& decision) {
  std::ostringstream out;
  out << "mode_name=" << decision.mode_name << "\n";
  out << "selected_indices=" << decision.selected_propagator_indices.size() << "\n";
  for (std::size_t index = 0; index < decision.selected_propagator_indices.size(); ++index) {
    out << "selected_index[" << index << "]="
        << decision.selected_propagator_indices[index] << "\n";
  }
  out << "selected_propagators=" << decision.selected_propagators.size() << "\n";
  for (std::size_t index = 0; index < decision.selected_propagators.size(); ++index) {
    out << "selected_propagator[" << index << "]="
        << decision.selected_propagators[index] << "\n";
  }
  return out.str();
}

std::string SerializePrecisionPolicyForFingerprint(const PrecisionPolicy& policy) {
  std::ostringstream out;
  out << "working_precision=" << policy.working_precision << "\n";
  out << "chop_precision=" << policy.chop_precision << "\n";
  out << "rationalize_precision=" << policy.rationalize_precision << "\n";
  out << "escalation_step=" << policy.escalation_step << "\n";
  out << "max_working_precision=" << policy.max_working_precision << "\n";
  out << "x_order=" << policy.x_order << "\n";
  out << "x_order_step=" << policy.x_order_step << "\n";
  return out.str();
}

std::string SerializeAmfSolveRuntimePolicyForFingerprint(
    const AmfSolveRuntimePolicy& policy) {
  std::ostringstream out;
  out << "extra_x_order=" << policy.extra_x_order << "\n";
  out << "learn_x_order=" << policy.learn_x_order << "\n";
  out << "test_x_order=" << policy.test_x_order << "\n";
  out << "run_length=" << policy.run_length << "\n";
  return out.str();
}

std::string SerializeOptionalAmfSolveRuntimePolicyForFingerprint(
    const std::optional<AmfSolveRuntimePolicy>& policy) {
  std::ostringstream out;
  out << "present=" << (policy.has_value() ? "true" : "false") << "\n";
  if (policy.has_value()) {
    out << SerializeAmfSolveRuntimePolicyForFingerprint(*policy);
  }
  return out.str();
}

std::string SerializeOptionalAmfRequestedD0ForFingerprint(
    const std::optional<std::string>& amf_requested_d0) {
  std::ostringstream out;
  out << "present=" << (amf_requested_d0.has_value() ? "true" : "false") << "\n";
  if (amf_requested_d0.has_value()) {
    out << "value=" << *amf_requested_d0 << "\n";
  }
  return out.str();
}

std::string SerializeOptionalAmfRequestedDimensionExpressionForFingerprint(
    const std::optional<std::string>& amf_requested_dimension_expression) {
  std::ostringstream out;
  out << "present=" << (amf_requested_dimension_expression.has_value() ? "true" : "false")
      << "\n";
  if (amf_requested_dimension_expression.has_value()) {
    out << "value=" << *amf_requested_dimension_expression << "\n";
  }
  return out.str();
}

std::string SerializeOptionalAmfFixedEpsForFingerprint(
    const std::optional<std::string>& fixed_eps) {
  std::ostringstream out;
  out << "present=" << (fixed_eps.has_value() ? "true" : "false") << "\n";
  if (fixed_eps.has_value()) {
    out << "value=" << *fixed_eps << "\n";
  }
  return out.str();
}

std::string SerializeDESystemForFingerprint(const DESystem& system) {
  std::ostringstream out;
  out << "masters=" << system.masters.size() << "\n";
  for (std::size_t index = 0; index < system.masters.size(); ++index) {
    out << "master[" << index << "]="
        << SerializeMasterIntegralForFingerprint(system.masters[index]) << "\n";
  }

  out << "variables=" << system.variables.size() << "\n";
  for (std::size_t index = 0; index < system.variables.size(); ++index) {
    const DifferentiationVariable& variable = system.variables[index];
    out << "variable[" << index << "].name=" << variable.name << "\n";
    out << "variable[" << index << "].kind=" << ToString(variable.kind) << "\n";

    const auto matrix_it = system.coefficient_matrices.find(variable.name);
    if (matrix_it == system.coefficient_matrices.end()) {
      out << "variable[" << index << "].matrix=missing\n";
      continue;
    }

    const auto& matrix = matrix_it->second;
    out << "variable[" << index << "].rows=" << matrix.size() << "\n";
    for (std::size_t row = 0; row < matrix.size(); ++row) {
      out << "variable[" << index << "].row[" << row << "].cols=" << matrix[row].size()
          << "\n";
      for (std::size_t column = 0; column < matrix[row].size(); ++column) {
        out << "variable[" << index << "].cell[" << row << "][" << column << "]="
            << matrix[row][column] << "\n";
      }
    }
  }

  out << "singular_points=" << system.singular_points.size() << "\n";
  for (std::size_t index = 0; index < system.singular_points.size(); ++index) {
    out << "singular_point[" << index << "]=" << system.singular_points[index] << "\n";
  }
  return out.str();
}

std::string SerializeSolveRequestForFingerprint(const SolveRequest& request) {
  std::ostringstream out;
  out << "system:\n" << SerializeDESystemForFingerprint(request.system);
  out << "boundary_requests=" << request.boundary_requests.size() << "\n";
  for (std::size_t index = 0; index < request.boundary_requests.size(); ++index) {
    const BoundaryRequest& boundary_request = request.boundary_requests[index];
    out << "boundary_request[" << index << "].variable=" << boundary_request.variable << "\n";
    out << "boundary_request[" << index << "].location=" << boundary_request.location << "\n";
    out << "boundary_request[" << index << "].strategy=" << boundary_request.strategy << "\n";
  }
  out << "boundary_conditions=" << request.boundary_conditions.size() << "\n";
  for (std::size_t index = 0; index < request.boundary_conditions.size(); ++index) {
    const BoundaryCondition& boundary_condition = request.boundary_conditions[index];
    out << "boundary_condition[" << index << "].variable=" << boundary_condition.variable
        << "\n";
    out << "boundary_condition[" << index << "].location=" << boundary_condition.location
        << "\n";
    out << "boundary_condition[" << index << "].strategy=" << boundary_condition.strategy
        << "\n";
    out << "boundary_condition[" << index << "].values=" << boundary_condition.values.size()
        << "\n";
    for (std::size_t value_index = 0; value_index < boundary_condition.values.size();
         ++value_index) {
      out << "boundary_condition[" << index << "].value[" << value_index << "]="
          << boundary_condition.values[value_index] << "\n";
    }
  }
  out << "start_location=" << request.start_location << "\n";
  out << "target_location=" << request.target_location << "\n";
  out << "requested_digits=" << request.requested_digits << "\n";
  out << "precision_policy:\n" << SerializePrecisionPolicyForFingerprint(request.precision_policy);
  out << "amf_runtime_policy:\n"
      << SerializeOptionalAmfSolveRuntimePolicyForFingerprint(request.amf_runtime_policy);
  out << "amf_requested_d0:\n"
      << SerializeOptionalAmfRequestedD0ForFingerprint(request.amf_requested_d0);
  out << "amf_requested_dimension_expression:\n"
      << SerializeOptionalAmfRequestedDimensionExpressionForFingerprint(
             request.amf_requested_dimension_expression);
  return out.str();
}

std::string ComputeSolveRequestFingerprint(const SolveRequest& request) {
  return ComputeArtifactFingerprint(SerializeSolveRequestForFingerprint(request));
}

std::string BuildSeriesSolverReplayFingerprint(const SeriesSolver& solver) {
  return ComputeArtifactFingerprint("series_solver_dynamic_type=" +
                                    std::string(typeid(solver).name()));
}

std::string BuildEtaGeneratedSolveInputFingerprint(
    const std::string& solve_kind,
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    const std::optional<AmfSolveRuntimePolicy>& amf_runtime_policy,
    const std::optional<std::string>& amf_requested_d0,
    const std::optional<std::string>& fixed_eps,
    const std::optional<std::string>& explicit_dimension_expression,
    const int requested_digits,
    const std::string& eta_symbol) {
  std::ostringstream out;
  out << "solve_kind=" << solve_kind << "\n";
  out << "spec_yaml:\n" << SerializeProblemSpecYaml(spec) << "\n";
  out << "master_basis:\n" << SerializeParsedMasterListForFingerprint(master_basis);
  out << "eta_decision:\n" << SerializeEtaInsertionDecisionForFingerprint(decision);
  out << "reduction_options:\n" << SerializeReductionOptionsYaml(options) << "\n";
  out << "solver_replay_fingerprint=" << BuildSeriesSolverReplayFingerprint(solver) << "\n";
  out << "start_location=" << start_location << "\n";
  out << "target_location=" << target_location << "\n";
  out << "requested_digits=" << requested_digits << "\n";
  out << "eta_symbol=" << eta_symbol << "\n";
  out << "precision_policy:\n" << SerializePrecisionPolicyForFingerprint(precision_policy);
  out << "amf_runtime_policy:\n"
      << SerializeOptionalAmfSolveRuntimePolicyForFingerprint(amf_runtime_policy);
  out << "amf_requested_d0:\n"
      << SerializeOptionalAmfRequestedD0ForFingerprint(amf_requested_d0);
  out << "amf_fixed_eps:\n" << SerializeOptionalAmfFixedEpsForFingerprint(fixed_eps);
  out << "exact_dimension_override:\n"
      << SerializeOptionalAmfRequestedDimensionExpressionForFingerprint(
             explicit_dimension_expression);
  return ComputeArtifactFingerprint(out.str());
}

std::string SanitizeCacheSlotComponent(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const char character : value) {
    const bool safe = (character >= 'a' && character <= 'z') ||
                      (character >= 'A' && character <= 'Z') ||
                      (character >= '0' && character <= '9') || character == '-' ||
                      character == '_' || character == '.';
    sanitized.push_back(safe ? character : '-');
  }
  return sanitized.empty() ? "unnamed" : sanitized;
}

std::string MakeSolvedPathCacheSlotName(const std::string& solve_kind,
                                        const ProblemSpec& spec,
                                        const EtaInsertionDecision& decision,
                                        const std::string& eta_symbol,
                                        const std::optional<std::string>& amf_requested_d0,
                                        const std::optional<std::string>& fixed_eps,
                                        const std::optional<std::string>& explicit_dimension_expression) {
  std::string slot_name = solve_kind + "-" + SanitizeCacheSlotComponent(spec.family.name) + "-" +
                          SanitizeCacheSlotComponent(decision.mode_name) + "-" +
                          SanitizeCacheSlotComponent(eta_symbol);
  if (amf_requested_d0.has_value()) {
    slot_name += "-" + SanitizeCacheSlotComponent(*amf_requested_d0);
  }
  if (explicit_dimension_expression.has_value()) {
    slot_name += "-exact-dimension-" + SanitizeCacheSlotComponent(*explicit_dimension_expression);
  } else if (fixed_eps.has_value()) {
    slot_name += "-fixed-eps-" + SanitizeCacheSlotComponent(*fixed_eps);
  }
  return slot_name;
}

struct SolvedPathCacheContext {
  bool replay_enabled = false;
  std::string solve_kind;
  std::string slot_name;
  std::string input_fingerprint;
  std::optional<std::string> expected_request_fingerprint;
};

enum class SolvedPathCacheReplayStatus {
  Miss,
  Rejected,
  Hit,
};

struct SolvedPathCacheReplayResult {
  SolvedPathCacheReplayStatus status = SolvedPathCacheReplayStatus::Miss;
  std::optional<SolverDiagnostics> diagnostics;
};

SolvedPathCacheReplayResult TryReplaySolvedPathCache(const ArtifactLayout& layout,
                                                     const SolvedPathCacheContext& cache_context) {
  if (!cache_context.replay_enabled) {
    return {};
  }

  const std::filesystem::path manifest_path =
      ResolveSolvedPathCacheManifestPath(layout, cache_context.slot_name);
  if (!std::filesystem::exists(manifest_path)) {
    return {};
  }

  try {
    const SolvedPathCacheManifest manifest = ReadSolvedPathCacheManifest(manifest_path);
    if (manifest.manifest_kind != "solved-path-cache" ||
        manifest.schema_version != kSolvedPathCacheSchemaVersion ||
        manifest.solve_kind != cache_context.solve_kind ||
        manifest.slot_name != cache_context.slot_name ||
        manifest.input_fingerprint != cache_context.input_fingerprint ||
        (cache_context.expected_request_fingerprint.has_value() &&
         manifest.request_fingerprint != *cache_context.expected_request_fingerprint) ||
        manifest.request_fingerprint.empty() || !manifest.success) {
      return {SolvedPathCacheReplayStatus::Rejected, std::nullopt};
    }

    SolverDiagnostics diagnostics;
    diagnostics.success = manifest.success;
    diagnostics.residual_norm = manifest.residual_norm;
    diagnostics.overlap_mismatch = manifest.overlap_mismatch;
    diagnostics.failure_code = manifest.failure_code;
    diagnostics.summary = manifest.summary;
    return {SolvedPathCacheReplayStatus::Hit, diagnostics};
  } catch (const std::exception&) {
    return {SolvedPathCacheReplayStatus::Rejected, std::nullopt};
  }
}

void PersistSolvedPathCacheManifest(const ArtifactLayout& layout,
                                    const SolvedPathCacheContext& cache_context,
                                    const SolveRequest& request,
                                    const SolverDiagnostics& diagnostics) {
  if (!diagnostics.success) {
    return;
  }

  const std::filesystem::path manifest_path =
      ResolveSolvedPathCacheManifestPath(layout, cache_context.slot_name);

  SolvedPathCacheManifest manifest;
  manifest.schema_version = kSolvedPathCacheSchemaVersion;
  manifest.solve_kind = cache_context.solve_kind;
  manifest.slot_name = cache_context.slot_name;
  manifest.input_fingerprint = cache_context.input_fingerprint;
  manifest.request_fingerprint = ComputeSolveRequestFingerprint(request);
  manifest.request_summary = DescribeDESystem(request.system) + "; start=" + request.start_location +
                             "; target=" + request.target_location +
                             "; requested_digits=" + std::to_string(request.requested_digits) +
                             "; precision_policy=" +
                             DescribePrecisionPolicy(request.precision_policy);
  if (request.amf_runtime_policy.has_value()) {
    manifest.request_summary +=
        "; amf_runtime_policy=" + DescribeAmfSolveRuntimePolicy(*request.amf_runtime_policy);
  }
  if (request.amf_requested_d0.has_value()) {
    manifest.request_summary += "; amf_requested_d0=" + *request.amf_requested_d0;
  }
  if (request.amf_requested_dimension_expression.has_value()) {
    manifest.request_summary +=
        "; amf_requested_dimension_expression=" + *request.amf_requested_dimension_expression;
  }
  manifest.cache_root = AbsoluteOrEmpty(manifest_path.parent_path());
  manifest.manifest_path = AbsoluteOrEmpty(manifest_path);
  manifest.success = diagnostics.success;
  manifest.residual_norm = diagnostics.residual_norm;
  manifest.overlap_mismatch = diagnostics.overlap_mismatch;
  manifest.failure_code = diagnostics.failure_code;
  manifest.summary = diagnostics.summary;

  try {
    WriteSolvedPathCacheManifest(manifest_path, manifest);
  } catch (const std::exception&) {
  }
}

SolverDiagnostics MakeUnsupportedSolverPathDiagnostics(const std::string& summary) {
  SolverDiagnostics diagnostics;
  diagnostics.success = false;
  diagnostics.residual_norm = 1.0;
  diagnostics.overlap_mismatch = 1.0;
  diagnostics.failure_code = kUnsupportedSolverPathCode;
  diagnostics.summary = summary;
  return diagnostics;
}

SolverDiagnostics MakeBoundaryUnsolvedDiagnostics(const BoundaryUnsolvedError& error) {
  SolverDiagnostics diagnostics;
  diagnostics.success = false;
  diagnostics.residual_norm = 1.0;
  diagnostics.overlap_mismatch = 1.0;
  diagnostics.failure_code = error.failure_code();
  diagnostics.summary = error.what();
  return diagnostics;
}

SolverDiagnostics MakeInsufficientPrecisionDiagnostics(const PrecisionDecision& decision) {
  SolverDiagnostics diagnostics;
  diagnostics.success = false;
  diagnostics.residual_norm = 1.0;
  diagnostics.overlap_mismatch = 1.0;
  diagnostics.failure_code = kInsufficientPrecisionCode;
  diagnostics.summary = decision.reason.empty()
                             ? "insufficient_precision: precision budget was exhausted"
                             : decision.reason;
  return diagnostics;
}

std::string StripFailureCodePrefix(const std::string& summary, const std::string& failure_code) {
  const std::string prefix = failure_code + ": ";
  if (summary.rfind(prefix, 0) == 0) {
    return summary.substr(prefix.size());
  }
  return summary;
}

SolverDiagnostics MakeRetryCeilingInsufficientPrecisionDiagnostics(
    const SolverDiagnostics& diagnostics,
    const std::string& reason) {
  SolverDiagnostics insufficient = diagnostics;
  insufficient.success = false;
  insufficient.failure_code = kInsufficientPrecisionCode;

  std::string summary_reason = StripFailureCodePrefix(reason, kInsufficientPrecisionCode);
  if (summary_reason.empty()) {
    summary_reason = "precision ceiling reached before satisfying the stability checks";
  }

  insufficient.summary = std::string(kInsufficientPrecisionCode) + ": " + summary_reason;
  if (!diagnostics.summary.empty() && diagnostics.summary != insufficient.summary) {
    insufficient.summary += "; last_solver_diagnostic=" + diagnostics.summary;
  }
  return insufficient;
}

SolverDiagnostics MakeMasterSetInstabilityDiagnostics(const std::string& summary) {
  SolverDiagnostics diagnostics;
  diagnostics.success = false;
  diagnostics.residual_norm = 1.0;
  diagnostics.overlap_mismatch = 1.0;
  diagnostics.failure_code = kMasterSetInstabilityCode;
  diagnostics.summary = summary.rfind(std::string(kMasterSetInstabilityCode) + ": ", 0) == 0
                            ? summary
                            : std::string(kMasterSetInstabilityCode) + ": " + summary;
  return diagnostics;
}

SolverDiagnostics MakeContinuationBudgetExhaustedDiagnostics(
    const SolverDiagnostics& diagnostics,
    const std::string& reason) {
  SolverDiagnostics exhausted = diagnostics;
  exhausted.success = false;
  exhausted.failure_code = kContinuationBudgetExhaustedCode;

  std::string summary_reason = StripFailureCodePrefix(reason, kInsufficientPrecisionCode);
  if (summary_reason.empty()) {
    summary_reason = "precision retry controller could not make further monotone progress";
  }

  exhausted.summary = std::string(kContinuationBudgetExhaustedCode) + ": " + summary_reason;
  if (!diagnostics.summary.empty()) {
    exhausted.summary += "; last_solver_diagnostic=" + diagnostics.summary;
  }
  return exhausted;
}

PrecisionObservation MakePrecisionObservationFromDiagnostics(const SolveRequest& request,
                                                            const SolverDiagnostics& diagnostics) {
  PrecisionObservation observation;
  observation.requested_digits = request.requested_digits;
  observation.stable_digits = 0;
  observation.residual_estimate = diagnostics.residual_norm;
  observation.overlap_mismatch = diagnostics.overlap_mismatch;
  observation.alternative_path_difference =
      std::max(diagnostics.residual_norm, diagnostics.overlap_mismatch);
  return observation;
}

bool IsRetryablePrecisionInstability(const SolverDiagnostics& diagnostics) {
  return !diagnostics.success && diagnostics.failure_code == kInsufficientPrecisionCode;
}

SolverDiagnostics SolveWithPrecisionRetry(const SeriesSolver& solver, SolveRequest request) {
  while (true) {
    const SolverDiagnostics diagnostics = solver.Solve(request);
    if (diagnostics.success || !IsRetryablePrecisionInstability(diagnostics)) {
      return diagnostics;
    }

    if (request.requested_digits <= request.precision_policy.working_precision) {
      return diagnostics;
    }

    const PrecisionDecision decision =
        EvaluatePrecision(request.precision_policy,
                          MakePrecisionObservationFromDiagnostics(request, diagnostics));
    if (decision.status == PrecisionStatus::Rejected) {
      return MakeRetryCeilingInsufficientPrecisionDiagnostics(diagnostics, decision.reason);
    }
    if (decision.status != PrecisionStatus::Escalate) {
      return MakeContinuationBudgetExhaustedDiagnostics(diagnostics, decision.reason);
    }
    if (decision.suggested_working_precision <= request.precision_policy.working_precision) {
      return MakeContinuationBudgetExhaustedDiagnostics(
          diagnostics,
          "precision retry controller could not advance working precision");
    }
    if (decision.suggested_x_order <= request.precision_policy.x_order) {
      return MakeContinuationBudgetExhaustedDiagnostics(
          diagnostics,
          "precision retry controller could not advance truncation order");
    }

    request.precision_policy.working_precision = decision.suggested_working_precision;
    request.precision_policy.x_order = decision.suggested_x_order;
  }
}

void PopulateSolveRequestExecutionInputs(
    SolveRequest& request,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    const std::optional<AmfSolveRuntimePolicy>& amf_runtime_policy,
    const std::optional<std::string>& amf_requested_d0,
    const std::optional<std::string>& amf_requested_dimension_expression,
    const int requested_digits) {
  request.start_location = start_location;
  request.target_location = target_location;
  request.precision_policy = precision_policy;
  request.amf_runtime_policy = amf_runtime_policy;
  request.amf_requested_d0 = amf_requested_d0;
  request.amf_requested_dimension_expression = amf_requested_dimension_expression;
  request.requested_digits = requested_digits;
}

SolverDiagnostics SolveEtaGeneratedSeriesWithSolvedPathCache(
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
    const std::optional<AmfSolveRuntimePolicy>& amf_runtime_policy,
    const std::optional<std::string>& amf_requested_d0,
    const std::optional<std::string>& amf_requested_dimension_expression,
    const int requested_digits,
    const std::string& eta_symbol,
    const bool skip_reduction,
    const SolvedPathCacheContext& cache_context) {
  SolvedPathCacheContext replay_context = cache_context;
  const std::optional<std::string> exact_dimension_override =
      ResolveExactDimensionOverride(amf_requested_dimension_expression);
  std::optional<SolveRequest> prepared_skip_reduction_request;
  if (skip_reduction) {
    SolveRequest request;
    request.system = AssembleWrapperEtaGeneratedDESystem(
        master_basis,
        LoadValidatedWrapperEtaGeneratedReductionState(
            spec, master_basis, decision, options, layout, exact_dimension_override, eta_symbol));
    PopulateSolveRequestExecutionInputs(request,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        amf_runtime_policy,
                                        amf_requested_d0,
                                        amf_requested_dimension_expression,
                                        requested_digits);
    replay_context.expected_request_fingerprint = ComputeSolveRequestFingerprint(request);
    prepared_skip_reduction_request = std::move(request);
  }

  const SolvedPathCacheReplayResult replay = TryReplaySolvedPathCache(layout, replay_context);
  if (replay.status == SolvedPathCacheReplayStatus::Hit) {
    return *replay.diagnostics;
  }

  SolveRequest request;
  try {
    if (prepared_skip_reduction_request.has_value()) {
      request = *prepared_skip_reduction_request;
    } else {
      request.system = BuildWrapperEtaGeneratedDESystem(spec,
                                                        master_basis,
                                                        decision,
                                                        options,
                                                        layout,
                                                        kira_executable,
                                                        fermat_executable,
                                                        exact_dimension_override,
                                                        eta_symbol,
                                                        false);
    }
  } catch (const MasterSetInstabilityError& error) {
    return MakeMasterSetInstabilityDiagnostics(error.what());
  }
  if (!prepared_skip_reduction_request.has_value()) {
    PopulateSolveRequestExecutionInputs(request,
                                        start_location,
                                        target_location,
                                        precision_policy,
                                        amf_runtime_policy,
                                        amf_requested_d0,
                                        amf_requested_dimension_expression,
                                        requested_digits);
  }

  const SolverDiagnostics diagnostics = SolveWithPrecisionRetry(solver, request);
  PersistSolvedPathCacheManifest(layout, cache_context, request, diagnostics);
  return diagnostics;
}

SolverDiagnostics MakeSuccessfulBootstrapSolveDiagnostics() {
  SolverDiagnostics diagnostics;
  diagnostics.success = true;
  diagnostics.residual_norm = 0.0;
  diagnostics.overlap_mismatch = 0.0;
  diagnostics.failure_code.clear();
  diagnostics.summary = "Solved by exact one-hop regular-point continuation.";
  return diagnostics;
}

SolverDiagnostics MakeSuccessfulMixedBootstrapSolveDiagnostics() {
  SolverDiagnostics diagnostics;
  diagnostics.success = true;
  diagnostics.residual_norm = 0.0;
  diagnostics.overlap_mismatch = 0.0;
  diagnostics.failure_code.clear();
  diagnostics.summary =
      "Solved by exact one-hop mixed regular/regular-singular continuation.";
  return diagnostics;
}

std::string MakeResolvedPointExpression(const std::string& variable_name,
                                        const ExactRational& value) {
  return variable_name + "=" + value.ToString();
}

ExactRational ComputeBootstrapMatchPoint(const ExactRational& start_value,
                                         const ExactRational& target_value) {
  return DivideRational(AddRational(start_value, target_value), IntegerRational(2));
}

ExactRational ComputeBootstrapCheckPoint(const ExactRational& start_value,
                                         const ExactRational& target_value) {
  return DivideRational(
      AddRational(MultiplyRational(IntegerRational(3), start_value), target_value),
      IntegerRational(4));
}

const BoundaryCondition& ResolveStartBoundaryCondition(const SolveRequest& request,
                                                       const std::string& variable_name) {
  if (request.boundary_requests.empty() && request.boundary_conditions.empty()) {
    throw BoundaryUnsolvedError("explicit start boundary attachment is required at " +
                                variable_name + " @ " + request.start_location);
  }

  ValidateManualBoundaryAttachment(request.system,
                                   request.boundary_requests,
                                   request.boundary_conditions,
                                   request.start_location);

  const auto condition_it =
      std::find_if(request.boundary_conditions.begin(),
                   request.boundary_conditions.end(),
                   [&variable_name, &request](const BoundaryCondition& condition) {
                     return condition.variable == variable_name &&
                            condition.location == request.start_location;
                   });
  if (condition_it == request.boundary_conditions.end()) {
    throw BoundaryUnsolvedError("no explicit boundary data matched the solver start location: " +
                                request.start_location);
  }
  return *condition_it;
}

ExactRationalVector ParseBoundaryValuesExactly(const BoundaryCondition& condition,
                                               const NumericEvaluationPoint& passive_bindings) {
  ExactRationalVector values;
  values.reserve(condition.values.size());
  for (const std::string& value : condition.values) {
    values.push_back(EvaluateCoefficientExpression(value, passive_bindings));
  }
  return values;
}

bool IsDivisionByZeroError(const std::runtime_error& error) {
  return std::string(error.what()).find("division by zero") != std::string::npos;
}

bool IsRegularCenterRejection(const std::invalid_argument& error) {
  const std::string message = error.what();
  return message.find("requires a regular center") != std::string::npos &&
         message.find(" is singular") != std::string::npos;
}

SolverDiagnostics SolveExactMixedRegularToSingularPath(
    const SolveRequest& request,
    const std::string& variable_name,
    const ExactRational& start_value,
    const ExactRational& target_value,
    const ExactRational& match_value,
    const ExactRational& check_value,
    const UpperTriangularMatrixSeriesPatch& start_patch,
    const ExactRationalVector& start_boundary_values,
    const NumericEvaluationPoint& passive_bindings) {
  const std::string target_expression = MakeResolvedPointExpression(variable_name, target_value);
  const std::string check_expression = MakeResolvedPointExpression(variable_name, check_value);
  const UpperTriangularMatrixFrobeniusSeriesPatch target_patch =
      GenerateUpperTriangularMatrixFrobeniusSeriesPatch(request.system,
                                                       variable_name,
                                                       target_expression,
                                                       kBootstrapContinuationOrder,
                                                       passive_bindings);
  const NormalizedUpperTriangularMatrixFrobeniusPatchData normalized_target_patch =
      NormalizeMatrixFrobeniusPatchCoefficients(target_patch, kBootstrapSolverPrefix);
  const std::vector<ExactRationalMatrix> start_coefficients =
      NormalizeMatrixPatchCoefficients(start_patch, passive_bindings, kBootstrapSolverPrefix);
  const ExactRationalMatrix start_match =
      EvaluateMatrixPolynomial(start_coefficients, start_value, match_value);
  const ExactRationalMatrix start_check =
      EvaluateMatrixPolynomial(start_coefficients, start_value, check_value);
  const ExactRationalMatrix target_match =
      EvaluateMatrixFrobeniusFundamentalMatrix(normalized_target_patch, target_value, match_value);
  const ExactRationalMatrix target_check =
      EvaluateMatrixFrobeniusFundamentalMatrix(normalized_target_patch, target_value, check_value);
  const ExactRationalMatrix handoff_matrix =
      MultiplyMatrices(InvertUpperTriangularMatrix(start_match), target_match);
  const ExactRationalMatrix mismatch =
      SubtractMatrices(target_check, MultiplyMatrices(start_check, handoff_matrix));
  const ExactRationalMatrix start_residual =
      EvaluateUpperTriangularMatrixSeriesPatchResidual(request.system,
                                                       variable_name,
                                                       start_patch,
                                                       check_expression,
                                                       passive_bindings);
  const ExactRationalMatrix target_residual =
      EvaluateUpperTriangularMatrixFrobeniusPatchResidual(request.system,
                                                          variable_name,
                                                          target_value,
                                                          check_value,
                                                          normalized_target_patch,
                                                          passive_bindings);

  if (!IsZeroMatrix(start_residual) || !IsZeroMatrix(target_residual) ||
      !IsZeroMatrix(mismatch)) {
    return MakeUnsupportedSolverPathDiagnostics(
        "exact mixed regular/regular-singular continuation checks were nonzero");
  }

  const ExactRationalVector regular_match_values =
      MultiplyMatrixVector(start_match, start_boundary_values);
  static_cast<void>(regular_match_values);
  return MakeSuccessfulMixedBootstrapSolveDiagnostics();
}

}  // namespace

SolverDiagnostics BootstrapSeriesSolver::Solve(const SolveRequest& request) const {
  if (request.system.variables.size() != 1) {
    return MakeUnsupportedSolverPathDiagnostics(
        std::string(kBootstrapSolverPrefix) +
        " supports exactly one declared system variable");
  }

  const std::vector<std::string> validation_messages = ValidateDESystem(request.system);
  if (!validation_messages.empty()) {
    return MakeUnsupportedSolverPathDiagnostics(validation_messages.front());
  }

  const std::string& variable_name = request.system.variables.front().name;
  const NumericEvaluationPoint passive_bindings = BuildBootstrapPassiveBindings(request);
  const ExactRational start_value =
      ParsePointValue(variable_name,
                      request.start_location,
                      passive_bindings,
                      kBootstrapSolverPrefix);
  const ExactRational target_value =
      ParsePointValue(variable_name,
                      request.target_location,
                      passive_bindings,
                      kBootstrapSolverPrefix);

  const BoundaryCondition* start_boundary = nullptr;
  try {
    start_boundary = &ResolveStartBoundaryCondition(request, variable_name);
  } catch (const BoundaryUnsolvedError& error) {
    return MakeBoundaryUnsolvedDiagnostics(error);
  }

  const ExactRationalVector start_boundary_values =
      ParseBoundaryValuesExactly(*start_boundary, passive_bindings);
  const PrecisionDecision precision_budget =
      EvaluatePrecisionBudget(request.precision_policy, request.requested_digits);
  if (precision_budget.status == PrecisionStatus::Rejected) {
    return MakeInsufficientPrecisionDiagnostics(precision_budget);
  }

  const std::string start_expression = MakeResolvedPointExpression(variable_name, start_value);
  const std::string target_expression = MakeResolvedPointExpression(variable_name, target_value);
  const ExactRational match_value = ComputeBootstrapMatchPoint(start_value, target_value);
  const ExactRational check_value = ComputeBootstrapCheckPoint(start_value, target_value);
  const std::string match_expression = MakeResolvedPointExpression(variable_name, match_value);
  const std::string check_expression = MakeResolvedPointExpression(variable_name, check_value);

  try {
    const UpperTriangularMatrixSeriesPatch start_patch =
        GenerateUpperTriangularRegularPointSeriesPatch(request.system,
                                                      variable_name,
                                                      start_expression,
                                                      kBootstrapContinuationOrder,
                                                      passive_bindings);
    try {
      const UpperTriangularMatrixSeriesPatch target_patch =
          GenerateUpperTriangularRegularPointSeriesPatch(request.system,
                                                        variable_name,
                                                        target_expression,
                                                        kBootstrapContinuationOrder,
                                                        passive_bindings);
      const UpperTriangularMatrixSeriesPatchOverlapDiagnostics overlap =
          MatchUpperTriangularMatrixSeriesPatches(variable_name,
                                                  start_patch,
                                                  target_patch,
                                                  match_expression,
                                                  check_expression,
                                                  passive_bindings);
      const ExactRationalMatrix start_residual =
          EvaluateUpperTriangularMatrixSeriesPatchResidual(request.system,
                                                          variable_name,
                                                          start_patch,
                                                          check_expression,
                                                          passive_bindings);
      const ExactRationalMatrix target_residual =
          EvaluateUpperTriangularMatrixSeriesPatchResidual(request.system,
                                                          variable_name,
                                                          target_patch,
                                                          check_expression,
                                                          passive_bindings);

      if (!IsZeroMatrix(start_residual) || !IsZeroMatrix(target_residual) ||
          !IsZeroMatrix(overlap.mismatch)) {
        return MakeUnsupportedSolverPathDiagnostics(
            "exact one-hop regular-point continuation checks were nonzero");
      }

      const std::vector<ExactRationalMatrix> start_coefficients =
          NormalizeMatrixPatchCoefficients(start_patch, passive_bindings, kBootstrapSolverPrefix);
      const ExactRationalMatrix transported_fundamental_matrix =
          EvaluateMatrixPolynomial(start_coefficients, start_value, target_value);
      const ExactRationalVector transported_target_values =
          MultiplyMatrixVector(transported_fundamental_matrix, start_boundary_values);
      static_cast<void>(transported_target_values);

      return MakeSuccessfulBootstrapSolveDiagnostics();
    } catch (const std::invalid_argument& error) {
      if (!IsRegularCenterRejection(error)) {
        throw;
      }
    }

    return SolveExactMixedRegularToSingularPath(request,
                                                variable_name,
                                                start_value,
                                                target_value,
                                                match_value,
                                                check_value,
                                                start_patch,
                                                start_boundary_values,
                                                passive_bindings);
  } catch (const std::invalid_argument& error) {
    if (IsRegularCenterRejection(error)) {
      return MakeUnsupportedSolverPathDiagnostics(
          "bootstrap regular-point continuation solver currently requires a regular start "
          "location");
    }
    return MakeUnsupportedSolverPathDiagnostics(error.what());
  } catch (const std::runtime_error& error) {
    if (IsDivisionByZeroError(error)) {
      return MakeUnsupportedSolverPathDiagnostics(error.what());
    }
    throw;
  }
}

SeriesPatch GenerateScalarRegularPointSeriesPatch(const DESystem& system,
                                                  const std::string& variable_name,
                                                  const std::string& center_expression,
                                                  const int order,
                                                  const NumericEvaluationPoint& passive_bindings) {
  if (order < 0) {
    throw std::invalid_argument(std::string(kScalarPatchPrefix) +
                                " requires a non-negative order");
  }

  if (system.masters.size() != 1) {
    throw std::invalid_argument(std::string(kScalarPatchPrefix) +
                                " requires exactly one master");
  }

  const auto& matrix = ResolveSelectedMatrix(system, variable_name, kScalarPatchPrefix);
  if (matrix.size() != 1 || matrix.front().size() != 1) {
    throw std::invalid_argument(std::string(kScalarPatchPrefix) +
                                " encountered unsupported coefficient shape: selected "
                                "coefficient matrix must be 1x1");
  }

  const ExactRational center_value =
      ParseCenterValue(variable_name, center_expression, passive_bindings, kScalarPatchPrefix);
  const std::string resolved_center_expression = variable_name + "=" + center_value.ToString();
  if (ClassifyRegularCenterWithFallback(system,
                                        variable_name,
                                        resolved_center_expression,
                                        center_value,
                                        passive_bindings,
                                        kScalarPatchPrefix) !=
      PointClassification::Regular) {
    throw std::invalid_argument(std::string(kScalarPatchPrefix) +
                                " requires a regular center but \"" +
                                resolved_center_expression + "\" is singular");
  }

  const ExactSeries coefficient_series =
      SeriesExpressionParser(matrix.front().front(),
                             variable_name,
                             center_value,
                             ResolvePassiveBindingsExactly(passive_bindings),
                             order,
                             kScalarPatchPrefix)
          .Parse();
  const std::vector<ExactRational> coefficients =
      ComputeScalarPatchCoefficients(coefficient_series, order);
  VerifyScalarResidual(coefficient_series, coefficients);

  SeriesPatch patch;
  patch.center = resolved_center_expression;
  patch.order = order;
  patch.basis_functions = BuildBasisFunctions(variable_name, center_value, order);
  patch.coefficients.reserve(coefficients.size());
  for (const ExactRational& coefficient : coefficients) {
    patch.coefficients.push_back(coefficient.ToString());
  }
  return patch;
}

ScalarFrobeniusSeriesPatch GenerateScalarFrobeniusSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    const int order,
    const NumericEvaluationPoint& passive_bindings) {
  if (order < 0) {
    throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                " requires a non-negative order");
  }

  if (system.masters.size() != 1) {
    throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                " requires exactly one master");
  }

  const auto& matrix =
      ResolveSelectedMatrix(system, variable_name, kScalarFrobeniusPatchPrefix);
  if (matrix.size() != 1 || matrix.front().size() != 1) {
    throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                " encountered unsupported coefficient shape: selected "
                                "coefficient matrix must be 1x1");
  }

  const ExactRational center_value = ParseCenterValue(variable_name,
                                                      center_expression,
                                                      passive_bindings,
                                                      kScalarFrobeniusPatchPrefix);
  const std::string resolved_center_expression = variable_name + "=" + center_value.ToString();
  const ExactPassiveBindings exact_passive_bindings =
      ResolvePassiveBindingsExactly(passive_bindings);
  const LaurentSeries coefficient_series =
      LaurentSeriesExpressionParser(matrix.front().front(),
                                    variable_name,
                                    center_value,
                                    exact_passive_bindings,
                                    std::max(order, 1),
                                    kScalarFrobeniusPatchPrefix)
          .Parse();
  const std::optional<int> leading_order = LeadingLaurentOrder(coefficient_series);

  PointClassification classification = PointClassification::Regular;
  try {
    classification = ClassifyFinitePoint(system,
                                         variable_name,
                                         resolved_center_expression,
                                         passive_bindings);
  } catch (const std::invalid_argument& error) {
    if (!IsUnsupportedSingularFormError(error)) {
      throw;
    }
    if (leading_order.has_value() && *leading_order < -1) {
      throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                  " requires a simple-pole regular-singular center but \"" +
                                  resolved_center_expression + "\" has higher-order pole");
    }
    if (ContainsParenthesizedDirectDifference(matrix.front().front(), variable_name)) {
      throw;
    }
    classification = PointClassification::Singular;
  }

  if (classification != PointClassification::Singular) {
    throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                " requires a singular center but \"" +
                                resolved_center_expression + "\" is regular");
  }

  if (!leading_order.has_value() || *leading_order >= 0) {
    throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                " requires a singular center but \"" +
                                resolved_center_expression + "\" is regular");
  }
  if (*leading_order < -1) {
    throw std::invalid_argument(std::string(kScalarFrobeniusPatchPrefix) +
                                " requires a simple-pole regular-singular center but \"" +
                                resolved_center_expression + "\" has higher-order pole");
  }

  const ExactRational indicial_exponent = LaurentCoefficient(coefficient_series, -1);
  const ExactSeries reduced_series = BuildFrobeniusReducedSeries(coefficient_series, order);
  const std::vector<ExactRational> coefficients =
      ComputeScalarPatchCoefficients(reduced_series, order);
  VerifyScalarResidual(reduced_series, coefficients);

  ScalarFrobeniusSeriesPatch patch;
  patch.center = resolved_center_expression;
  patch.indicial_exponent = indicial_exponent.ToString();
  patch.order = order;
  patch.basis_functions = BuildBasisFunctions(variable_name, center_value, order);
  patch.coefficients.reserve(coefficients.size());
  for (const ExactRational& coefficient : coefficients) {
    patch.coefficients.push_back(coefficient.ToString());
  }
  return patch;
}

UpperTriangularMatrixSeriesPatch GenerateUpperTriangularRegularPointSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    const int order,
    const NumericEvaluationPoint& passive_bindings) {
  if (order < 0) {
    throw std::invalid_argument(std::string(kMatrixPatchPrefix) +
                                " requires a non-negative order");
  }

  const auto& matrix = ResolveSelectedMatrix(system, variable_name, kMatrixPatchPrefix);
  const std::size_t dimension = system.masters.size();
  if (matrix.size() != dimension) {
    throw std::invalid_argument(std::string(kMatrixPatchPrefix) +
                                " requires the selected coefficient matrix to be square "
                                "and dimension-matched to masters.size()");
  }
  for (const auto& row : matrix) {
    if (row.size() != dimension) {
      throw std::invalid_argument(std::string(kMatrixPatchPrefix) +
                                  " requires the selected coefficient matrix to be square "
                                  "and dimension-matched to masters.size()");
    }
  }

  const ExactRational center_value =
      ParseCenterValue(variable_name, center_expression, passive_bindings, kMatrixPatchPrefix);
  const std::string resolved_center_expression = variable_name + "=" + center_value.ToString();
  if (ClassifyRegularCenterWithFallback(system,
                                        variable_name,
                                        resolved_center_expression,
                                        center_value,
                                        passive_bindings,
                                        kMatrixPatchPrefix) !=
      PointClassification::Regular) {
    throw std::invalid_argument(std::string(kMatrixPatchPrefix) +
                                " requires a regular center but \"" +
                                resolved_center_expression + "\" is singular");
  }

  const ExactPassiveBindings exact_passive_bindings =
      ResolvePassiveBindingsExactly(passive_bindings);
  const ExactSeriesMatrix local_series =
      ExpandMatrixSeriesLocally(matrix,
                                variable_name,
                                center_value,
                                exact_passive_bindings,
                                order,
                                kMatrixPatchPrefix);
  const std::vector<ExactRationalMatrix> degree_matrices =
      BuildDegreeMatrices(local_series, order);
  RequireUpperTriangularLocalSupport(degree_matrices, order, kMatrixPatchPrefix);

  UpperTriangularMatrixSeriesPatch patch;
  patch.center = resolved_center_expression;
  patch.order = order;
  patch.basis_functions = BuildBasisFunctions(variable_name, center_value, order);
  patch.coefficient_matrices = ComputeMatrixPatchCoefficients(degree_matrices);
  VerifyMatrixResidual(degree_matrices, patch.coefficient_matrices);
  return patch;
}

UpperTriangularMatrixFrobeniusSeriesPatch
GenerateUpperTriangularMatrixFrobeniusSeriesPatch(
    const DESystem& system,
    const std::string& variable_name,
    const std::string& center_expression,
    const int order,
    const NumericEvaluationPoint& passive_bindings) {
  if (order < 0) {
    throw std::invalid_argument(std::string(kMatrixFrobeniusPatchPrefix) +
                                " requires a non-negative order");
  }

  const auto& matrix =
      ResolveSelectedMatrix(system, variable_name, kMatrixFrobeniusPatchPrefix);
  const std::size_t dimension = system.masters.size();
  if (matrix.size() != dimension) {
    throw std::invalid_argument(std::string(kMatrixFrobeniusPatchPrefix) +
                                " requires the selected coefficient matrix to be square "
                                "and dimension-matched to masters.size()");
  }
  for (const auto& row : matrix) {
    if (row.size() != dimension) {
      throw std::invalid_argument(std::string(kMatrixFrobeniusPatchPrefix) +
                                  " requires the selected coefficient matrix to be square "
                                  "and dimension-matched to masters.size()");
    }
  }

  const ExactRational center_value = ParseCenterValue(variable_name,
                                                      center_expression,
                                                      passive_bindings,
                                                      kMatrixFrobeniusPatchPrefix);
  const std::string resolved_center_expression = variable_name + "=" + center_value.ToString();
  const ExactPassiveBindings exact_passive_bindings =
      ResolvePassiveBindingsExactly(passive_bindings);
  const LaurentSeriesMatrix local_series =
      ExpandMatrixLaurentSeriesLocally(matrix,
                                       variable_name,
                                       center_value,
                                       exact_passive_bindings,
                                       std::max(order, 1),
                                       kMatrixFrobeniusPatchPrefix);

  PointClassification classification = PointClassification::Regular;
  try {
    classification = ClassifyFinitePoint(system,
                                         variable_name,
                                         resolved_center_expression,
                                         passive_bindings);
  } catch (const std::invalid_argument& error) {
    if (!IsUnsupportedSingularFormError(error)) {
      throw;
    }
    if (HasHigherOrderPole(local_series)) {
      throw std::invalid_argument(std::string(kMatrixFrobeniusPatchPrefix) +
                                  " requires a simple-pole regular-singular center but \"" +
                                  resolved_center_expression + "\" has higher-order pole");
    }
    if (MatrixContainsParenthesizedDirectDifference(matrix, variable_name)) {
      throw;
    }
    classification = PointClassification::Singular;
  }

  if (classification != PointClassification::Singular || !HasNegativeLaurentSupport(local_series)) {
    throw std::invalid_argument(std::string(kMatrixFrobeniusPatchPrefix) +
                                " requires a singular center but \"" +
                                resolved_center_expression + "\" is regular");
  }
  if (HasHigherOrderPole(local_series)) {
    throw std::invalid_argument(std::string(kMatrixFrobeniusPatchPrefix) +
                                " requires a simple-pole regular-singular center but \"" +
                                resolved_center_expression + "\" has higher-order pole");
  }

  const ExactRationalMatrix residue_matrix =
      BuildLaurentDegreeMatrices(local_series, -1, -1).front();
  RequireDiagonalResidueMatrix(residue_matrix, kMatrixFrobeniusPatchPrefix);
  const std::vector<ExactRational> indicial_exponents =
      ExtractDiagonalEntries(residue_matrix);
  const std::vector<ExactRationalMatrix> regular_tail_degree_matrices =
      BuildLaurentDegreeMatrices(local_series, 0, order);
  RequireUpperTriangularLocalSupport(
      regular_tail_degree_matrices, order, kMatrixFrobeniusPatchPrefix);

  UpperTriangularMatrixFrobeniusSeriesPatch patch;
  patch.center = resolved_center_expression;
  patch.order = order;
  patch.basis_functions = BuildBasisFunctions(variable_name, center_value, order);
  patch.coefficient_matrices = ComputeMatrixFrobeniusPatchCoefficients(
      regular_tail_degree_matrices, indicial_exponents, kMatrixFrobeniusPatchPrefix);
  VerifyMatrixFrobeniusReducedResidual(regular_tail_degree_matrices,
                                       indicial_exponents,
                                       patch.coefficient_matrices,
                                       kMatrixFrobeniusPatchPrefix);
  patch.indicial_exponents.reserve(indicial_exponents.size());
  for (const ExactRational& exponent : indicial_exponents) {
    patch.indicial_exponents.push_back(exponent.ToString());
  }
  return patch;
}

ExactRational EvaluateScalarSeriesPatchResidual(
    const DESystem& system,
    const std::string& variable_name,
    const SeriesPatch& patch,
    const std::string& point_expression,
    const NumericEvaluationPoint& passive_bindings) {
  if (system.masters.size() != 1) {
    throw std::invalid_argument(std::string(kScalarResidualPrefix) +
                                " requires exactly one master");
  }

  const auto& matrix = ResolveSelectedMatrix(system, variable_name, kScalarResidualPrefix);
  if (matrix.size() != 1 || matrix.front().size() != 1) {
    throw std::invalid_argument(std::string(kScalarResidualPrefix) +
                                " encountered unsupported coefficient shape: selected "
                                "coefficient matrix must be 1x1");
  }

  const ExactRational center_value =
      ParseSeriesPatchCenterValue(
          variable_name, patch.center, passive_bindings, kScalarResidualPrefix);
  const std::vector<ExactRational> coefficients =
      NormalizeScalarCoefficients(patch, passive_bindings, kScalarResidualPrefix);
  const ExactRational point_value =
      ParsePointValue(variable_name, point_expression, passive_bindings, kScalarResidualPrefix);

  NumericEvaluationPoint evaluation_point = passive_bindings;
  evaluation_point[variable_name] = point_value.ToString();
  const ExactRational coefficient =
      EvaluateCoefficientMatrix(system, variable_name, evaluation_point).front().front();
  const ExactRational patch_value =
      EvaluateScalarPolynomial(coefficients, center_value, point_value);
  const ExactRational patch_derivative =
      EvaluateScalarPolynomialDerivative(coefficients, center_value, point_value);
  return SubtractRational(patch_derivative,
                          MultiplyRational(coefficient, patch_value));
}

ScalarSeriesPatchOverlapDiagnostics MatchScalarSeriesPatches(
    const std::string& variable_name,
    const SeriesPatch& left_patch,
    const SeriesPatch& right_patch,
    const std::string& match_point_expression,
    const std::string& check_point_expression,
    const NumericEvaluationPoint& passive_bindings) {
  const ExactRational left_center =
      ParseSeriesPatchCenterValue(
          variable_name, left_patch.center, passive_bindings, kScalarOverlapPrefix);
  const ExactRational right_center =
      ParseSeriesPatchCenterValue(
          variable_name, right_patch.center, passive_bindings, kScalarOverlapPrefix);
  const std::vector<ExactRational> left_coefficients =
      NormalizeScalarCoefficients(left_patch, passive_bindings, kScalarOverlapPrefix);
  const std::vector<ExactRational> right_coefficients =
      NormalizeScalarCoefficients(right_patch, passive_bindings, kScalarOverlapPrefix);
  const ExactRational match_point =
      ParsePointValue(variable_name,
                      match_point_expression,
                      passive_bindings,
                      kScalarOverlapPrefix);
  const ExactRational check_point =
      ParsePointValue(variable_name,
                      check_point_expression,
                      passive_bindings,
                      kScalarOverlapPrefix);
  RequireDistinctPoints(match_point, check_point, kScalarOverlapPrefix);

  const ExactRational left_match =
      EvaluateScalarPolynomial(left_coefficients, left_center, match_point);
  const ExactRational right_match =
      EvaluateScalarPolynomial(right_coefficients, right_center, match_point);
  const ExactRational left_check =
      EvaluateScalarPolynomial(left_coefficients, left_center, check_point);
  const ExactRational right_check =
      EvaluateScalarPolynomial(right_coefficients, right_center, check_point);

  ScalarSeriesPatchOverlapDiagnostics diagnostics;
  diagnostics.lambda = DivideRational(right_match, left_match);
  diagnostics.mismatch =
      SubtractRational(right_check,
                       MultiplyRational(diagnostics.lambda, left_check));
  return diagnostics;
}

ExactRationalMatrix EvaluateUpperTriangularMatrixSeriesPatchResidual(
    const DESystem& system,
    const std::string& variable_name,
    const UpperTriangularMatrixSeriesPatch& patch,
    const std::string& point_expression,
    const NumericEvaluationPoint& passive_bindings) {
  const auto& matrix = ResolveSelectedMatrix(system, variable_name, kMatrixResidualPrefix);
  const std::size_t dimension = system.masters.size();
  if (matrix.size() != dimension) {
    throw std::invalid_argument(std::string(kMatrixResidualPrefix) +
                                " requires the selected coefficient matrix to be square "
                                "and dimension-matched to masters.size()");
  }
  for (const auto& row : matrix) {
    if (row.size() != dimension) {
      throw std::invalid_argument(std::string(kMatrixResidualPrefix) +
                                  " requires the selected coefficient matrix to be square "
                                  "and dimension-matched to masters.size()");
    }
  }

  const ExactRational center_value =
      ParseSeriesPatchCenterValue(
          variable_name, patch.center, passive_bindings, kMatrixResidualPrefix);
  const std::vector<ExactRationalMatrix> coefficient_matrices =
      NormalizeMatrixPatchCoefficients(patch, passive_bindings, kMatrixResidualPrefix);
  if (coefficient_matrices.front().size() != dimension) {
    throw std::invalid_argument(std::string(kMatrixResidualPrefix) +
                                " requires matrix patch dimension to match masters.size()");
  }

  const ExactRational point_value =
      ParsePointValue(variable_name, point_expression, passive_bindings, kMatrixResidualPrefix);
  NumericEvaluationPoint evaluation_point = passive_bindings;
  evaluation_point[variable_name] = point_value.ToString();
  const ExactRationalMatrix coefficient_matrix =
      EvaluateCoefficientMatrix(system, variable_name, evaluation_point);
  const ExactRationalMatrix patch_value =
      EvaluateMatrixPolynomial(coefficient_matrices, center_value, point_value);
  const ExactRationalMatrix patch_derivative =
      EvaluateMatrixPolynomialDerivative(coefficient_matrices, center_value, point_value);
  return SubtractMatrices(patch_derivative, MultiplyMatrices(coefficient_matrix, patch_value));
}

UpperTriangularMatrixSeriesPatchOverlapDiagnostics MatchUpperTriangularMatrixSeriesPatches(
    const std::string& variable_name,
    const UpperTriangularMatrixSeriesPatch& left_patch,
    const UpperTriangularMatrixSeriesPatch& right_patch,
    const std::string& match_point_expression,
    const std::string& check_point_expression,
    const NumericEvaluationPoint& passive_bindings) {
  const ExactRational left_center =
      ParseSeriesPatchCenterValue(
          variable_name, left_patch.center, passive_bindings, kMatrixOverlapPrefix);
  const ExactRational right_center =
      ParseSeriesPatchCenterValue(
          variable_name, right_patch.center, passive_bindings, kMatrixOverlapPrefix);
  const std::vector<ExactRationalMatrix> left_coefficients =
      NormalizeMatrixPatchCoefficients(left_patch, passive_bindings, kMatrixOverlapPrefix);
  const std::vector<ExactRationalMatrix> right_coefficients =
      NormalizeMatrixPatchCoefficients(right_patch, passive_bindings, kMatrixOverlapPrefix);
  if (left_coefficients.front().size() != right_coefficients.front().size()) {
    throw std::invalid_argument(std::string(kMatrixOverlapPrefix) +
                                " requires matrix patches with matching dimensions");
  }

  const ExactRational match_point =
      ParsePointValue(variable_name,
                      match_point_expression,
                      passive_bindings,
                      kMatrixOverlapPrefix);
  const ExactRational check_point =
      ParsePointValue(variable_name,
                      check_point_expression,
                      passive_bindings,
                      kMatrixOverlapPrefix);
  RequireDistinctPoints(match_point, check_point, kMatrixOverlapPrefix);

  const ExactRationalMatrix left_match =
      EvaluateMatrixPolynomial(left_coefficients, left_center, match_point);
  const ExactRationalMatrix right_match =
      EvaluateMatrixPolynomial(right_coefficients, right_center, match_point);
  const ExactRationalMatrix left_check =
      EvaluateMatrixPolynomial(left_coefficients, left_center, check_point);
  const ExactRationalMatrix right_check =
      EvaluateMatrixPolynomial(right_coefficients, right_center, check_point);

  UpperTriangularMatrixSeriesPatchOverlapDiagnostics diagnostics;
  diagnostics.match_matrix =
      MultiplyMatrices(right_match, InvertUpperTriangularMatrix(left_match));
  diagnostics.mismatch =
      SubtractMatrices(right_check,
                       MultiplyMatrices(diagnostics.match_matrix, left_check));
  return diagnostics;
}

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
    const int requested_digits) {
  SolveRequest request;
  try {
    request.system = BuildInvariantGeneratedDESystem(spec,
                                                     master_basis,
                                                     invariant_name,
                                                     options,
                                                     layout,
                                                     kira_executable,
                                                     fermat_executable);
  } catch (const MasterSetInstabilityError& error) {
    return MakeMasterSetInstabilityDiagnostics(error.what());
  }
  request.start_location = start_location;
  request.target_location = target_location;
  request.precision_policy = precision_policy;
  request.requested_digits = requested_digits;
  return SolveWithPrecisionRetry(solver, std::move(request));
}

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
    const int requested_digits) {
  if (invariant_names.empty()) {
    throw std::runtime_error(
        "automatic invariant solver handoff list requires at least one invariant name");
  }

  SolveRequest request;
  try {
    request.system = BuildInvariantGeneratedDESystemList(spec,
                                                         master_basis,
                                                         invariant_names,
                                                         options,
                                                         layout,
                                                         kira_executable,
                                                         fermat_executable);
  } catch (const MasterSetInstabilityError& error) {
    return MakeMasterSetInstabilityDiagnostics(error.what());
  }
  request.start_location = start_location;
  request.target_location = target_location;
  request.precision_policy = precision_policy;
  request.requested_digits = requested_digits;
  return SolveWithPrecisionRetry(solver, std::move(request));
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  return SolveEtaGeneratedSeries(spec,
                                 master_basis,
                                 decision,
                                 options,
                                 layout,
                                 kira_executable,
                                 fermat_executable,
                                 solver,
                                 start_location,
                                 target_location,
                                 precision_policy,
                                 requested_digits,
                                 eta_symbol,
                                 std::nullopt);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const std::optional<std::string> normalized_dimension_expression =
      NormalizePublicDimensionExpression(exact_dimension_override);
  const std::optional<std::string> normalized_exact_dimension_override =
      ResolveExactDimensionOverride(normalized_dimension_expression);
  SolveRequest request;
  try {
    request.system = BuildEtaGeneratedDESystem(spec,
                                               master_basis,
                                               decision,
                                               options,
                                               layout,
                                               kira_executable,
                                               fermat_executable,
                                               eta_symbol,
                                               normalized_exact_dimension_override);
  } catch (const MasterSetInstabilityError& error) {
    return MakeMasterSetInstabilityDiagnostics(error.what());
  }
  request.start_location = start_location;
  request.target_location = target_location;
  request.precision_policy = precision_policy;
  request.amf_requested_dimension_expression = normalized_dimension_expression;
  request.requested_digits = requested_digits;
  return SolveWithPrecisionRetry(solver, std::move(request));
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  return SolveEtaModePlannedSeries(spec,
                                   master_basis,
                                   eta_mode,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol,
                                   std::nullopt);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const EtaInsertionDecision decision = eta_mode.Plan(spec);
  return SolveEtaGeneratedSeries(spec,
                                 master_basis,
                                 decision,
                                 options,
                                 layout,
                                 kira_executable,
                                 fermat_executable,
                                 solver,
                                 start_location,
                                 target_location,
                                 precision_policy,
                                 requested_digits,
                                 eta_symbol,
                                 exact_dimension_override);
}

SolverDiagnostics SolvePlannedAmfOptionsEtaModeSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const AmfOptions& amf_options,
    const std::string& solve_kind,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    const int requested_digits,
    const std::string& eta_symbol) {
  return SolvePlannedAmfOptionsEtaModeSeries(spec,
                                             master_basis,
                                             decision,
                                             amf_options,
                                             solve_kind,
                                             options,
                                             layout,
                                             kira_executable,
                                             fermat_executable,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol,
                                             std::nullopt);
}

SolverDiagnostics SolvePlannedAmfOptionsEtaModeSeries(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const AmfOptions& amf_options,
    const std::string& solve_kind,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const SeriesSolver& solver,
    const std::string& start_location,
    const std::string& target_location,
    const PrecisionPolicy& precision_policy,
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const std::optional<std::string> normalized_explicit_dimension_expression =
      NormalizePublicDimensionExpression(exact_dimension_override);
  const PrecisionPolicy live_precision_policy =
      BuildAmfOptionsPrecisionPolicy(precision_policy, amf_options);
  const std::optional<AmfSolveRuntimePolicy> live_amf_runtime_policy =
      BuildAmfOptionsRuntimePolicy(amf_options);
  const std::optional<std::string> live_amf_requested_d0 = amf_options.d0;
  const std::optional<std::string> live_amf_fixed_eps = amf_options.fixed_eps;
  const std::optional<std::string> live_amf_requested_dimension_expression =
      normalized_explicit_dimension_expression.has_value()
          ? normalized_explicit_dimension_expression
          : BuildAmfRequestedDimensionExpression(live_amf_requested_d0, live_amf_fixed_eps);
  const std::optional<std::string> live_amf_fixed_eps_for_cache_identity =
      normalized_explicit_dimension_expression.has_value() ? std::nullopt : live_amf_fixed_eps;

  SolvedPathCacheContext cache_context;
  cache_context.replay_enabled = amf_options.use_cache;
  cache_context.solve_kind = solve_kind;
  cache_context.slot_name =
      MakeSolvedPathCacheSlotName(
          cache_context.solve_kind,
          spec,
          decision,
          eta_symbol,
          live_amf_requested_d0,
          live_amf_fixed_eps_for_cache_identity,
          normalized_explicit_dimension_expression);
  cache_context.input_fingerprint =
      BuildEtaGeneratedSolveInputFingerprint(cache_context.solve_kind,
                                            spec,
                                            master_basis,
                                            decision,
                                            options,
                                            solver,
                                            start_location,
                                            target_location,
                                            live_precision_policy,
                                            live_amf_runtime_policy,
                                            live_amf_requested_d0,
                                            live_amf_fixed_eps_for_cache_identity,
                                            normalized_explicit_dimension_expression,
                                            requested_digits,
                                            eta_symbol);
  return SolveEtaGeneratedSeriesWithSolvedPathCache(spec,
                                                    master_basis,
                                                    decision,
                                                    options,
                                                    layout,
                                                    kira_executable,
                                                    fermat_executable,
                                                    solver,
                                                    start_location,
                                                    target_location,
                                                    live_precision_policy,
                                                    live_amf_runtime_policy,
                                                    live_amf_requested_d0,
                                                    live_amf_requested_dimension_expression,
                                                    requested_digits,
                                                    eta_symbol,
                                                    amf_options.skip_reduction,
                                                    cache_context);
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  const std::shared_ptr<EtaMode> eta_mode = MakeBuiltinEtaMode(eta_mode_name);
  return SolveEtaModePlannedSeries(spec,
                                   master_basis,
                                   *eta_mode,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const std::shared_ptr<EtaMode> eta_mode = MakeBuiltinEtaMode(eta_mode_name);
  return SolveEtaModePlannedSeries(spec,
                                   master_basis,
                                   *eta_mode,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol,
                                   exact_dimension_override);
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  const std::string selected_eta_mode_name = SelectBuiltinEtaModeName(spec, eta_mode_names);
  return SolveBuiltinEtaModeSeries(spec,
                                   master_basis,
                                   selected_eta_mode_name,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const std::string selected_eta_mode_name = SelectBuiltinEtaModeName(spec, eta_mode_names);
  return SolveBuiltinEtaModeSeries(spec,
                                   master_basis,
                                   selected_eta_mode_name,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol,
                                   exact_dimension_override);
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  return SolveAmfOptionsEtaModeSeries(spec,
                                      master_basis,
                                      amf_options,
                                      options,
                                      layout,
                                      kira_executable,
                                      fermat_executable,
                                      solver,
                                      start_location,
                                      target_location,
                                      precision_policy,
                                      requested_digits,
                                      eta_symbol,
                                      std::nullopt);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const EtaInsertionDecision decision = PlanBuiltinAmfOptionsEtaMode(spec, amf_options);
  return SolvePlannedAmfOptionsEtaModeSeries(spec,
                                             master_basis,
                                             decision,
                                             amf_options,
                                             "amf-options-builtin-eta-mode-series",
                                             options,
                                             layout,
                                             kira_executable,
                                             fermat_executable,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol,
                                             exact_dimension_override);
}

SolverDiagnostics SolveAmfOptionsEndingSchemeEtaInfinitySeries(
    const ProblemSpec& spec,
    const AmfOptions& amf_options,
    const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes,
    const SolveRequest& request_template,
    const BoundaryProvider& provider,
    const SeriesSolver& solver,
    const std::string& eta_symbol) {
  const BoundaryRequest boundary_request =
      GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(spec,
                                                               amf_options,
                                                               user_defined_schemes,
                                                               eta_symbol);

  SolveRequest solve_request = request_template;
  solve_request.boundary_requests = {boundary_request};

  const SolveRequest attached_request =
      AttachBoundaryConditionsFromProvider(solve_request, provider);
  return solver.Solve(attached_request);
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  const std::shared_ptr<EtaMode> eta_mode = ResolveEtaMode(eta_mode_name, user_defined_modes);
  return SolveEtaModePlannedSeries(spec,
                                   master_basis,
                                   *eta_mode,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const std::shared_ptr<EtaMode> eta_mode = ResolveEtaMode(eta_mode_name, user_defined_modes);
  return SolveEtaModePlannedSeries(spec,
                                   master_basis,
                                   *eta_mode,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   solver,
                                   start_location,
                                   target_location,
                                   precision_policy,
                                   requested_digits,
                                   eta_symbol,
                                   exact_dimension_override);
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  const EtaInsertionDecision decision =
      SelectResolvedEtaModeDecision(spec, eta_mode_names, user_defined_modes);
  return SolveEtaGeneratedSeries(spec,
                                 master_basis,
                                 decision,
                                 options,
                                 layout,
                                 kira_executable,
                                 fermat_executable,
                                 solver,
                                 start_location,
                                 target_location,
                                 precision_policy,
                                 requested_digits,
                                 eta_symbol);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const EtaInsertionDecision decision =
      SelectResolvedEtaModeDecision(spec, eta_mode_names, user_defined_modes);
  return SolveEtaGeneratedSeries(spec,
                                 master_basis,
                                 decision,
                                 options,
                                 layout,
                                 kira_executable,
                                 fermat_executable,
                                 solver,
                                 start_location,
                                 target_location,
                                 precision_policy,
                                 requested_digits,
                                 eta_symbol,
                                 exact_dimension_override);
}

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
    const int requested_digits,
    const std::string& eta_symbol) {
  return SolveAmfOptionsEtaModeSeries(spec,
                                      master_basis,
                                      amf_options,
                                      user_defined_modes,
                                      options,
                                      layout,
                                      kira_executable,
                                      fermat_executable,
                                      solver,
                                      start_location,
                                      target_location,
                                      precision_policy,
                                      requested_digits,
                                      eta_symbol,
                                      std::nullopt);
}

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
    const int requested_digits,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const EtaInsertionDecision decision =
      PlanAmfOptionsEtaMode(spec, amf_options, user_defined_modes);
  return SolvePlannedAmfOptionsEtaModeSeries(spec,
                                             master_basis,
                                             decision,
                                             amf_options,
                                             "amf-options-resolved-eta-mode-series",
                                             options,
                                             layout,
                                             kira_executable,
                                             fermat_executable,
                                             solver,
                                             start_location,
                                             target_location,
                                             precision_policy,
                                             requested_digits,
                                             eta_symbol,
                                             exact_dimension_override);
}

std::unique_ptr<SeriesSolver> MakeBootstrapSeriesSolver() {
  return std::make_unique<BootstrapSeriesSolver>();
}

SolverDiagnostics SolveDifferentialEquation(const SolveRequest& request) {
  const std::unique_ptr<SeriesSolver> solver = MakeBootstrapSeriesSolver();
  return solver->Solve(request);
}

}  // namespace amflow
