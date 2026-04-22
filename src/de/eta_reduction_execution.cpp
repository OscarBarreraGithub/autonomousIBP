#include "amflow/de/eta_reduction_execution.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>

#include "amflow/solver/coefficient_evaluator.hpp"

namespace amflow {

namespace {

std::string StatusToString(const CommandExecutionStatus status) {
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

std::string VariableContext(const GeneratedDerivativeVariable& generated_variable,
                            const std::size_t index) {
  if (!generated_variable.variable.name.empty()) {
    return "variable \"" + generated_variable.variable.name + "\"";
  }
  return "variable[" + std::to_string(index) + "]";
}

std::string RemoveWhitespace(const std::string& value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (const char character : value) {
    if (!std::isspace(static_cast<unsigned char>(character))) {
      normalized.push_back(character);
    }
  }
  return normalized;
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

std::optional<std::string> NormalizeDimensionExpression(
    const std::optional<std::string>& dimension_expression) {
  if (!dimension_expression.has_value()) {
    return std::nullopt;
  }

  const std::string normalized_expression = RemoveWhitespace(*dimension_expression);
  if (normalized_expression.empty()) {
    throw std::invalid_argument("eta-generated dimension expression must not be empty");
  }
  DimensionExpressionSyntaxValidator(*dimension_expression).Validate();

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

std::optional<std::string> ResolveExactDimensionOverride(
    const std::optional<std::string>& exact_dimension_override) {
  if (!exact_dimension_override.has_value()) {
    return std::nullopt;
  }

  const std::string normalized_expression = RemoveWhitespace(*exact_dimension_override);
  if (normalized_expression.empty()) {
    throw std::invalid_argument("eta-generated dimension expression must not be empty");
  }
  DimensionExpressionSyntaxValidator(*exact_dimension_override).Validate();

  try {
    return EvaluateCoefficientExpression(normalized_expression, NumericEvaluationPoint{})
        .ToString();
  } catch (const std::invalid_argument& error) {
    if (!IsMissingNumericBindingError(error)) {
      throw;
    }
    return std::nullopt;
  }
}

std::string SubstituteDimensionIdentifier(const std::string& expression,
                                          const std::string& dimension_expression) {
  std::string rewritten;
  rewritten.reserve(expression.size() + dimension_expression.size());

  for (std::size_t index = 0; index < expression.size();) {
    if (!IsDimensionIdentifierStart(expression[index])) {
      rewritten.push_back(expression[index]);
      ++index;
      continue;
    }

    std::size_t end = index + 1;
    while (end < expression.size() && IsDimensionIdentifierContinuation(expression[end])) {
      ++end;
    }

    if (expression.compare(index, end - index, "dimension") == 0 &&
        (end == expression.size() || (expression[end] != '(' && expression[end] != '['))) {
      rewritten += "(" + dimension_expression + ")";
    } else {
      rewritten.append(expression, index, end - index);
    }
    index = end;
  }

  return rewritten;
}

void ApplySymbolicDimensionExpression(DESystem& system, const std::string& dimension_expression) {
  for (auto& [variable_name, matrix] : system.coefficient_matrices) {
    static_cast<void>(variable_name);
    for (auto& row : matrix) {
      for (std::string& cell : row) {
        cell = SubstituteDimensionIdentifier(cell, dimension_expression);
      }
    }
  }
  for (std::string& point : system.singular_points) {
    point = SubstituteDimensionIdentifier(point, dimension_expression);
  }
}

void ApplyExactDimensionOverride(BackendPreparation& preparation,
                                 const ArtifactLayout& layout,
                                 const std::optional<std::string>& exact_dimension_override) {
  const std::optional<std::string> normalized_override =
      ResolveExactDimensionOverride(exact_dimension_override);
  preparation.command_arguments.clear();
  if (normalized_override.has_value()) {
    preparation.command_arguments.push_back("-sd=" + *normalized_override);
  }

  std::ostringstream command;
  command << "FERMATPATH=<fermat-executable> kira";
  for (const std::string& argument : preparation.command_arguments) {
    command << " " << argument;
  }
  command << " " << (layout.generated_config_dir / "jobs.yaml").string();
  preparation.commands = {command.str()};
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

}  // namespace

DESystem BuildEtaGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol) {
  return BuildEtaGeneratedDESystem(spec,
                                   master_basis,
                                   decision,
                                   options,
                                   layout,
                                   kira_executable,
                                   fermat_executable,
                                   eta_symbol,
                                   std::nullopt);
}

DESystem BuildEtaGeneratedDESystem(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  EtaGeneratedReductionExecution execution =
      RunEtaGeneratedReduction(spec,
                               master_basis,
                               decision,
                               options,
                               layout,
                               kira_executable,
                               fermat_executable,
                               eta_symbol,
                               exact_dimension_override);
  if (!execution.execution_result.Succeeded()) {
    std::ostringstream message;
    message << "eta-generated DE construction requires successful reducer execution; "
            << "status=" << StatusToString(execution.execution_result.status)
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

EtaGeneratedReductionExecution RunEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol) {
  return RunEtaGeneratedReduction(spec,
                                  master_basis,
                                  decision,
                                  options,
                                  layout,
                                  kira_executable,
                                  fermat_executable,
                                  eta_symbol,
                                  std::nullopt);
}

EtaGeneratedReductionExecution RunEtaGeneratedReduction(
    const ProblemSpec& spec,
    const ParsedMasterList& master_basis,
    const EtaInsertionDecision& decision,
    const ReductionOptions& options,
    const ArtifactLayout& layout,
    const std::filesystem::path& kira_executable,
    const std::filesystem::path& fermat_executable,
    const std::string& eta_symbol,
    const std::optional<std::string>& exact_dimension_override) {
  const std::optional<std::string> normalized_dimension_expression =
      NormalizeDimensionExpression(exact_dimension_override);
  const std::optional<std::string> normalized_exact_dimension_override =
      ResolveExactDimensionOverride(normalized_dimension_expression);

  EtaGeneratedReductionExecution execution;
  execution.preparation =
      PrepareEtaGeneratedReduction(spec, master_basis, decision, options, layout, eta_symbol);
  ApplyExactDimensionOverride(
      execution.preparation.backend_preparation, layout, normalized_exact_dimension_override);

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
  if (execution.assembled_system.has_value() && normalized_dimension_expression.has_value() &&
      !normalized_exact_dimension_override.has_value()) {
    ApplySymbolicDimensionExpression(*execution.assembled_system,
                                     *normalized_dimension_expression);
  }
  return execution;
}

}  // namespace amflow
