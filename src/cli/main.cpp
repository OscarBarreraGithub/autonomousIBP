#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include "amflow/core/options.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/io/problem_spec_io.hpp"
#include "amflow/io/sample_data.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/kira/target_reduction.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/ending_scheme.hpp"
#include "amflow/runtime/eta_mode.hpp"
#include "amflow/solver/series_solver.hpp"

namespace {

std::vector<std::string> LoadedSpecValidationMessages(const amflow::ProblemSpec& spec) {
  return amflow::ValidateLoadedProblemSpec(spec);
}

void PrintMessages(std::ostream& stream, const std::vector<std::string>& messages) {
  for (const auto& message : messages) {
    stream << message << "\n";
  }
}

std::filesystem::path DefaultArtifactRootForSpec(const std::filesystem::path& spec_path) {
  const std::string stem = spec_path.stem().empty() ? "problem-spec" : spec_path.stem().string();
  return std::filesystem::path("artifacts") / stem;
}

amflow::ReductionOptions MakeBootstrapReductionOptions() {
  amflow::ReductionOptions options;
  options.ibp_reducer = "Kira";
  options.permutation_option = 1;
  return options;
}

std::string TrimAsciiWhitespace(const std::string& value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

struct CommandProbeResult {
  bool succeeded = false;
  std::string output;
};

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

CommandProbeResult RunShellCommand(const std::string& command) {
  std::array<char, 256> buffer{};
  CommandProbeResult result;
  FILE* pipe = ::popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return result;
  }
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output += buffer.data();
  }
  const int exit_code = ::pclose(pipe);
  if (exit_code != 0) {
    result.output.clear();
    return result;
  }
  result.succeeded = true;
  result.output = TrimAsciiWhitespace(result.output);
  return result;
}

std::filesystem::path FindRepositoryRoot(const std::filesystem::path& start_path) {
  std::filesystem::path current = std::filesystem::absolute(start_path);
  if (!std::filesystem::exists(current)) {
    current = current.parent_path();
  }
  while (!current.empty()) {
    if (std::filesystem::exists(current / ".git")) {
      return current;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return {};
}

CommandProbeResult GitOutput(const std::filesystem::path& repo_root, const std::string& args) {
  if (repo_root.empty()) {
    return {};
  }
  return RunShellCommand("git -C " + ShellSingleQuote(repo_root.string()) + " " + args +
                         " 2>/dev/null");
}

bool PathStartsWith(const std::filesystem::path& path, const std::filesystem::path& prefix) {
  auto path_it = path.begin();
  auto prefix_it = prefix.begin();
  for (; prefix_it != prefix.end(); ++path_it, ++prefix_it) {
    if (path_it == path.end() || *path_it != *prefix_it) {
      return false;
    }
  }
  return true;
}

std::string DetermineSpecProvenance(const std::filesystem::path& spec_path,
                                    const std::filesystem::path& repo_root) {
  const std::filesystem::path absolute_spec = std::filesystem::weakly_canonical(spec_path);
  if (!repo_root.empty() &&
      PathStartsWith(absolute_spec, std::filesystem::weakly_canonical(repo_root))) {
    const std::filesystem::path accepted_frozen_fixture =
        std::filesystem::weakly_canonical(repo_root / "specs/problem-spec.k0-smoke.yaml");
    if (absolute_spec == accepted_frozen_fixture) {
      return "repo-local frozen K0 smoke fixture derived from preserved input";
    }
    return "repo-local file-backed ProblemSpec";
  }
  return "external file-backed ProblemSpec";
}

std::map<std::string, std::string> EffectiveReductionOptionsMap(
    const amflow::ReductionOptions& options) {
  std::map<std::string, std::string> values;
  values["IBPReducer"] = options.ibp_reducer;
  values["BlackBoxRank"] = std::to_string(options.black_box_rank);
  values["BlackBoxDot"] = std::to_string(options.black_box_dot);
  values["ComplexMode"] = options.complex_mode ? "true" : "false";
  values["DeleteBlackBoxDirectory"] = options.delete_black_box_directory ? "true" : "false";
  values["IntegralOrder"] = std::to_string(options.integral_order);
  values["ReductionMode"] = amflow::ToString(options.reduction_mode);
  values["KiraInsertPrefactors"] = options.kira_insert_prefactors ? "true" : "false";
  if (options.kira_insert_prefactors_surface.has_value()) {
    values["KiraInsertPrefactorsSurface"] =
        amflow::SerializeKiraInsertPrefactorsSurface(*options.kira_insert_prefactors_surface);
  }
  if (options.permutation_option.has_value()) {
    values["PermutationOption"] = std::to_string(*options.permutation_option);
  }
  if (options.master_rank.has_value()) {
    values["MasterRank"] = std::to_string(*options.master_rank);
  }
  if (options.master_dot.has_value()) {
    values["MasterDot"] = std::to_string(*options.master_dot);
  }
  return values;
}

std::map<std::string, std::string> NonDefaultReductionOptionsMap(
    const amflow::ReductionOptions& options) {
  std::map<std::string, std::string> values;
  const amflow::ReductionOptions defaults;
  if (options.ibp_reducer != defaults.ibp_reducer) {
    values["IBPReducer"] = options.ibp_reducer;
  }
  if (options.black_box_rank != defaults.black_box_rank) {
    values["BlackBoxRank"] = std::to_string(options.black_box_rank);
  }
  if (options.black_box_dot != defaults.black_box_dot) {
    values["BlackBoxDot"] = std::to_string(options.black_box_dot);
  }
  if (options.complex_mode != defaults.complex_mode) {
    values["ComplexMode"] = options.complex_mode ? "true" : "false";
  }
  if (options.delete_black_box_directory != defaults.delete_black_box_directory) {
    values["DeleteBlackBoxDirectory"] =
        options.delete_black_box_directory ? "true" : "false";
  }
  if (options.integral_order != defaults.integral_order) {
    values["IntegralOrder"] = std::to_string(options.integral_order);
  }
  if (options.reduction_mode != defaults.reduction_mode) {
    values["ReductionMode"] = amflow::ToString(options.reduction_mode);
  }
  if (options.kira_insert_prefactors != defaults.kira_insert_prefactors) {
    values["KiraInsertPrefactors"] = options.kira_insert_prefactors ? "true" : "false";
  }
  const bool has_non_default_insert_prefactors_surface =
      options.kira_insert_prefactors_surface.has_value() &&
      (!defaults.kira_insert_prefactors_surface.has_value() ||
       amflow::SerializeKiraInsertPrefactorsSurface(*options.kira_insert_prefactors_surface) !=
           amflow::SerializeKiraInsertPrefactorsSurface(
               *defaults.kira_insert_prefactors_surface));
  if (has_non_default_insert_prefactors_surface) {
    values["KiraInsertPrefactorsSurface"] =
        amflow::SerializeKiraInsertPrefactorsSurface(*options.kira_insert_prefactors_surface);
  }
  if (options.permutation_option != defaults.permutation_option &&
      options.permutation_option.has_value()) {
    values["PermutationOption"] = std::to_string(*options.permutation_option);
  }
  if (options.master_rank != defaults.master_rank && options.master_rank.has_value()) {
    values["MasterRank"] = std::to_string(*options.master_rank);
  }
  if (options.master_dot != defaults.master_dot && options.master_dot.has_value()) {
    values["MasterDot"] = std::to_string(*options.master_dot);
  }
  return values;
}

std::string ToString(amflow::CommandExecutionStatus status) {
  switch (status) {
    case amflow::CommandExecutionStatus::NotRun:
      return "not-run";
    case amflow::CommandExecutionStatus::Completed:
      return "completed";
    case amflow::CommandExecutionStatus::FailedToStart:
      return "failed-to-start";
    case amflow::CommandExecutionStatus::InvalidConfiguration:
      return "invalid-configuration";
    case amflow::CommandExecutionStatus::Signaled:
      return "signaled";
  }
  return "unknown";
}

std::string ToString(amflow::ParsedReductionStatus status) {
  switch (status) {
    case amflow::ParsedReductionStatus::ParsedRules:
      return "parsed-rules";
    case amflow::ParsedReductionStatus::IdentityFallback:
      return "identity-fallback";
  }
  return "unknown";
}

void PrintExecutionResult(std::ostream& stream,
                          const amflow::CommandExecutionResult& result) {
  stream << "command: " << result.command << "\n";
  stream << "working_directory: " << result.working_directory.string() << "\n";
  stream << "status: " << ToString(result.status) << "\n";
  stream << "exit_code: " << result.exit_code << "\n";
  stream << "stdout_log: " << result.stdout_log_path.string() << "\n";
  stream << "stderr_log: " << result.stderr_log_path.string() << "\n";
  if (!result.environment_overrides.empty()) {
    stream << "environment_overrides:\n";
    for (const auto& [key, value] : result.environment_overrides) {
      stream << "  " << key << "=" << value << "\n";
    }
  }
  if (!result.error_message.empty()) {
    stream << "error: " << result.error_message << "\n";
  }
}

void PrintParsedReductionResult(std::ostream& stream,
                                const amflow::ParsedReductionResult& result) {
  stream << "family: " << result.master_list.family << "\n";
  stream << "masters_path: " << result.master_list.source_path.string() << "\n";
  stream << "rule_path: " << result.rule_path.string() << "\n";
  stream << "status: " << ToString(result.status) << "\n";
  stream << "master_count: " << result.master_list.masters.size() << "\n";
  stream << "explicit_rule_count: " << result.explicit_rule_count << "\n";
  stream << "total_rule_count: " << result.rules.size() << "\n";
  stream << "masters:\n";
  for (const auto& master : result.master_list.masters) {
    stream << "  - " << master.Label() << "\n";
  }
  stream << "rules:\n";
  for (const auto& rule : result.rules) {
    stream << "  - target: " << rule.target.Label() << "\n";
    for (const auto& term : rule.terms) {
      stream << "    term: " << term.coefficient << " * " << term.master.Label() << "\n";
    }
  }
}

int EmitKiraArtifacts(const amflow::ProblemSpec& spec, const std::filesystem::path& root) {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(root);
  amflow::KiraBackend backend;
  const amflow::BackendPreparation preparation =
      backend.Prepare(spec, MakeBootstrapReductionOptions(), layout);

  if (!preparation.validation_messages.empty()) {
    PrintMessages(std::cerr, preparation.validation_messages);
    return 2;
  }

  const auto write_messages = amflow::WritePreparationFiles(preparation, layout);
  if (!write_messages.empty()) {
    PrintMessages(std::cerr, write_messages);
    return 2;
  }
  for (const auto& [path, _] : preparation.generated_files) {
    std::cout << (layout.generated_config_dir / path).string() << "\n";
  }
  return 0;
}

int RunKiraForSpec(const amflow::ProblemSpec& spec,
                   const std::filesystem::path& root,
                   const std::filesystem::path& kira_executable,
                   const std::filesystem::path& fermat_executable,
                   const std::filesystem::path& spec_path = {},
                   const std::vector<std::string>& additional_validation_messages = {}) {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(root);
  const amflow::ReductionOptions reduction_options = MakeBootstrapReductionOptions();
  amflow::KiraBackend backend;
  amflow::BackendPreparation preparation = backend.Prepare(spec, reduction_options, layout);
  preparation.validation_messages.insert(preparation.validation_messages.end(),
                                         additional_validation_messages.begin(),
                                         additional_validation_messages.end());

  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_executable, fermat_executable);
  if (!spec_path.empty()) {
    const std::filesystem::path repo_root = FindRepositoryRoot(spec_path);
    const CommandProbeResult commit_probe = GitOutput(repo_root, "rev-parse HEAD");
    const CommandProbeResult status_probe = GitOutput(repo_root, "status --short");
    const amflow::FileBackedKiraRunManifestInput manifest_input = {
        spec_path,
        DetermineSpecProvenance(spec_path, repo_root),
        amflow::SerializeProblemSpecYaml(spec),
        spec.family.name,
        spec.targets.size(),
        layout.root,
        result.working_directory,
        kira_executable,
        fermat_executable,
        result.command,
        ToString(result.status),
        result.exit_code,
        repo_root,
        commit_probe.succeeded ? commit_probe.output : std::string{},
        status_probe.succeeded ? (status_probe.output.empty() ? std::string("clean")
                                                              : status_probe.output)
                               : std::string{},
        1,
        EffectiveReductionOptionsMap(reduction_options),
        NonDefaultReductionOptionsMap(reduction_options),
        result.stdout_log_path,
        result.stderr_log_path,
    };
    amflow::WriteArtifactManifest(layout, amflow::MakeFileBackedKiraRunManifest(manifest_input));
  }
  if (result.Succeeded()) {
    PrintExecutionResult(std::cout, result);
    return 0;
  }

  PrintExecutionResult(std::cerr, result);
  return result.status == amflow::CommandExecutionStatus::InvalidConfiguration ? 2 : 4;
}

struct CliYamlLine {
  std::size_t number = 0;
  int indent = 0;
  std::string text;
};

struct DirectSolveSeriesSpec {
  bool present = false;
  bool amflow_state_input = false;
  bool retained_solution_samples_input = false;
  std::string benchmark_id;
  std::string amflow_output_name;
  std::string amflow_config_raw;
  std::string family;
  std::string integral_kind;
  std::string variable;
  std::string start_location;
  std::string target_location;
  std::vector<amflow::MasterIntegral> masters;
  std::vector<amflow::MasterIntegral> retained_reduction_masters;
  std::vector<amflow::TargetIntegral> targets;
  std::map<std::string, std::vector<std::vector<std::string>>> coefficient_matrices;
  std::vector<std::string> singular_points;
  std::vector<amflow::BoundaryCondition> boundary_conditions;
  std::string boundary_state_kind;
  std::string boundary_state_direction;
  std::vector<std::string> boundary_epsilon_samples;
  std::map<std::string, std::string> boundary_state_raw_files;
  std::string finite_source_variable;
  std::string finite_solution_basis_reduction_path;
  std::string target_reduction_path;
  std::vector<int> phase_space_prescription;
  std::vector<int> phase_space_cut;
};

[[noreturn]] void FailCliYamlParse(const std::size_t line_number,
                                   const std::string& message) {
  throw std::runtime_error("solve_series parse error at line " +
                           std::to_string(line_number) + ": " + message);
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::vector<CliYamlLine> TokenizeCliYaml(const std::string& yaml) {
  std::vector<CliYamlLine> lines;
  std::istringstream stream(yaml);
  std::string raw;
  std::size_t line_number = 0;
  while (std::getline(stream, raw)) {
    ++line_number;
    if (!raw.empty() && raw.back() == '\r') {
      raw.pop_back();
    }
    const std::size_t first_non_space = raw.find_first_not_of(' ');
    if (first_non_space == std::string::npos || raw[first_non_space] == '#') {
      continue;
    }
    if (first_non_space % 2 != 0) {
      FailCliYamlParse(line_number, "indentation must use multiples of two spaces");
    }
    lines.push_back({line_number,
                     static_cast<int>(first_non_space),
                     raw.substr(first_non_space)});
  }
  return lines;
}

std::pair<std::string, std::string> SplitCliYamlKeyValue(const CliYamlLine& line,
                                                         const std::string& text) {
  const std::size_t separator = text.find(':');
  if (separator == std::string::npos) {
    FailCliYamlParse(line.number, "expected key/value pair");
  }
  return {TrimAsciiWhitespace(text.substr(0, separator)),
          TrimAsciiWhitespace(text.substr(separator + 1))};
}

std::string ParseCliYamlString(const CliYamlLine& line, const std::string& value) {
  const std::string trimmed = TrimAsciiWhitespace(value);
  if (trimmed.empty()) {
    FailCliYamlParse(line.number, "expected a scalar value");
  }
  if (trimmed.front() != '"') {
    return trimmed;
  }
  if (trimmed.size() < 2 || trimmed.back() != '"') {
    FailCliYamlParse(line.number, "unterminated quoted string");
  }
  std::string parsed;
  parsed.reserve(trimmed.size() - 2);
  for (std::size_t index = 1; index + 1 < trimmed.size(); ++index) {
    const char character = trimmed[index];
    if (character != '\\') {
      parsed.push_back(character);
      continue;
    }
    if (index + 1 >= trimmed.size() - 1) {
      FailCliYamlParse(line.number, "invalid escape sequence");
    }
    const char escaped = trimmed[++index];
    switch (escaped) {
      case '\\':
      case '"':
        parsed.push_back(escaped);
        break;
      case 'n':
        parsed.push_back('\n');
        break;
      case 't':
        parsed.push_back('\t');
        break;
      default:
        FailCliYamlParse(line.number, "unsupported escape sequence");
    }
  }
  return parsed;
}

std::vector<std::string> SplitCliYamlListItems(const CliYamlLine& line,
                                               const std::string& value) {
  const std::string trimmed = TrimAsciiWhitespace(value);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    FailCliYamlParse(line.number, "expected a bracketed list");
  }
  const std::string inner = TrimAsciiWhitespace(trimmed.substr(1, trimmed.size() - 2));
  if (inner.empty()) {
    return {};
  }

  std::vector<std::string> items;
  std::string current;
  bool in_quotes = false;
  bool escaping = false;
  for (const char character : inner) {
    if (escaping) {
      current.push_back(character);
      escaping = false;
      continue;
    }
    if (character == '\\') {
      current.push_back(character);
      escaping = true;
      continue;
    }
    if (character == '"') {
      in_quotes = !in_quotes;
      current.push_back(character);
      continue;
    }
    if (character == ',' && !in_quotes) {
      items.push_back(TrimAsciiWhitespace(current));
      current.clear();
      continue;
    }
    current.push_back(character);
  }
  if (in_quotes) {
    FailCliYamlParse(line.number, "unterminated quoted string in list");
  }
  items.push_back(TrimAsciiWhitespace(current));
  return items;
}

std::vector<std::string> ParseCliYamlStringList(const CliYamlLine& line,
                                                const std::string& value) {
  std::vector<std::string> parsed;
  for (const auto& item : SplitCliYamlListItems(line, value)) {
    parsed.push_back(ParseCliYamlString(line, item));
  }
  return parsed;
}

int ParseCliYamlInteger(const CliYamlLine& line, const std::string& value) {
  const std::string trimmed = TrimAsciiWhitespace(value);
  if (trimmed.empty()) {
    FailCliYamlParse(line.number, "expected an integer value");
  }
  std::size_t consumed = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(trimmed, &consumed);
  } catch (const std::exception&) {
    FailCliYamlParse(line.number, "invalid integer value");
  }
  if (consumed != trimmed.size()) {
    FailCliYamlParse(line.number, "invalid integer value");
  }
  return parsed;
}

std::vector<int> ParseCliYamlIntegerList(const CliYamlLine& line,
                                         const std::string& value) {
  std::vector<int> parsed;
  for (const auto& item : SplitCliYamlListItems(line, value)) {
    parsed.push_back(ParseCliYamlInteger(line, item));
  }
  return parsed;
}

void SkipCliYamlBlock(const std::vector<CliYamlLine>& lines,
                      std::size_t& index,
                      const int effective_indent) {
  ++index;
  while (index < lines.size() && lines[index].indent > effective_indent) {
    ++index;
  }
}

void RecordUniqueCliYamlKey(std::set<std::string>& seen,
                            const CliYamlLine& line,
                            const std::string& scope,
                            const std::string& key) {
  if (!seen.insert(key).second) {
    FailCliYamlParse(line.number, "duplicate " + scope + " field: " + key);
  }
}

bool ApplySolveSeriesMasterField(amflow::MasterIntegral& master,
                                 const CliYamlLine& line,
                                 const std::string& key,
                                 const std::string& value) {
  if (key == "family") {
    master.family = ParseCliYamlString(line, value);
    return true;
  }
  if (key == "indices") {
    master.indices = ParseCliYamlIntegerList(line, value);
    return true;
  }
  if (key == "label") {
    master.label = ParseCliYamlString(line, value);
    return true;
  }
  return false;
}

bool ApplySolveSeriesBoundaryField(amflow::BoundaryCondition& condition,
                                   const CliYamlLine& line,
                                   const std::string& key,
                                   const std::string& value) {
  if (key == "variable") {
    condition.variable = ParseCliYamlString(line, value);
    return true;
  }
  if (key == "location") {
    condition.location = ParseCliYamlString(line, value);
    return true;
  }
  if (key == "values") {
    condition.values = ParseCliYamlStringList(line, value);
    return true;
  }
  if (key == "strategy") {
    condition.strategy = ParseCliYamlString(line, value);
    return true;
  }
  return false;
}

std::vector<amflow::MasterIntegral> ParseSolveSeriesMasters(
    const std::vector<CliYamlLine>& lines,
    std::size_t& index,
    const int parent_indent) {
  std::vector<amflow::MasterIntegral> masters;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const CliYamlLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      FailCliYamlParse(line.number, "expected a masters list item");
    }

    amflow::MasterIntegral master;
    std::set<std::string> seen_fields;
    const auto [first_key, first_value] =
        SplitCliYamlKeyValue(line, TrimAsciiWhitespace(line.text.substr(2)));
    RecordUniqueCliYamlKey(seen_fields, line, "master", first_key);
    if (ApplySolveSeriesMasterField(master, line, first_key, first_value)) {
      ++index;
    } else {
      SkipCliYamlBlock(lines, index, parent_indent + 4);
    }

    while (index < lines.size() && lines[index].indent > parent_indent + 2) {
      const CliYamlLine& nested = lines[index];
      if (nested.indent != parent_indent + 4) {
        FailCliYamlParse(nested.number, "unexpected indentation inside masters");
      }
      const auto [key, value] = SplitCliYamlKeyValue(nested, nested.text);
      RecordUniqueCliYamlKey(seen_fields, nested, "master", key);
      if (ApplySolveSeriesMasterField(master, nested, key, value)) {
        ++index;
      } else {
        SkipCliYamlBlock(lines, index, nested.indent);
      }
    }

    if (master.family.empty()) {
      FailCliYamlParse(line.number, "master family must not be empty");
    }
    if (master.indices.empty()) {
      FailCliYamlParse(line.number, "master indices must not be empty");
    }
    masters.push_back(std::move(master));
  }
  return masters;
}

std::vector<std::vector<std::string>> ParseSolveSeriesMatrixRows(
    const std::vector<CliYamlLine>& lines,
    std::size_t& index,
    const int parent_indent) {
  std::vector<std::vector<std::string>> rows;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const CliYamlLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      FailCliYamlParse(line.number, "expected a coefficient matrix row");
    }
    rows.push_back(ParseCliYamlStringList(line, TrimAsciiWhitespace(line.text.substr(2))));
    ++index;
  }
  if (rows.empty()) {
    FailCliYamlParse(lines[index - 1].number, "coefficient matrix must not be empty");
  }
  return rows;
}

std::map<std::string, std::vector<std::vector<std::string>>> ParseSolveSeriesMatrices(
    const std::vector<CliYamlLine>& lines,
    std::size_t& index,
    const int parent_indent) {
  std::map<std::string, std::vector<std::vector<std::string>>> matrices;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const CliYamlLine& line = lines[index];
    if (line.indent != parent_indent + 2) {
      FailCliYamlParse(line.number, "unexpected indentation inside coefficient_matrices");
    }
    const auto [variable, value] = SplitCliYamlKeyValue(line, line.text);
    if (!value.empty()) {
      FailCliYamlParse(line.number, "coefficient matrix entries must use block row syntax");
    }
    ++index;
    auto rows = ParseSolveSeriesMatrixRows(lines, index, line.indent);
    if (!matrices.emplace(variable, std::move(rows)).second) {
      FailCliYamlParse(line.number, "duplicate coefficient matrix variable: " + variable);
    }
  }
  return matrices;
}

std::vector<amflow::BoundaryCondition> ParseSolveSeriesBoundaryConditions(
    const std::vector<CliYamlLine>& lines,
    std::size_t& index,
    const int parent_indent) {
  std::vector<amflow::BoundaryCondition> conditions;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const CliYamlLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      FailCliYamlParse(line.number, "expected a boundary_conditions list item");
    }

    amflow::BoundaryCondition condition;
    std::set<std::string> seen_fields;
    const auto [first_key, first_value] =
        SplitCliYamlKeyValue(line, TrimAsciiWhitespace(line.text.substr(2)));
    RecordUniqueCliYamlKey(seen_fields, line, "boundary condition", first_key);
    if (ApplySolveSeriesBoundaryField(condition, line, first_key, first_value)) {
      ++index;
    } else {
      SkipCliYamlBlock(lines, index, parent_indent + 4);
    }

    while (index < lines.size() && lines[index].indent > parent_indent + 2) {
      const CliYamlLine& nested = lines[index];
      if (nested.indent != parent_indent + 4) {
        FailCliYamlParse(nested.number, "unexpected indentation inside boundary_conditions");
      }
      const auto [key, value] = SplitCliYamlKeyValue(nested, nested.text);
      RecordUniqueCliYamlKey(seen_fields, nested, "boundary condition", key);
      if (ApplySolveSeriesBoundaryField(condition, nested, key, value)) {
        ++index;
      } else {
        SkipCliYamlBlock(lines, index, nested.indent);
      }
    }

    if (condition.variable.empty()) {
      FailCliYamlParse(line.number, "boundary condition variable must not be empty");
    }
    if (condition.location.empty()) {
      FailCliYamlParse(line.number, "boundary condition location must not be empty");
    }
    if (condition.values.empty()) {
      FailCliYamlParse(line.number, "boundary condition values must not be empty");
    }
    conditions.push_back(std::move(condition));
  }
  return conditions;
}

DirectSolveSeriesSpec ParseDirectSolveSeriesSpec(const std::string& yaml) {
  const std::vector<CliYamlLine> lines = TokenizeCliYaml(yaml);
  DirectSolveSeriesSpec spec;

  std::size_t index = 0;
  while (index < lines.size()) {
    const CliYamlLine& line = lines[index];
    if (line.indent != 0) {
      FailCliYamlParse(line.number, "unexpected indentation at top level");
    }
    const auto [key, value] = SplitCliYamlKeyValue(line, line.text);
    if (key != "solve_series") {
      SkipCliYamlBlock(lines, index, line.indent);
      continue;
    }
    if (spec.present) {
      FailCliYamlParse(line.number, "duplicate top-level solve_series block");
    }
    if (!value.empty()) {
      FailCliYamlParse(line.number, "solve_series must use mapping syntax");
    }
    spec.present = true;
    ++index;

    std::set<std::string> seen_fields;
    while (index < lines.size() && lines[index].indent > line.indent) {
      const CliYamlLine& nested = lines[index];
      if (nested.indent != line.indent + 2) {
        FailCliYamlParse(nested.number, "unexpected indentation inside solve_series");
      }
      const auto [field, field_value] = SplitCliYamlKeyValue(nested, nested.text);
      RecordUniqueCliYamlKey(seen_fields, nested, "solve_series", field);
      if (field == "benchmark_id") {
        spec.benchmark_id = ParseCliYamlString(nested, field_value);
        ++index;
        continue;
      }
      if (field == "variable") {
        spec.variable = ParseCliYamlString(nested, field_value);
        ++index;
        continue;
      }
      if (field == "start_location") {
        spec.start_location = ParseCliYamlString(nested, field_value);
        ++index;
        continue;
      }
      if (field == "target_location") {
        spec.target_location = ParseCliYamlString(nested, field_value);
        ++index;
        continue;
      }
      if (field == "target_reduction_path") {
        spec.target_reduction_path = ParseCliYamlString(nested, field_value);
        ++index;
        continue;
      }
      if (field == "singular_points") {
        spec.singular_points = ParseCliYamlStringList(nested, field_value);
        ++index;
        continue;
      }
      if (field == "masters") {
        if (!field_value.empty()) {
          FailCliYamlParse(nested.number, "masters must use block list syntax");
        }
        ++index;
        spec.masters = ParseSolveSeriesMasters(lines, index, nested.indent);
        continue;
      }
      if (field == "coefficient_matrices") {
        if (!field_value.empty()) {
          FailCliYamlParse(nested.number, "coefficient_matrices must use mapping syntax");
        }
        ++index;
        spec.coefficient_matrices =
            ParseSolveSeriesMatrices(lines, index, nested.indent);
        continue;
      }
      if (field == "boundary_conditions") {
        if (!field_value.empty()) {
          FailCliYamlParse(nested.number, "boundary_conditions must use block list syntax");
        }
        ++index;
        spec.boundary_conditions =
            ParseSolveSeriesBoundaryConditions(lines, index, nested.indent);
        continue;
      }
      SkipCliYamlBlock(lines, index, nested.indent);
    }
  }

  return spec;
}

void ValidateDirectSolveSeriesSpec(const DirectSolveSeriesSpec& spec) {
  if (!spec.present) {
    throw std::invalid_argument(
        "solve-series requires an embedded solve_series direct-solver block; plain "
        "ProblemSpec files do not carry a DE system or boundary conditions");
  }
  if (spec.variable.empty()) {
    throw std::invalid_argument("solve_series.variable must not be empty");
  }
  if (spec.start_location.empty()) {
    throw std::invalid_argument("solve_series.start_location must not be empty");
  }
  if (spec.target_location.empty()) {
    throw std::invalid_argument("solve_series.target_location must not be empty");
  }
  if (spec.masters.empty()) {
    throw std::invalid_argument("solve_series.masters must not be empty");
  }
  if (!(spec.amflow_state_input && spec.retained_solution_samples_input) &&
      spec.coefficient_matrices.find(spec.variable) == spec.coefficient_matrices.end()) {
    throw std::invalid_argument("solve_series.coefficient_matrices must include variable " +
                                spec.variable);
  }
  for (const std::string& singular_point : spec.singular_points) {
    if (TrimAsciiWhitespace(singular_point).empty()) {
      throw std::invalid_argument("solve_series.singular_points entries must not be empty");
    }
  }
  if (spec.boundary_conditions.empty() && !spec.amflow_state_input) {
    throw std::invalid_argument("solve_series.boundary_conditions must not be empty");
  }
  if (spec.amflow_state_input && spec.boundary_state_kind.empty()) {
    throw std::invalid_argument("AMFlow solve-series state boundary_state.kind must not be empty");
  }
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("failed to open file: " + path.string());
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

void WriteTextFile(const std::filesystem::path& path, const std::string& contents) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream stream(path, std::ios::out | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("failed to open output file: " + path.string());
  }
  stream << contents;
  if (!stream) {
    throw std::runtime_error("failed to write output file: " + path.string());
  }
}

std::string IntegralLabel(const std::string& family, const std::vector<int>& indices) {
  std::ostringstream out;
  out << family << "[";
  for (std::size_t index = 0; index < indices.size(); ++index) {
    if (index > 0) {
      out << ",";
    }
    out << indices[index];
  }
  out << "]";
  return out.str();
}

std::string JsonString(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (const char character : value) {
    switch (character) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(character) < 0x20) {
          out << "\\u00";
          const char* digits = "0123456789abcdef";
          out << digits[(static_cast<unsigned char>(character) >> 4) & 0x0f];
          out << digits[static_cast<unsigned char>(character) & 0x0f];
        } else {
          out << character;
        }
    }
  }
  out << '"';
  return out.str();
}

struct CliJsonValue {
  enum class Kind {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object,
  };

  Kind kind = Kind::Null;
  bool boolean_value = false;
  std::string scalar;
  std::vector<CliJsonValue> array;
  std::map<std::string, CliJsonValue> object;
};

class CliJsonParser {
 public:
  explicit CliJsonParser(const std::string& input) : input_(input) {}

  CliJsonValue Parse() {
    SkipWhitespace();
    CliJsonValue value = ParseValue();
    SkipWhitespace();
    if (position_ != input_.size()) {
      throw std::runtime_error("solve-series JSON parse error: unexpected trailing input");
    }
    return value;
  }

 private:
  [[noreturn]] void Fail(const std::string& message) const {
    throw std::runtime_error("solve-series JSON parse error at byte " +
                             std::to_string(position_) + ": " + message);
  }

  void SkipWhitespace() {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
      ++position_;
    }
  }

  bool Consume(const char expected) {
    if (position_ < input_.size() && input_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  char Peek() const {
    return position_ < input_.size() ? input_[position_] : '\0';
  }

  void ExpectLiteral(const std::string& literal) {
    if (input_.compare(position_, literal.size(), literal) != 0) {
      Fail("expected " + literal);
    }
    position_ += literal.size();
  }

  static int HexValue(const char value) {
    if (value >= '0' && value <= '9') {
      return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
      return 10 + value - 'a';
    }
    if (value >= 'A' && value <= 'F') {
      return 10 + value - 'A';
    }
    return -1;
  }

  std::string ParseStringLiteral() {
    if (!Consume('"')) {
      Fail("expected string");
    }

    std::string value;
    while (position_ < input_.size()) {
      const char character = input_[position_++];
      if (character == '"') {
        return value;
      }
      if (static_cast<unsigned char>(character) < 0x20) {
        Fail("unescaped control character in string");
      }
      if (character != '\\') {
        value.push_back(character);
        continue;
      }
      if (position_ >= input_.size()) {
        Fail("unterminated escape sequence");
      }
      const char escaped = input_[position_++];
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          value.push_back(escaped);
          break;
        case 'b':
          value.push_back('\b');
          break;
        case 'f':
          value.push_back('\f');
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        case 'u': {
          if (position_ + 4 > input_.size()) {
            Fail("incomplete unicode escape");
          }
          int codepoint = 0;
          for (int index = 0; index < 4; ++index) {
            const int hex = HexValue(input_[position_++]);
            if (hex < 0) {
              Fail("invalid unicode escape");
            }
            codepoint = codepoint * 16 + hex;
          }
          value.push_back(codepoint >= 0x20 && codepoint < 0x7f
                              ? static_cast<char>(codepoint)
                              : '?');
          break;
        }
        default:
          Fail("unsupported escape sequence");
      }
    }
    Fail("unterminated string");
  }

  CliJsonValue ParseArray() {
    CliJsonValue value;
    value.kind = CliJsonValue::Kind::Array;
    Consume('[');
    SkipWhitespace();
    if (Consume(']')) {
      return value;
    }
    while (true) {
      value.array.push_back(ParseValue());
      SkipWhitespace();
      if (Consume(']')) {
        return value;
      }
      if (!Consume(',')) {
        Fail("expected ',' or ']'");
      }
      SkipWhitespace();
    }
  }

  CliJsonValue ParseObject() {
    CliJsonValue value;
    value.kind = CliJsonValue::Kind::Object;
    Consume('{');
    SkipWhitespace();
    if (Consume('}')) {
      return value;
    }
    while (true) {
      if (Peek() != '"') {
        Fail("expected object key");
      }
      const std::string key = ParseStringLiteral();
      SkipWhitespace();
      if (!Consume(':')) {
        Fail("expected ':' after object key");
      }
      SkipWhitespace();
      CliJsonValue member = ParseValue();
      if (!value.object.emplace(key, std::move(member)).second) {
        Fail("duplicate object key: " + key);
      }
      SkipWhitespace();
      if (Consume('}')) {
        return value;
      }
      if (!Consume(',')) {
        Fail("expected ',' or '}'");
      }
      SkipWhitespace();
    }
  }

  CliJsonValue ParseNumber() {
    const std::size_t start = position_;
    Consume('-');
    if (Peek() == '0') {
      ++position_;
    } else if (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
      while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
        ++position_;
      }
    } else {
      Fail("expected number");
    }
    if (Consume('.')) {
      if (std::isdigit(static_cast<unsigned char>(Peek())) == 0) {
        Fail("expected fractional digits");
      }
      while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
        ++position_;
      }
    }
    if (Peek() == 'e' || Peek() == 'E') {
      ++position_;
      if (Peek() == '+' || Peek() == '-') {
        ++position_;
      }
      if (std::isdigit(static_cast<unsigned char>(Peek())) == 0) {
        Fail("expected exponent digits");
      }
      while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
        ++position_;
      }
    }

    CliJsonValue value;
    value.kind = CliJsonValue::Kind::Number;
    value.scalar = input_.substr(start, position_ - start);
    return value;
  }

  CliJsonValue ParseValue() {
    SkipWhitespace();
    const char character = Peek();
    if (character == '"') {
      CliJsonValue value;
      value.kind = CliJsonValue::Kind::String;
      value.scalar = ParseStringLiteral();
      return value;
    }
    if (character == '{') {
      return ParseObject();
    }
    if (character == '[') {
      return ParseArray();
    }
    if (character == '-' || std::isdigit(static_cast<unsigned char>(character)) != 0) {
      return ParseNumber();
    }
    if (input_.compare(position_, 4, "true") == 0) {
      ExpectLiteral("true");
      CliJsonValue value;
      value.kind = CliJsonValue::Kind::Boolean;
      value.boolean_value = true;
      return value;
    }
    if (input_.compare(position_, 5, "false") == 0) {
      ExpectLiteral("false");
      CliJsonValue value;
      value.kind = CliJsonValue::Kind::Boolean;
      value.boolean_value = false;
      return value;
    }
    if (input_.compare(position_, 4, "null") == 0) {
      ExpectLiteral("null");
      return {};
    }
    Fail("expected JSON value");
  }

  const std::string& input_;
  std::size_t position_ = 0;
};

bool LooksLikeJsonObject(const std::string& value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  return first != std::string::npos && value[first] == '{';
}

const CliJsonValue& RequireJsonObject(const CliJsonValue& value, const std::string& path) {
  if (value.kind != CliJsonValue::Kind::Object) {
    throw std::runtime_error(path + " must be a JSON object");
  }
  return value;
}

const CliJsonValue& RequireJsonArray(const CliJsonValue& value, const std::string& path) {
  if (value.kind != CliJsonValue::Kind::Array) {
    throw std::runtime_error(path + " must be a JSON array");
  }
  return value;
}

const CliJsonValue& RequireJsonField(const CliJsonValue& object,
                                     const std::string& field,
                                     const std::string& path) {
  RequireJsonObject(object, path);
  const auto it = object.object.find(field);
  if (it == object.object.end()) {
    throw std::runtime_error(path + "." + field + " is required");
  }
  return it->second;
}

const CliJsonValue* FindJsonField(const CliJsonValue& object, const std::string& field) {
  if (object.kind != CliJsonValue::Kind::Object) {
    return nullptr;
  }
  const auto it = object.object.find(field);
  return it == object.object.end() ? nullptr : &it->second;
}

std::string RequireJsonString(const CliJsonValue& value, const std::string& path) {
  if (value.kind != CliJsonValue::Kind::String) {
    throw std::runtime_error(path + " must be a JSON string");
  }
  return value.scalar;
}

std::string OptionalJsonStringField(const CliJsonValue& object,
                                    const std::string& field,
                                    const std::string& fallback) {
  const CliJsonValue* value = FindJsonField(object, field);
  return value == nullptr ? fallback : RequireJsonString(*value, field);
}

bool RequireJsonBoolean(const CliJsonValue& value, const std::string& path) {
  if (value.kind != CliJsonValue::Kind::Boolean) {
    throw std::runtime_error(path + " must be a JSON boolean");
  }
  return value.boolean_value;
}

int RequireJsonInteger(const CliJsonValue& value, const std::string& path) {
  if (value.kind != CliJsonValue::Kind::Number) {
    throw std::runtime_error(path + " must be a JSON integer");
  }
  if (value.scalar.find_first_of(".eE") != std::string::npos) {
    throw std::runtime_error(path + " must be a JSON integer");
  }
  std::size_t consumed = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(value.scalar, &consumed);
  } catch (const std::exception&) {
    throw std::runtime_error(path + " is outside the supported integer range");
  }
  if (consumed != value.scalar.size()) {
    throw std::runtime_error(path + " must be a JSON integer");
  }
  return parsed;
}

std::vector<int> RequireJsonIntegerArray(const CliJsonValue& value,
                                         const std::string& path) {
  const CliJsonValue& array = RequireJsonArray(value, path);
  std::vector<int> parsed;
  parsed.reserve(array.array.size());
  for (std::size_t index = 0; index < array.array.size(); ++index) {
    parsed.push_back(RequireJsonInteger(array.array[index],
                                        path + "[" + std::to_string(index) + "]"));
  }
  return parsed;
}

std::vector<std::string> RequireJsonStringArray(const CliJsonValue& value,
                                                const std::string& path) {
  const CliJsonValue& array = RequireJsonArray(value, path);
  std::vector<std::string> parsed;
  parsed.reserve(array.array.size());
  for (std::size_t index = 0; index < array.array.size(); ++index) {
    parsed.push_back(RequireJsonString(array.array[index],
                                       path + "[" + std::to_string(index) + "]"));
  }
  return parsed;
}

std::string StripWrappedQuoteLiteral(const std::string& value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

amflow::MasterIntegral ParseAmflowStateMaster(const CliJsonValue& value,
                                              const std::string& path) {
  RequireJsonObject(value, path);
  amflow::MasterIntegral master;
  master.family = RequireJsonString(RequireJsonField(value, "family", path),
                                    path + ".family");
  master.indices = RequireJsonIntegerArray(RequireJsonField(value, "indices", path),
                                           path + ".indices");
  master.label = OptionalJsonStringField(value, "label", "");
  if (master.family.empty()) {
    throw std::runtime_error(path + ".family must not be empty");
  }
  if (master.indices.empty()) {
    throw std::runtime_error(path + ".indices must not be empty");
  }
  return master;
}

amflow::TargetIntegral ParseAmflowStateTarget(const CliJsonValue& value,
                                              const std::string& path) {
  RequireJsonObject(value, path);
  amflow::TargetIntegral target;
  target.family = RequireJsonString(RequireJsonField(value, "family", path),
                                    path + ".family");
  target.indices = RequireJsonIntegerArray(RequireJsonField(value, "indices", path),
                                           path + ".indices");
  if (target.family.empty()) {
    throw std::runtime_error(path + ".family must not be empty");
  }
  if (target.indices.empty()) {
    throw std::runtime_error(path + ".indices must not be empty");
  }
  return target;
}

std::vector<amflow::MasterIntegral> ParseAmflowStateMasters(const CliJsonValue& value,
                                                            const std::string& path) {
  const CliJsonValue& array = RequireJsonArray(value, path);
  std::vector<amflow::MasterIntegral> masters;
  masters.reserve(array.array.size());
  for (std::size_t index = 0; index < array.array.size(); ++index) {
    masters.push_back(ParseAmflowStateMaster(array.array[index],
                                             path + "[" + std::to_string(index) + "]"));
  }
  return masters;
}

std::vector<amflow::TargetIntegral> ParseAmflowStateTargets(const CliJsonValue& value,
                                                            const std::string& path) {
  const CliJsonValue& array = RequireJsonArray(value, path);
  std::vector<amflow::TargetIntegral> targets;
  targets.reserve(array.array.size());
  for (std::size_t index = 0; index < array.array.size(); ++index) {
    targets.push_back(ParseAmflowStateTarget(array.array[index],
                                             path + "[" + std::to_string(index) + "]"));
  }
  return targets;
}

void AppendUniqueMaster(std::vector<amflow::MasterIntegral>& masters,
                        const amflow::MasterIntegral& master) {
  const std::string label = IntegralLabel(master.family, master.indices);
  const bool exists =
      std::any_of(masters.begin(),
                  masters.end(),
                  [&label](const amflow::MasterIntegral& candidate) {
                    return IntegralLabel(candidate.family, candidate.indices) == label;
                  });
  if (!exists) {
    masters.push_back(master);
  }
}

std::vector<std::vector<std::string>> ParseAmflowStateStringMatrix(
    const CliJsonValue& value,
    const std::string& path) {
  const CliJsonValue& rows = RequireJsonArray(value, path);
  std::vector<std::vector<std::string>> matrix;
  matrix.reserve(rows.array.size());
  for (std::size_t row_index = 0; row_index < rows.array.size(); ++row_index) {
    matrix.push_back(RequireJsonStringArray(
        rows.array[row_index], path + "[" + std::to_string(row_index) + "]"));
  }
  return matrix;
}

std::map<std::string, std::vector<std::vector<std::string>>>
ParseAmflowStateCoefficientMatrices(const CliJsonValue& value, const std::string& path) {
  RequireJsonObject(value, path);
  std::map<std::string, std::vector<std::vector<std::string>>> matrices;
  for (const auto& [variable, matrix] : value.object) {
    matrices.emplace(variable, ParseAmflowStateStringMatrix(matrix, path + "." + variable));
  }
  return matrices;
}

std::map<std::string, std::string> ParseAmflowStateBoundaryRawFiles(
    const CliJsonValue& value,
    const std::string& path) {
  RequireJsonObject(value, path);
  std::map<std::string, std::string> files;
  for (const auto& [name, payload] : value.object) {
    RequireJsonObject(payload, path + "." + name);
    files.emplace(name,
                  RequireJsonString(RequireJsonField(payload, "raw", path + "." + name),
                                    path + "." + name + ".raw"));
  }
  return files;
}

DirectSolveSeriesSpec ParseAmflowSolveSeriesStateJsonRoot(
    const CliJsonValue& root,
    const std::string& path) {
  RequireJsonObject(root, path);
  const std::string kind =
      RequireJsonString(RequireJsonField(root, "kind", path), path + ".kind");
  if (kind != "amflow_solve_series_state") {
    throw std::invalid_argument(
        "solve-series JSON input must have kind \"amflow_solve_series_state\"");
  }
  if (RequireJsonInteger(RequireJsonField(root, "schema_version", path),
                         path + ".schema_version") != 1) {
    throw std::invalid_argument("solve-series AMFlow state JSON schema_version must be 1");
  }

  DirectSolveSeriesSpec spec;
  spec.present = true;
  spec.amflow_state_input = true;
  spec.benchmark_id = OptionalJsonStringField(root, "benchmark_id", "");
  spec.amflow_output_name = OptionalJsonStringField(root, "amflow_output_name", "");
  if (const CliJsonValue* amflow_config = FindJsonField(root, "amflow_config")) {
    RequireJsonObject(*amflow_config, path + ".amflow_config");
    if (const CliJsonValue* raw_config = FindJsonField(*amflow_config, "raw")) {
      RequireJsonObject(*raw_config, path + ".amflow_config.raw");
      if (const CliJsonValue* raw_text = FindJsonField(*raw_config, "raw")) {
        spec.amflow_config_raw =
            RequireJsonString(*raw_text, path + ".amflow_config.raw.raw");
      }
    }
  }
  spec.integral_kind = OptionalJsonStringField(root, "integral_kind", "");
  spec.family = RequireJsonString(RequireJsonField(root, "family", path), path + ".family");
  spec.variable =
      RequireJsonString(RequireJsonField(root, "variable", path), path + ".variable");
  spec.start_location = OptionalJsonStringField(root, "start_location", "infinity");
  spec.target_location =
      OptionalJsonStringField(root, "target_location", spec.variable + "=0");
  spec.masters = ParseAmflowStateMasters(RequireJsonField(root, "masters", path),
                                         path + ".masters");
  if (const CliJsonValue* solution_sample_cache =
          FindJsonField(root, "solution_sample_cache")) {
    RequireJsonObject(*solution_sample_cache, path + ".solution_sample_cache");
    spec.retained_solution_samples_input =
        RequireJsonBoolean(
            RequireJsonField(*solution_sample_cache,
                             "enabled",
                             path + ".solution_sample_cache"),
            path + ".solution_sample_cache.enabled");
  }
  if (const CliJsonValue* coefficient_matrices =
          FindJsonField(root, "coefficient_matrices")) {
    spec.coefficient_matrices = ParseAmflowStateCoefficientMatrices(
        *coefficient_matrices,
        path + ".coefficient_matrices");
  } else if (!spec.retained_solution_samples_input) {
    throw std::invalid_argument(
        "solve-series AMFlow state JSON must include coefficient_matrices unless "
        "solution_sample_cache.enabled is true");
  }
  if (const CliJsonValue* singular_points = FindJsonField(root, "singular_points")) {
    spec.singular_points =
        RequireJsonStringArray(*singular_points, path + ".singular_points");
    for (const std::string& singular_point : spec.singular_points) {
      if (TrimAsciiWhitespace(singular_point).empty()) {
        throw std::invalid_argument(path + ".singular_points entries must not be empty");
      }
    }
  }

  const CliJsonValue& boundary_state = RequireJsonField(root, "boundary_state", path);
  spec.boundary_state_kind = RequireJsonString(
      RequireJsonField(boundary_state, "kind", path + ".boundary_state"),
      path + ".boundary_state.kind");
  if (spec.boundary_state_kind != "amflow_eta_infinity_asymptotic_with_subsystem_samples" &&
      spec.boundary_state_kind != "amflow_finite_solution_samples") {
    throw std::invalid_argument(
        "solve-series AMFlow state JSON carries unsupported boundary_state.kind: " +
        spec.boundary_state_kind);
  }
  if (spec.boundary_state_kind == "amflow_finite_solution_samples") {
    spec.retained_solution_samples_input = true;
  }
  if (const CliJsonValue* direction = FindJsonField(boundary_state, "direction")) {
    spec.boundary_state_direction =
        StripWrappedQuoteLiteral(
            RequireJsonString(*direction, path + ".boundary_state.direction"));
  }
  if (const CliJsonValue* epsilon_samples = FindJsonField(boundary_state, "epsilon_samples")) {
    spec.boundary_epsilon_samples =
        RequireJsonStringArray(*epsilon_samples, path + ".boundary_state.epsilon_samples");
  }
  if (const CliJsonValue* files = FindJsonField(boundary_state, "files")) {
    spec.boundary_state_raw_files =
        ParseAmflowStateBoundaryRawFiles(*files, path + ".boundary_state.files");
  }

  std::vector<amflow::MasterIntegral> phase_space_output_masters;
  if (const CliJsonValue* phase_space = FindJsonField(root, "phase_space")) {
    RequireJsonObject(*phase_space, path + ".phase_space");
    if (const CliJsonValue* prescription = FindJsonField(*phase_space, "prescription")) {
      spec.phase_space_prescription =
          RequireJsonIntegerArray(*prescription, path + ".phase_space.prescription");
    }
    if (const CliJsonValue* cut = FindJsonField(*phase_space, "cut")) {
      spec.phase_space_cut = RequireJsonIntegerArray(*cut, path + ".phase_space.cut");
    }
    if (const CliJsonValue* output_masters = FindJsonField(*phase_space, "output_masters")) {
      phase_space_output_masters =
          ParseAmflowStateMasters(*output_masters, path + ".phase_space.output_masters");
    }
    if (spec.integral_kind.empty()) {
      spec.integral_kind = "phase_space";
    }
    spec.retained_solution_samples_input = true;
  }
  if (!spec.integral_kind.empty() && spec.integral_kind != "loop" &&
      spec.integral_kind != "phase_space") {
    throw std::invalid_argument("solve-series AMFlow state JSON carries unsupported "
                                "integral_kind: " +
                                spec.integral_kind);
  }
  if (spec.integral_kind == "loop" &&
      (!spec.phase_space_cut.empty() || !spec.phase_space_prescription.empty() ||
       !phase_space_output_masters.empty())) {
    throw std::invalid_argument(
        "solve-series AMFlow state JSON integral_kind loop cannot include phase_space "
        "metadata");
  }
  if (spec.integral_kind == "phase_space" && spec.phase_space_cut.empty()) {
    throw std::invalid_argument(
        "solve-series phase_space AMFlow state JSON must include phase_space.cut");
  }
  if (spec.integral_kind == "phase_space" &&
      spec.phase_space_prescription.empty()) {
    throw std::invalid_argument(
        "solve-series phase_space AMFlow state JSON must include phase_space.prescription");
  }
  for (const int prescription : spec.phase_space_prescription) {
    if (prescription != -1 && prescription != 0 && prescription != 1) {
      throw std::invalid_argument(
          "solve-series phase_space.prescription entries must be -1, 0, or 1");
    }
  }
  for (const int cut : spec.phase_space_cut) {
    if (cut != 0 && cut != 1) {
      throw std::invalid_argument("solve-series phase_space.cut entries must be 0 or 1");
    }
  }
  if (spec.integral_kind == "phase_space") {
    for (const amflow::MasterIntegral& master : spec.masters) {
      if (master.indices.size() != spec.phase_space_cut.size()) {
        throw std::invalid_argument(
            "solve-series phase_space.cut length must match phase-space master index width");
      }
    }
  }
  if (spec.retained_solution_samples_input &&
      spec.boundary_state_raw_files.find("solution") ==
          spec.boundary_state_raw_files.end()) {
    throw std::invalid_argument(
        "solve-series retained solution-sample AMFlow state JSON must include "
        "boundary_state.files.solution");
  }

  std::vector<amflow::TargetIntegral> finite_output_targets;
  if (const CliJsonValue* finite_start = FindJsonField(root, "finite_start")) {
    RequireJsonObject(*finite_start, path + ".finite_start");
    spec.finite_source_variable =
        OptionalJsonStringField(*finite_start, "source_variable", "");
    spec.finite_solution_basis_reduction_path =
        OptionalJsonStringField(*finite_start, "solution_basis_reduction_path", "");
    if (const CliJsonValue* output_integrals =
            FindJsonField(*finite_start, "output_integrals")) {
      finite_output_targets =
          ParseAmflowStateTargets(*output_integrals,
                                  path + ".finite_start.output_integrals");
    }
  }

  if (const CliJsonValue* reduction = FindJsonField(root, "reduction")) {
    if (const CliJsonValue* masters = FindJsonField(*reduction, "masters")) {
      spec.retained_reduction_masters =
          ParseAmflowStateMasters(*masters, path + ".reduction.masters");
    }
    if (const CliJsonValue* target_reduction_path =
            FindJsonField(*reduction, "target_reduction_path")) {
      spec.target_reduction_path =
          RequireJsonString(*target_reduction_path,
                            path + ".reduction.target_reduction_path");
    }
    if (const CliJsonValue* targets = FindJsonField(*reduction, "targets")) {
      spec.targets = ParseAmflowStateTargets(*targets, path + ".reduction.targets");
    }
  }
  for (const amflow::MasterIntegral& master : phase_space_output_masters) {
    AppendUniqueMaster(spec.retained_reduction_masters, master);
  }
  if (spec.targets.empty()) {
    if (!finite_output_targets.empty()) {
      spec.targets = std::move(finite_output_targets);
    } else {
      for (const amflow::MasterIntegral& master : spec.masters) {
        spec.targets.push_back({master.family, master.indices});
      }
    }
  }
  return spec;
}

std::vector<DirectSolveSeriesSpec> ParseAmflowSolveSeriesStateBundleJsonRoot(
    const CliJsonValue& root,
    const std::string& path) {
  RequireJsonObject(root, path);
  const std::string kind =
      RequireJsonString(RequireJsonField(root, "kind", path), path + ".kind");
  if (kind != "amflow_solve_series_state_bundle") {
    throw std::invalid_argument(
        "solve-series JSON bundle input must have kind "
        "\"amflow_solve_series_state_bundle\"");
  }
  if (RequireJsonInteger(RequireJsonField(root, "schema_version", path),
                         path + ".schema_version") != 1) {
    throw std::invalid_argument(
        "solve-series AMFlow state bundle JSON schema_version must be 1");
  }

  const CliJsonValue& states = RequireJsonArray(RequireJsonField(root, "states", path),
                                                path + ".states");
  if (states.array.empty()) {
    throw std::invalid_argument(
        "solve-series AMFlow state bundle JSON must contain at least one state");
  }
  std::vector<DirectSolveSeriesSpec> specs;
  specs.reserve(states.array.size());
  for (std::size_t index = 0; index < states.array.size(); ++index) {
    specs.push_back(ParseAmflowSolveSeriesStateJsonRoot(
        states.array[index], path + ".states[" + std::to_string(index) + "]"));
  }
  return specs;
}

struct ParsedSolveSeriesJsonInput {
  std::vector<DirectSolveSeriesSpec> specs;
  bool bundle = false;
};

std::filesystem::path ResolveSolveSeriesJsonReferencePath(
    const std::filesystem::path& source_path,
    const std::string& raw_reference) {
  if (raw_reference.empty()) {
    throw std::runtime_error("$.cpp_solve_series_input must not be empty");
  }
  const std::filesystem::path reference_path(raw_reference);
  if (reference_path.is_absolute()) {
    return reference_path;
  }
  return source_path.parent_path() / reference_path;
}

ParsedSolveSeriesJsonInput ParseSolveSeriesJsonInputRoot(
    const CliJsonValue& root,
    const std::string& path,
    const std::filesystem::path& source_path) {
  RequireJsonObject(root, path);
  const CliJsonValue* kind_field = FindJsonField(root, "kind");
  if (kind_field == nullptr) {
    const CliJsonValue* referenced_input = FindJsonField(root, "cpp_solve_series_input");
    if (referenced_input == nullptr) {
      throw std::runtime_error(path + ".kind is required");
    }
    const std::filesystem::path referenced_path =
        ResolveSolveSeriesJsonReferencePath(
            source_path,
            RequireJsonString(*referenced_input, path + ".cpp_solve_series_input"));
    const std::string referenced_raw = ReadTextFile(referenced_path);
    if (!LooksLikeJsonObject(referenced_raw)) {
      throw std::runtime_error(
          "solve-series cpp_solve_series_input must point to a JSON object: " +
          referenced_path.string());
    }
    const CliJsonValue referenced_root = CliJsonParser(referenced_raw).Parse();
    return ParseSolveSeriesJsonInputRoot(referenced_root, "$", referenced_path);
  }

  const std::string kind = RequireJsonString(*kind_field, path + ".kind");
  if (kind == "amflow_solve_series_state_bundle") {
    return {ParseAmflowSolveSeriesStateBundleJsonRoot(root, path), true};
  }
  if (kind == "amflow_solve_series_state") {
    return {{ParseAmflowSolveSeriesStateJsonRoot(root, path)}, false};
  }
  throw std::invalid_argument(
      "solve-series JSON input must have kind \"amflow_solve_series_state\" or "
      "\"amflow_solve_series_state_bundle\"");
}

bool IsAutomaticVsManualSmokeProblemSpec(const amflow::ProblemSpec& problem_spec) {
  return problem_spec.family.name == "automatic_vs_manual_k0_smoke";
}

DirectSolveSeriesSpec LoadAutomaticVsManualAmflowStateFallback(
    const std::filesystem::path& source_spec_path) {
  const std::filesystem::path repo_root = FindRepositoryRoot(source_spec_path);
  if (repo_root.empty()) {
    throw std::runtime_error(
        "automatic_vs_manual solve-series fallback requires a repository-local spec path");
  }
  const std::filesystem::path state_path =
      repo_root / "tools/reference-harness/specs/phase0/"
                  "automatic_vs_manual.amflow-state.json";
  const CliJsonValue root = CliJsonParser(ReadTextFile(state_path)).Parse();
  return ParseAmflowSolveSeriesStateJsonRoot(root, "$");
}

std::optional<std::string> ExtractMathematicaRuleValue(const std::string& raw,
                                                       const std::string& quoted_key) {
  const std::size_t key_position = raw.find(quoted_key);
  if (key_position == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t arrow = raw.find("->", key_position + quoted_key.size());
  if (arrow == std::string::npos) {
    return std::nullopt;
  }
  std::size_t begin = arrow + 2;
  while (begin < raw.size() &&
         std::isspace(static_cast<unsigned char>(raw[begin])) != 0) {
    ++begin;
  }
  if (begin >= raw.size()) {
    return std::nullopt;
  }

  int depth = 0;
  bool in_string = false;
  bool escaping = false;
  for (std::size_t index = begin; index < raw.size(); ++index) {
    const char character = raw[index];
    if (escaping) {
      escaping = false;
      continue;
    }
    if (character == '\\') {
      escaping = true;
      continue;
    }
    if (character == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (character == '{' || character == '[' || character == '(') {
      ++depth;
      continue;
    }
    if (character == '}' || character == ']' || character == ')') {
      --depth;
      if (depth < 0) {
        return raw.substr(begin, index - begin);
      }
      if (depth == 0 && raw[begin] == '{') {
        return raw.substr(begin, index - begin + 1);
      }
      continue;
    }
    if (character == ',' && depth == 0) {
      return raw.substr(begin, index - begin);
    }
  }
  return raw.substr(begin);
}

std::vector<std::string> SplitMathematicaTopLevelItems(const std::string& raw) {
  std::string value = TrimAsciiWhitespace(raw);
  if (value.size() >= 2 && value.front() == '{' && value.back() == '}') {
    value = value.substr(1, value.size() - 2);
  }

  std::vector<std::string> items;
  std::size_t start = 0;
  int depth = 0;
  bool in_string = false;
  bool escaping = false;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char character = value[index];
    if (escaping) {
      escaping = false;
      continue;
    }
    if (character == '\\') {
      escaping = true;
      continue;
    }
    if (character == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (character == '{' || character == '[' || character == '(') {
      ++depth;
      continue;
    }
    if (character == '}' || character == ']' || character == ')') {
      --depth;
      continue;
    }
    if (character == ',' && depth == 0) {
      items.push_back(TrimAsciiWhitespace(value.substr(start, index - start)));
      start = index + 1;
    }
  }
  items.push_back(TrimAsciiWhitespace(value.substr(start)));
  items.erase(std::remove_if(items.begin(),
                             items.end(),
                             [](const std::string& item) { return item.empty(); }),
              items.end());
  return items;
}

std::vector<amflow::Propagator> ParseAmflowConfigPropagators(
    const std::string& amflow_config_raw) {
  std::vector<amflow::Propagator> propagators;
  const std::optional<std::string> raw_propagators =
      ExtractMathematicaRuleValue(amflow_config_raw, "\"Propagator\"");
  if (!raw_propagators.has_value()) {
    return propagators;
  }
  for (const std::string& expression : SplitMathematicaTopLevelItems(*raw_propagators)) {
    propagators.push_back(amflow::Propagator(expression));
  }
  return propagators;
}

int SectorMaskFromIntegralIndices(const std::vector<int>& indices) {
  int sector = 0;
  const std::size_t width =
      std::min<std::size_t>(indices.size(), static_cast<std::size_t>(std::numeric_limits<int>::digits));
  for (std::size_t index = 0; index < width; ++index) {
    if (indices[index] > 0) {
      sector |= (1 << index);
    }
  }
  return sector;
}

int DeriveTopLevelSectorMask(const DirectSolveSeriesSpec& direct_spec) {
  int sector = 0;
  for (const amflow::TargetIntegral& target : direct_spec.targets) {
    sector |= SectorMaskFromIntegralIndices(target.indices);
  }
  for (const amflow::MasterIntegral& master : direct_spec.retained_reduction_masters) {
    sector |= SectorMaskFromIntegralIndices(master.indices);
  }
  for (const amflow::MasterIntegral& master : direct_spec.masters) {
    sector |= SectorMaskFromIntegralIndices(master.indices);
  }
  return sector;
}

std::vector<int> ExtractSignedIntegers(const std::string& raw) {
  std::vector<int> values;
  for (std::size_t index = 0; index < raw.size();) {
    bool negative = false;
    if (raw[index] == '+' || raw[index] == '-') {
      negative = raw[index] == '-';
      ++index;
    }
    if (index >= raw.size() ||
        std::isdigit(static_cast<unsigned char>(raw[index])) == 0) {
      ++index;
      continue;
    }
    int value = 0;
    while (index < raw.size() &&
           std::isdigit(static_cast<unsigned char>(raw[index])) != 0) {
      value = value * 10 + (raw[index] - '0');
      ++index;
    }
    values.push_back(negative ? -value : value);
  }
  return values;
}

int SectorMaskFromOneBasedPositions(const std::vector<int>& positions) {
  int sector = 0;
  for (const int position : positions) {
    if (position <= 0 || position > std::numeric_limits<int>::digits) {
      continue;
    }
    sector |= (1 << (position - 1));
  }
  return sector;
}

amflow::ProblemSpec MakeProblemSpecForAmflowState(const DirectSolveSeriesSpec& direct_spec) {
  amflow::ProblemSpec problem_spec;
  problem_spec.family.name = direct_spec.family;
  problem_spec.family.propagators =
      ParseAmflowConfigPropagators(direct_spec.amflow_config_raw);
  int top_level_sector = DeriveTopLevelSectorMask(direct_spec);
  if (direct_spec.benchmark_id == "user_defined_ending") {
    const auto subsystem = direct_spec.boundary_state_raw_files.find("subsystem");
    if (subsystem != direct_spec.boundary_state_raw_files.end()) {
      const int retained_terminal_sector =
          SectorMaskFromOneBasedPositions(ExtractSignedIntegers(subsystem->second));
      if (retained_terminal_sector != 0) {
        top_level_sector = retained_terminal_sector;
      }
    }
  }
  if (top_level_sector != 0) {
    problem_spec.family.top_level_sectors = {top_level_sector};
  }
  problem_spec.targets = direct_spec.targets;
  problem_spec.dimension = "4 - 2*eps";
  problem_spec.complex_mode = true;
  problem_spec.notes =
      "generated in-memory from amflow_solve_series_state for solve-series ingestion";
  return problem_spec;
}

using BigFloat = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<300>>;
constexpr int kSerializedBigFloatDecimalDigits = 180;

struct BigComplex {
  BigFloat real = 0;
  BigFloat imaginary = 0;
};

BigComplex operator+(const BigComplex& lhs, const BigComplex& rhs) {
  return {lhs.real + rhs.real, lhs.imaginary + rhs.imaginary};
}

BigComplex operator-(const BigComplex& lhs, const BigComplex& rhs) {
  return {lhs.real - rhs.real, lhs.imaginary - rhs.imaginary};
}

BigComplex operator*(const BigComplex& lhs, const BigComplex& rhs) {
  return {lhs.real * rhs.real - lhs.imaginary * rhs.imaginary,
          lhs.real * rhs.imaginary + lhs.imaginary * rhs.real};
}

BigComplex operator*(const BigComplex& lhs, const BigFloat& rhs) {
  return {lhs.real * rhs, lhs.imaginary * rhs};
}

BigComplex operator/(const BigComplex& lhs, const BigFloat& rhs) {
  return {lhs.real / rhs, lhs.imaginary / rhs};
}

BigFloat BigAbs(const BigComplex& value) {
  return sqrt(value.real * value.real + value.imaginary * value.imaginary);
}

bool IsTiny(const BigFloat& value) {
  return abs(value) < BigFloat("1e-120");
}

bool IsTiny(const BigComplex& value) {
  return IsTiny(value.real) && IsTiny(value.imaginary);
}

bool NearlyEqual(const BigFloat& lhs, const BigFloat& rhs) {
  return abs(lhs - rhs) < BigFloat("1e-60");
}

std::string RequireAmflowBoundaryRawFile(const DirectSolveSeriesSpec& spec,
                                         const std::string& name) {
  const auto it = spec.boundary_state_raw_files.find(name);
  if (it == spec.boundary_state_raw_files.end() || it->second.empty()) {
    throw std::runtime_error("AMFlow solve-series boundary state is missing raw file " +
                             name);
  }
  return it->second;
}

std::vector<std::string> SplitTopLevelMath(const std::string& value,
                                           const char separator = ',') {
  std::vector<std::string> parts;
  int depth = 0;
  bool in_string = false;
  bool escaping = false;
  std::size_t start = 0;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char character = value[index];
    if (escaping) {
      escaping = false;
      continue;
    }
    if (character == '\\' && in_string) {
      escaping = true;
      continue;
    }
    if (character == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (character == '(' || character == '[' || character == '{') {
      ++depth;
      continue;
    }
    if (character == ')' || character == ']' || character == '}') {
      --depth;
      continue;
    }
    if (character == separator && depth == 0) {
      const std::string part = TrimAsciiWhitespace(value.substr(start, index - start));
      if (!part.empty()) {
        parts.push_back(part);
      }
      start = index + 1;
    }
  }
  const std::string tail = TrimAsciiWhitespace(value.substr(start));
  if (!tail.empty()) {
    parts.push_back(tail);
  }
  return parts;
}

std::string StripOuterMathBraces(const std::string& raw, const std::string& path) {
  const std::string value = TrimAsciiWhitespace(raw);
  if (value.size() < 2 || value.front() != '{' || value.back() != '}') {
    throw std::runtime_error(path + " must be a Mathematica list");
  }
  return TrimAsciiWhitespace(value.substr(1, value.size() - 2));
}

std::vector<std::string> SplitMathList(const std::string& raw, const std::string& path) {
  const std::string body = StripOuterMathBraces(raw, path);
  if (body.empty()) {
    return {};
  }
  return SplitTopLevelMath(body);
}

std::string RemoveAsciiSpaces(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](const unsigned char character) {
                               return std::isspace(character) != 0;
                             }),
              value.end());
  return value;
}

amflow::MasterIntegral ParseMathJIntegral(const std::string& raw,
                                          const std::string& path) {
  const std::string compact = RemoveAsciiSpaces(raw);
  if (compact.rfind("j[", 0) != 0 || compact.back() != ']') {
    throw std::runtime_error(path + " must be a j[family,...] integral");
  }
  const std::string body = compact.substr(2, compact.size() - 3);
  const std::vector<std::string> fields = SplitTopLevelMath(body);
  if (fields.size() < 2) {
    throw std::runtime_error(path + " must carry a family and at least one index");
  }
  amflow::MasterIntegral integral;
  integral.family = fields.front();
  for (std::size_t index = 1; index < fields.size(); ++index) {
    std::size_t consumed = 0;
    int parsed = 0;
    try {
      parsed = std::stoi(fields[index], &consumed);
    } catch (const std::exception&) {
      throw std::runtime_error(path + " carries a non-integer integral index");
    }
    if (consumed != fields[index].size()) {
      throw std::runtime_error(path + " carries a non-integer integral index");
    }
    integral.indices.push_back(parsed);
  }
  return integral;
}

std::vector<amflow::MasterIntegral> ParseMathJIntegralList(const std::string& raw,
                                                           const std::string& path) {
  std::vector<amflow::MasterIntegral> integrals;
  const std::vector<std::string> fields = SplitMathList(raw, path);
  integrals.reserve(fields.size());
  for (std::size_t index = 0; index < fields.size(); ++index) {
    integrals.push_back(ParseMathJIntegral(fields[index],
                                           path + "[" + std::to_string(index) + "]"));
  }
  return integrals;
}

struct ParsedAmflowBoundaryRegion {
  std::vector<std::string> powers;
  std::vector<amflow::MasterIntegral> local_masters;
  std::vector<std::vector<std::vector<std::string>>> coefficient_table;
};

std::vector<std::vector<std::vector<std::string>>> ParseBoundaryCoefficientTable(
    const std::string& raw,
    const std::size_t master_count,
    const std::size_t local_master_count,
    const std::string& path) {
  const std::vector<std::string> master_rows = SplitMathList(raw, path);
  if (master_rows.size() != master_count) {
    throw std::runtime_error(path + " row count does not match the top-level master count");
  }

  std::vector<std::vector<std::vector<std::string>>> table;
  table.reserve(master_rows.size());
  for (std::size_t master_index = 0; master_index < master_rows.size(); ++master_index) {
    const std::vector<std::string> raw_orders =
        SplitMathList(master_rows[master_index],
                      path + "[" + std::to_string(master_index) + "]");
    std::vector<std::vector<std::string>> orders;
    orders.reserve(raw_orders.size());
    for (std::size_t order_index = 0; order_index < raw_orders.size(); ++order_index) {
      std::vector<std::string> coefficients =
          SplitMathList(raw_orders[order_index],
                        path + "[" + std::to_string(master_index) + "][" +
                            std::to_string(order_index) + "]");
      if (coefficients.size() != local_master_count) {
        throw std::runtime_error(path + " coefficient-vector width does not match local "
                                 "boundary master count");
      }
      orders.push_back(std::move(coefficients));
    }
    table.push_back(std::move(orders));
  }
  return table;
}

std::vector<ParsedAmflowBoundaryRegion> ParseAmflowBoundaryRegions(
    const std::string& raw,
    const std::size_t master_count) {
  const std::vector<std::string> entries = SplitMathList(raw, "boundary_state.files.boundary.raw");
  if (entries.empty()) {
    throw std::runtime_error("AMFlow boundary file did not contain any eta-infinity regions");
  }

  std::vector<ParsedAmflowBoundaryRegion> regions;
  regions.reserve(entries.size());
  for (std::size_t region_index = 0; region_index < entries.size(); ++region_index) {
    const std::string path = "boundary_state.files.boundary.raw[" +
                             std::to_string(region_index) + "]";
    const std::vector<std::string> fields = SplitMathList(entries[region_index], path);
    if (fields.size() != 4) {
      throw std::runtime_error(path + " must have {powers, config, masters, table}");
    }

    ParsedAmflowBoundaryRegion region;
    region.powers = SplitMathList(fields[0], path + ".powers");
    if (region.powers.size() != master_count) {
      throw std::runtime_error(path + ".powers count does not match top-level masters");
    }
    region.local_masters = ParseMathJIntegralList(fields[2], path + ".masters");
    region.coefficient_table =
        ParseBoundaryCoefficientTable(fields[3],
                                      master_count,
                                      region.local_masters.size(),
                                      path + ".table");
    regions.push_back(std::move(region));
  }
  return regions;
}

std::size_t FindTopLevelArrow(const std::string& value) {
  int depth = 0;
  bool in_string = false;
  bool escaping = false;
  for (std::size_t index = 0; index + 1 < value.size(); ++index) {
    const char character = value[index];
    if (escaping) {
      escaping = false;
      continue;
    }
    if (character == '\\' && in_string) {
      escaping = true;
      continue;
    }
    if (character == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (character == '(' || character == '[' || character == '{') {
      ++depth;
      continue;
    }
    if (character == ')' || character == ']' || character == '}') {
      --depth;
      continue;
    }
    if (character == '-' && value[index + 1] == '>' && depth == 0) {
      return index;
    }
  }
  return std::string::npos;
}

BigFloat ParseBigFloatRational(const std::string& raw) {
  const std::string value = RemoveAsciiSpaces(raw);
  const std::size_t slash = value.find('/');
  if (slash == std::string::npos) {
    return BigFloat(value);
  }
  return BigFloat(value.substr(0, slash)) / BigFloat(value.substr(slash + 1));
}

std::string NormalizeMathematicaNumericAtom(std::string value) {
  value = RemoveAsciiSpaces(std::move(value));
  for (std::size_t tick = value.find('`'); tick != std::string::npos;
       tick = value.find('`')) {
    std::size_t end = tick + 1;
    while (end < value.size() &&
           (std::isdigit(static_cast<unsigned char>(value[end])) != 0 ||
            value[end] == '.')) {
      ++end;
    }
    value.erase(tick, end - tick);
  }
  for (std::size_t power = value.find("*^"); power != std::string::npos;
       power = value.find("*^", power + 1)) {
    value.replace(power, 2, "e");
  }
  if (!value.empty() && value.back() == '.') {
    value.push_back('0');
  }
  return value;
}

class RealBoundaryExpressionParser {
 public:
  explicit RealBoundaryExpressionParser(std::string expression_in)
      : expression_(NormalizeMathematicaNumericAtom(std::move(expression_in))) {}

  BigFloat Parse() {
    if (expression_.empty() || expression_ == "+") {
      return BigFloat(1);
    }
    if (expression_ == "-") {
      return BigFloat(-1);
    }
    const BigFloat value = ParseExpression();
    if (position_ != expression_.size()) {
      throw std::runtime_error("unsupported retained boundary expression near: " +
                               expression_.substr(position_));
    }
    return value;
  }

 private:
  static BigFloat PowIntegerValue(BigFloat base, int exponent) {
    if (exponent == 0) {
      return BigFloat(1);
    }
    bool invert = false;
    if (exponent < 0) {
      invert = true;
      exponent = -exponent;
    }
    BigFloat result = 1;
    while (exponent > 0) {
      if ((exponent & 1) != 0) {
        result *= base;
      }
      base *= base;
      exponent >>= 1;
    }
    return invert ? BigFloat(1) / result : result;
  }

  bool Consume(const char expected) {
    if (position_ < expression_.size() && expression_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  bool StartsWithAtPosition(const std::string& prefix) const {
    return expression_.compare(position_, prefix.size(), prefix) == 0;
  }

  BigFloat ParseExpression() {
    BigFloat value = ParseTerm();
    while (position_ < expression_.size()) {
      if (Consume('+')) {
        value += ParseTerm();
      } else if (Consume('-')) {
        value -= ParseTerm();
      } else {
        break;
      }
    }
    return value;
  }

  bool StartsImplicitFactor() const {
    if (position_ >= expression_.size()) {
      return false;
    }
    const char character = expression_[position_];
    return character == '(' || character == '[' ||
           std::isdigit(static_cast<unsigned char>(character)) != 0 ||
           character == '.' || std::isalpha(static_cast<unsigned char>(character)) != 0;
  }

  BigFloat ParseTerm() {
    BigFloat value = ParsePower();
    while (position_ < expression_.size()) {
      if (Consume('*')) {
        value *= ParsePower();
      } else if (Consume('/')) {
        value /= ParsePower();
      } else if (StartsImplicitFactor()) {
        value *= ParsePower();
      } else {
        break;
      }
    }
    return value;
  }

  BigFloat ParsePower() {
    BigFloat value = ParseUnary();
    if (Consume('^')) {
      value = PowIntegerValue(value, ParseSignedIntegerExponent());
    }
    return value;
  }

  BigFloat ParseUnary() {
    if (Consume('+')) {
      return ParseUnary();
    }
    if (Consume('-')) {
      return -ParseUnary();
    }
    return ParsePrimary();
  }

  int ParseSignedIntegerExponent() {
    bool parenthesized = false;
    if (Consume('(')) {
      parenthesized = true;
    }
    bool negative = false;
    if (Consume('+')) {
      negative = false;
    } else if (Consume('-')) {
      negative = true;
    }
    if (position_ >= expression_.size() ||
        std::isdigit(static_cast<unsigned char>(expression_[position_])) == 0) {
      throw std::runtime_error("retained boundary power expects an integer exponent");
    }
    int exponent = 0;
    while (position_ < expression_.size() &&
           std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
      exponent = exponent * 10 + (expression_[position_] - '0');
      ++position_;
    }
    if (parenthesized && !Consume(')')) {
      throw std::runtime_error("retained boundary power exponent is missing ')'");
    }
    return negative ? -exponent : exponent;
  }

  BigFloat ParsePrimary() {
    if (Consume('(')) {
      const BigFloat value = ParseExpression();
      if (!Consume(')')) {
        throw std::runtime_error("retained boundary expression is missing ')'");
      }
      return value;
    }
    if (StartsWithAtPosition("Gamma[")) {
      position_ += 6;
      const BigFloat argument = ParseExpression();
      if (!Consume(']')) {
        throw std::runtime_error("retained boundary Gamma expression is missing ']'");
      }
      return boost::math::tgamma(argument);
    }
    if (StartsWithAtPosition("Pi")) {
      position_ += 2;
      return boost::math::constants::pi<BigFloat>();
    }
    return ParseNumber();
  }

  BigFloat ParseNumber() {
    const std::size_t begin = position_;
    bool saw_digit = false;
    while (position_ < expression_.size() &&
           std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
      saw_digit = true;
      ++position_;
    }
    if (Consume('.')) {
      while (position_ < expression_.size() &&
             std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
        saw_digit = true;
        ++position_;
      }
    }
    if (!saw_digit) {
      throw std::runtime_error("unsupported retained boundary expression near: " +
                               expression_.substr(begin));
    }
    if (position_ < expression_.size() &&
        (expression_[position_] == 'e' || expression_[position_] == 'E')) {
      ++position_;
      if (position_ < expression_.size() &&
          (expression_[position_] == '+' || expression_[position_] == '-')) {
        ++position_;
      }
      if (position_ >= expression_.size() ||
          std::isdigit(static_cast<unsigned char>(expression_[position_])) == 0) {
        throw std::runtime_error("retained boundary scientific notation exponent is invalid");
      }
      while (position_ < expression_.size() &&
             std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
        ++position_;
      }
    }
    return BigFloat(expression_.substr(begin, position_ - begin));
  }

  std::string expression_;
  std::size_t position_ = 0;
};

BigFloat ParseRealBoundaryAtom(const std::string& raw) {
  return RealBoundaryExpressionParser(raw).Parse();
}

std::vector<std::string> SplitTopLevelTerms(const std::string& raw) {
  const std::string expression = RemoveAsciiSpaces(raw);
  std::vector<std::string> terms;
  int depth = 0;
  std::size_t start = 0;
  for (std::size_t index = 0; index < expression.size(); ++index) {
    const char character = expression[index];
    if (character == '(' || character == '[' || character == '{') {
      ++depth;
      continue;
    }
    if (character == ')' || character == ']' || character == '}') {
      --depth;
      continue;
    }
    const char previous = index == 0 ? '\0' : expression[index - 1];
    if ((character == '+' || character == '-') && depth == 0 && index != 0 &&
        previous != '^' && previous != 'e' && previous != 'E') {
      terms.push_back(expression.substr(start, index - start));
      start = index;
    }
  }
  terms.push_back(expression.substr(start));
  terms.erase(std::remove_if(terms.begin(),
                             terms.end(),
                             [](const std::string& value) { return value.empty(); }),
              terms.end());
  return terms;
}

BigComplex ParseBoundaryComplexValue(const std::string& raw) {
  BigComplex value;
  for (std::string term : SplitTopLevelTerms(raw)) {
    if (term == "I" || term == "+I") {
      value.imaginary += 1;
      continue;
    }
    if (term == "-I") {
      value.imaginary -= 1;
      continue;
    }
    if (term.size() > 2 && term.substr(term.size() - 2) == "*I") {
      value.imaginary += ParseRealBoundaryAtom(term.substr(0, term.size() - 2));
      continue;
    }
    value.real += ParseRealBoundaryAtom(term);
  }
  return value;
}

std::vector<std::vector<std::vector<BigComplex>>> ParseBoundaryMiSamples(
    const std::string& raw,
    const std::vector<ParsedAmflowBoundaryRegion>& regions,
    const std::size_t sample_count) {
  const std::vector<std::string> entries =
      SplitMathList(raw, "boundary_state.files.boundarymi.raw");
  if (entries.size() != regions.size()) {
    throw std::runtime_error("AMFlow boundarymi region count does not match boundary regions");
  }

  std::vector<std::vector<std::vector<BigComplex>>> samples;
  samples.reserve(entries.size());
  for (std::size_t region_index = 0; region_index < entries.size(); ++region_index) {
    const std::string path = "boundary_state.files.boundarymi.raw[" +
                             std::to_string(region_index) + "]";
    const std::vector<std::string> rules = SplitMathList(entries[region_index], path);
    if (rules.size() != regions[region_index].local_masters.size()) {
      throw std::runtime_error(path + " rule count does not match local boundary masters");
    }

    std::vector<std::vector<BigComplex>> region_samples;
    region_samples.reserve(rules.size());
    for (std::size_t rule_index = 0; rule_index < rules.size(); ++rule_index) {
      const std::size_t arrow = FindTopLevelArrow(rules[rule_index]);
      if (arrow == std::string::npos) {
        throw std::runtime_error(path + " rule is missing ->");
      }
      const amflow::MasterIntegral lhs =
          ParseMathJIntegral(rules[rule_index].substr(0, arrow),
                             path + "[" + std::to_string(rule_index) + "].lhs");
      if (IntegralLabel(lhs.family, lhs.indices) !=
          IntegralLabel(regions[region_index].local_masters[rule_index].family,
                        regions[region_index].local_masters[rule_index].indices)) {
        throw std::runtime_error(path + " local master order does not match boundary table");
      }

      const std::vector<std::string> raw_values =
          SplitMathList(rules[rule_index].substr(arrow + 2),
                        path + "[" + std::to_string(rule_index) + "].rhs");
      if (raw_values.size() != sample_count) {
        throw std::runtime_error(path + " sample count does not match epslist");
      }
      std::vector<BigComplex> parsed_values;
      parsed_values.reserve(raw_values.size());
      for (const std::string& raw_value : raw_values) {
        parsed_values.push_back(ParseBoundaryComplexValue(raw_value));
      }
      region_samples.push_back(std::move(parsed_values));
    }
    samples.push_back(std::move(region_samples));
  }
  return samples;
}

std::map<std::string, std::vector<BigComplex>> ParseMathIntegralSampleRules(
    const std::string& raw,
    const std::size_t sample_count,
    const std::string& path) {
  const std::vector<std::string> rules = SplitMathList(raw, path);
  std::map<std::string, std::vector<BigComplex>> samples_by_label;
  for (std::size_t rule_index = 0; rule_index < rules.size(); ++rule_index) {
    const std::string rule_path = path + "[" + std::to_string(rule_index) + "]";
    const std::size_t arrow = FindTopLevelArrow(rules[rule_index]);
    if (arrow == std::string::npos) {
      throw std::runtime_error(rule_path + " rule is missing ->");
    }
    const amflow::MasterIntegral lhs =
        ParseMathJIntegral(rules[rule_index].substr(0, arrow), rule_path + ".lhs");
    const std::string label = IntegralLabel(lhs.family, lhs.indices);
    const std::vector<std::string> raw_values =
        SplitMathList(rules[rule_index].substr(arrow + 2), rule_path + ".rhs");
    if (raw_values.size() != sample_count) {
      throw std::runtime_error(rule_path + " sample count does not match epslist");
    }
    std::vector<BigComplex> parsed_values;
    parsed_values.reserve(raw_values.size());
    for (const std::string& raw_value : raw_values) {
      parsed_values.push_back(ParseBoundaryComplexValue(raw_value));
    }
    if (!samples_by_label.emplace(label, std::move(parsed_values)).second) {
      throw std::runtime_error(path + " contains duplicate integral sample rule for " +
                               label);
    }
  }
  return samples_by_label;
}

BigFloat EvaluateBoundaryTableCoefficient(const std::string& expression,
                                          const std::string& epsilon_sample) {
  const amflow::ExactRational exact =
      amflow::EvaluateCoefficientExpression(expression, {{"eps", epsilon_sample}});
  return ParseBigFloatRational(exact.ToString());
}

std::vector<std::vector<BigComplex>> EvaluateLeadingBoundarySamples(
    const DirectSolveSeriesSpec& spec,
    const std::vector<ParsedAmflowBoundaryRegion>& regions,
    const std::vector<std::vector<std::vector<BigComplex>>>& boundary_mi_samples) {
  const std::size_t master_count = spec.masters.size();
  const std::size_t sample_count = spec.boundary_epsilon_samples.size();
  std::vector<std::vector<BigComplex>> master_samples(
      master_count, std::vector<BigComplex>(sample_count));

  for (std::size_t region_index = 0; region_index < regions.size(); ++region_index) {
    const ParsedAmflowBoundaryRegion& region = regions[region_index];
    for (std::size_t master_index = 0; master_index < master_count; ++master_index) {
      if (region.coefficient_table[master_index].empty()) {
        continue;
      }
      const std::vector<std::string>& leading_coefficients =
          region.coefficient_table[master_index].front();
      for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        BigComplex contribution;
        for (std::size_t local_index = 0; local_index < leading_coefficients.size();
             ++local_index) {
          const BigFloat coefficient =
              EvaluateBoundaryTableCoefficient(leading_coefficients[local_index],
                                               spec.boundary_epsilon_samples[sample_index]);
          contribution =
              contribution + boundary_mi_samples[region_index][local_index][sample_index] *
                                 coefficient;
        }
        master_samples[master_index][sample_index] =
            master_samples[master_index][sample_index] + contribution;
      }
    }
  }
  return master_samples;
}

std::vector<std::vector<std::vector<BigComplex>>> EvaluateBoundaryRegionContributionSamples(
    const DirectSolveSeriesSpec& spec,
    const std::vector<ParsedAmflowBoundaryRegion>& regions,
    const std::vector<std::vector<std::vector<BigComplex>>>& boundary_mi_samples) {
  const std::size_t master_count = spec.masters.size();
  const std::size_t sample_count = spec.boundary_epsilon_samples.size();
  std::vector<std::vector<std::vector<BigComplex>>> region_contributions(
      regions.size(),
      std::vector<std::vector<BigComplex>>(
          master_count, std::vector<BigComplex>(sample_count)));

  for (std::size_t region_index = 0; region_index < regions.size(); ++region_index) {
    const ParsedAmflowBoundaryRegion& region = regions[region_index];
    for (std::size_t master_index = 0; master_index < master_count; ++master_index) {
      if (region.coefficient_table[master_index].empty()) {
        continue;
      }
      const std::vector<std::string>& leading_coefficients =
          region.coefficient_table[master_index].front();
      for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        BigComplex contribution;
        for (std::size_t local_index = 0; local_index < leading_coefficients.size();
             ++local_index) {
          const BigFloat coefficient =
              EvaluateBoundaryTableCoefficient(leading_coefficients[local_index],
                                               spec.boundary_epsilon_samples[sample_index]);
          contribution =
              contribution + boundary_mi_samples[region_index][local_index][sample_index] *
                                 coefficient;
        }
        region_contributions[region_index][master_index][sample_index] = contribution;
      }
    }
  }

  return region_contributions;
}

struct InfinityEtaSeries {
  std::map<int, BigFloat> coefficients_by_degree;
};

void PruneTinyInfinityEtaTerms(InfinityEtaSeries& series) {
  for (auto it = series.coefficients_by_degree.begin();
       it != series.coefficients_by_degree.end();) {
    if (IsTiny(it->second)) {
      it = series.coefficients_by_degree.erase(it);
    } else {
      ++it;
    }
  }
}

InfinityEtaSeries MakeConstantInfinityEtaSeries(const BigFloat& value) {
  InfinityEtaSeries series;
  if (!IsTiny(value)) {
    series.coefficients_by_degree.emplace(0, value);
  }
  return series;
}

InfinityEtaSeries MakeEtaInfinityEtaSeries() {
  InfinityEtaSeries series;
  series.coefficients_by_degree.emplace(1, BigFloat(1));
  return series;
}

InfinityEtaSeries AddInfinityEtaSeries(InfinityEtaSeries lhs,
                                       const InfinityEtaSeries& rhs,
                                       const BigFloat& sign = BigFloat(1)) {
  for (const auto& [degree, coefficient] : rhs.coefficients_by_degree) {
    lhs.coefficients_by_degree[degree] += sign * coefficient;
  }
  PruneTinyInfinityEtaTerms(lhs);
  return lhs;
}

InfinityEtaSeries MultiplyInfinityEtaSeries(const InfinityEtaSeries& lhs,
                                            const InfinityEtaSeries& rhs) {
  InfinityEtaSeries result;
  for (const auto& [left_degree, left_coefficient] : lhs.coefficients_by_degree) {
    for (const auto& [right_degree, right_coefficient] : rhs.coefficients_by_degree) {
      result.coefficients_by_degree[left_degree + right_degree] +=
          left_coefficient * right_coefficient;
    }
  }
  PruneTinyInfinityEtaTerms(result);
  return result;
}

std::optional<std::pair<int, BigFloat>> LeadingInfinityEtaTerm(
    const InfinityEtaSeries& series) {
  if (series.coefficients_by_degree.empty()) {
    return std::nullopt;
  }
  const auto it = series.coefficients_by_degree.rbegin();
  return std::make_pair(it->first, it->second);
}

InfinityEtaSeries DivideInfinityEtaSeries(const InfinityEtaSeries& numerator,
                                          const InfinityEtaSeries& denominator,
                                          const std::string& expression) {
  const std::optional<std::pair<int, BigFloat>> numerator_leading =
      LeadingInfinityEtaTerm(numerator);
  const std::optional<std::pair<int, BigFloat>> denominator_leading =
      LeadingInfinityEtaTerm(denominator);
  if (!numerator_leading.has_value()) {
    return {};
  }
  if (!denominator_leading.has_value() || IsTiny(denominator_leading->second)) {
    throw std::runtime_error("eta-infinity DE asymptotic transport encountered zero leading "
                             "denominator in \"" +
                             expression + "\"");
  }

  InfinityEtaSeries result;
  result.coefficients_by_degree.emplace(
      numerator_leading->first - denominator_leading->first,
      numerator_leading->second / denominator_leading->second);
  return result;
}

InfinityEtaSeries PowerInfinityEtaSeries(const InfinityEtaSeries& base,
                                         const int exponent,
                                         const std::string& expression) {
  if (exponent < 0) {
    return DivideInfinityEtaSeries(MakeConstantInfinityEtaSeries(BigFloat(1)),
                                   PowerInfinityEtaSeries(base, -exponent, expression),
                                   expression);
  }
  InfinityEtaSeries result = MakeConstantInfinityEtaSeries(BigFloat(1));
  for (int index = 0; index < exponent; ++index) {
    result = MultiplyInfinityEtaSeries(result, base);
  }
  return result;
}

class InfinityEtaSeriesParser {
 public:
  InfinityEtaSeriesParser(std::string expression, BigFloat epsilon_value)
      : expression_(std::move(expression)), epsilon_value_(std::move(epsilon_value)) {}

  InfinityEtaSeries Parse() {
    InfinityEtaSeries value = ParseExpression();
    SkipSpaces();
    if (position_ != expression_.size()) {
      throw std::runtime_error("eta-infinity DE asymptotic parser found trailing input in \"" +
                               expression_ + "\"");
    }
    return value;
  }

 private:
  void SkipSpaces() {
    while (position_ < expression_.size() &&
           std::isspace(static_cast<unsigned char>(expression_[position_])) != 0) {
      ++position_;
    }
  }

  bool Match(const char expected) {
    SkipSpaces();
    if (position_ >= expression_.size() || expression_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  InfinityEtaSeries ParseExpression() {
    InfinityEtaSeries value = ParseTerm();
    while (true) {
      if (Match('+')) {
        value = AddInfinityEtaSeries(std::move(value), ParseTerm());
        continue;
      }
      if (Match('-')) {
        value = AddInfinityEtaSeries(std::move(value), ParseTerm(), BigFloat(-1));
        continue;
      }
      break;
    }
    return value;
  }

  InfinityEtaSeries ParseTerm() {
    InfinityEtaSeries value = ParsePower();
    while (true) {
      if (Match('*')) {
        value = MultiplyInfinityEtaSeries(value, ParsePower());
        continue;
      }
      if (Match('/')) {
        value = DivideInfinityEtaSeries(value, ParsePower(), expression_);
        continue;
      }
      break;
    }
    return value;
  }

  InfinityEtaSeries ParsePower() {
    InfinityEtaSeries value = ParseUnary();
    if (!Match('^')) {
      return value;
    }
    const int exponent = ParseIntegerExponent();
    return PowerInfinityEtaSeries(value, exponent, expression_);
  }

  InfinityEtaSeries ParseUnary() {
    if (Match('+')) {
      return ParseUnary();
    }
    if (Match('-')) {
      return AddInfinityEtaSeries(MakeConstantInfinityEtaSeries(BigFloat(0)),
                                  ParseUnary(),
                                  BigFloat(-1));
    }
    return ParsePrimary();
  }

  int ParseIntegerExponent() {
    SkipSpaces();
    bool negative = false;
    if (position_ < expression_.size() &&
        (expression_[position_] == '+' || expression_[position_] == '-')) {
      negative = expression_[position_] == '-';
      ++position_;
    }
    const std::size_t begin = position_;
    while (position_ < expression_.size() &&
           std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
      ++position_;
    }
    if (begin == position_) {
      throw std::runtime_error("eta-infinity DE asymptotic parser requires integer powers in \"" +
                               expression_ + "\"");
    }
    const int exponent = std::stoi(expression_.substr(begin, position_ - begin));
    return negative ? -exponent : exponent;
  }

  InfinityEtaSeries ParsePrimary() {
    SkipSpaces();
    if (Match('(')) {
      InfinityEtaSeries value = ParseExpression();
      if (!Match(')')) {
        throw std::runtime_error("eta-infinity DE asymptotic parser expected ')' in \"" +
                                 expression_ + "\"");
      }
      return value;
    }

    if (position_ < expression_.size() &&
        std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
      const std::size_t begin = position_;
      while (position_ < expression_.size() &&
             std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
        ++position_;
      }
      return MakeConstantInfinityEtaSeries(
          BigFloat(expression_.substr(begin, position_ - begin)));
    }

    if (position_ < expression_.size() &&
        (std::isalpha(static_cast<unsigned char>(expression_[position_])) != 0 ||
         expression_[position_] == '_')) {
      const std::size_t begin = position_;
      ++position_;
      while (position_ < expression_.size() &&
             (std::isalnum(static_cast<unsigned char>(expression_[position_])) != 0 ||
              expression_[position_] == '_')) {
        ++position_;
      }
      const std::string identifier = expression_.substr(begin, position_ - begin);
      if (identifier == "eta") {
        return MakeEtaInfinityEtaSeries();
      }
      if (identifier == "eps") {
        return MakeConstantInfinityEtaSeries(epsilon_value_);
      }
      throw std::runtime_error("eta-infinity DE asymptotic parser requires a numeric binding for "
                               "symbol \"" +
                               identifier + "\" in \"" + expression_ + "\"");
    }

    throw std::runtime_error("eta-infinity DE asymptotic parser found malformed expression \"" +
                             expression_ + "\"");
  }

  std::string expression_;
  BigFloat epsilon_value_;
  std::size_t position_ = 0;
};

std::optional<std::pair<int, BigFloat>> LeadingInfinityEtaTerm(
    const std::string& expression,
    const BigFloat& epsilon_value) {
  return LeadingInfinityEtaTerm(
      InfinityEtaSeriesParser(expression, epsilon_value).Parse());
}

BigFloat EvaluatePowerExpressionAtEpsilon(const std::string& expression,
                                          const std::string& epsilon_sample) {
  return ParseBigFloatRational(
      amflow::EvaluateCoefficientExpression(expression, {{"eps", epsilon_sample}}).ToString());
}

std::vector<std::string> DistinctDeclaredPowerExpressions(
    const std::vector<ParsedAmflowBoundaryRegion>& regions,
    const std::size_t master_index) {
  std::vector<std::string> powers;
  for (const ParsedAmflowBoundaryRegion& region : regions) {
    if (master_index >= region.powers.size()) {
      continue;
    }
    if (std::find(powers.begin(), powers.end(), region.powers[master_index]) ==
        powers.end()) {
      powers.push_back(region.powers[master_index]);
    }
  }
  return powers;
}

struct AsymptoticSourceCoefficient {
  BigFloat power;
  BigComplex coefficient;
};

std::vector<std::vector<AsymptoticSourceCoefficient>> BuildAsymptoticSourceCoefficients(
    const DirectSolveSeriesSpec& spec,
    const std::vector<ParsedAmflowBoundaryRegion>& regions,
    const std::vector<std::vector<std::vector<BigComplex>>>& region_contributions,
    const std::size_t sample_index) {
  std::vector<std::vector<AsymptoticSourceCoefficient>> source_by_master(spec.masters.size());
  for (std::size_t region_index = 0; region_index < regions.size(); ++region_index) {
    for (std::size_t master_index = 0; master_index < spec.masters.size(); ++master_index) {
      const BigComplex coefficient =
          region_contributions[region_index][master_index][sample_index];
      if (IsTiny(coefficient)) {
        continue;
      }
      source_by_master[master_index].push_back(
          {EvaluatePowerExpressionAtEpsilon(regions[region_index].powers[master_index],
                                            spec.boundary_epsilon_samples[sample_index]),
           coefficient});
    }
  }
  return source_by_master;
}

std::optional<BigComplex> TryComputeAsymptoticTransportCandidate(
    const DirectSolveSeriesSpec& spec,
    const std::vector<std::vector<AsymptoticSourceCoefficient>>& source_by_master,
    const std::size_t target_master_index,
    const std::string& target_power_expression,
    const std::size_t sample_index) {
  const BigFloat epsilon_value =
      ParseBigFloatRational(spec.boundary_epsilon_samples[sample_index]);
  const BigFloat target_power =
      EvaluatePowerExpressionAtEpsilon(target_power_expression,
                                       spec.boundary_epsilon_samples[sample_index]);
  const auto matrix_it = spec.coefficient_matrices.find(spec.variable);
  if (matrix_it == spec.coefficient_matrices.end() ||
      target_master_index >= matrix_it->second.size()) {
    return std::nullopt;
  }

  BigFloat diagonal_residue = 0;
  const std::string& diagonal_expression =
      matrix_it->second[target_master_index][target_master_index];
  const std::optional<std::pair<int, BigFloat>> diagonal_leading =
      LeadingInfinityEtaTerm(diagonal_expression, epsilon_value);
  if (diagonal_leading.has_value()) {
    if (diagonal_leading->first > -1) {
      return std::nullopt;
    }
    if (diagonal_leading->first == -1) {
      diagonal_residue = diagonal_leading->second;
    }
  }

  BigComplex rhs;
  for (std::size_t source_master_index = 0; source_master_index < spec.masters.size();
       ++source_master_index) {
    if (source_master_index == target_master_index ||
        source_master_index >= matrix_it->second[target_master_index].size()) {
      continue;
    }
    const std::optional<std::pair<int, BigFloat>> source_matrix_leading =
        LeadingInfinityEtaTerm(matrix_it->second[target_master_index][source_master_index],
                               epsilon_value);
    if (!source_matrix_leading.has_value()) {
      continue;
    }
    for (const AsymptoticSourceCoefficient& source :
         source_by_master[source_master_index]) {
      if (!NearlyEqual(BigFloat(source_matrix_leading->first) + source.power,
                       target_power - BigFloat(1))) {
        continue;
      }
      rhs = rhs + source.coefficient * source_matrix_leading->second;
    }
  }

  if (IsTiny(rhs)) {
    return BigComplex{};
  }
  const BigFloat denominator = target_power - diagonal_residue;
  if (IsTiny(denominator)) {
    return std::nullopt;
  }
  return rhs / denominator;
}

int ApplyEtaInfinityAsymptoticTransportFromDE(
    const DirectSolveSeriesSpec& spec,
    const std::vector<ParsedAmflowBoundaryRegion>& regions,
    const std::vector<std::vector<std::vector<BigComplex>>>& region_contributions,
    std::vector<std::vector<BigComplex>>& master_samples) {
  try {
    if (regions.empty() || spec.boundary_epsilon_samples.empty()) {
      return 0;
    }

    const std::vector<std::vector<AsymptoticSourceCoefficient>> first_sample_sources =
        BuildAsymptoticSourceCoefficients(spec, regions, region_contributions, 0);
    int transported_count = 0;
    for (std::size_t master_index = 0; master_index < master_samples.size(); ++master_index) {
      const bool already_has_boundary =
          std::any_of(master_samples[master_index].begin(),
                      master_samples[master_index].end(),
                      [](const BigComplex& sample) { return !IsTiny(sample); });
      if (already_has_boundary) {
        continue;
      }

      std::vector<std::string> viable_power_expressions;
      for (const std::string& power_expression :
           DistinctDeclaredPowerExpressions(regions, master_index)) {
        const std::optional<BigComplex> candidate =
            TryComputeAsymptoticTransportCandidate(spec,
                                                   first_sample_sources,
                                                   master_index,
                                                   power_expression,
                                                   0);
        if (candidate.has_value() && !IsTiny(*candidate)) {
          viable_power_expressions.push_back(power_expression);
        }
      }
      if (viable_power_expressions.size() != 1) {
        continue;
      }

      std::vector<BigComplex> transported_samples(spec.boundary_epsilon_samples.size());
      bool all_samples_supported = true;
      bool any_nonzero = false;
      for (std::size_t sample_index = 0;
           sample_index < spec.boundary_epsilon_samples.size();
           ++sample_index) {
        const std::vector<std::vector<AsymptoticSourceCoefficient>> sources =
            sample_index == 0
                ? first_sample_sources
                : BuildAsymptoticSourceCoefficients(
                      spec, regions, region_contributions, sample_index);
        const std::optional<BigComplex> candidate =
            TryComputeAsymptoticTransportCandidate(spec,
                                                   sources,
                                                   master_index,
                                                   viable_power_expressions.front(),
                                                   sample_index);
        if (!candidate.has_value()) {
          all_samples_supported = false;
          break;
        }
        transported_samples[sample_index] = *candidate;
        any_nonzero = any_nonzero || !IsTiny(*candidate);
      }
      if (!all_samples_supported || !any_nonzero) {
        continue;
      }

      master_samples[master_index] = std::move(transported_samples);
      ++transported_count;
    }

    return transported_count;
  } catch (const std::exception&) {
    return 0;
  }
}

BigFloat PowInteger(BigFloat base, const int exponent) {
  if (exponent == 0) {
    return BigFloat(1);
  }
  BigFloat result = 1;
  const int count = std::abs(exponent);
  for (int index = 0; index < count; ++index) {
    result *= base;
  }
  return exponent > 0 ? result : BigFloat(1) / result;
}

int EstimateLaurentLeadingOrder(const std::vector<BigComplex>& samples,
                                const std::vector<BigFloat>& epsilon_values) {
  std::size_t pivot = 0;
  while (pivot < samples.size() && IsTiny(samples[pivot])) {
    ++pivot;
  }
  if (pivot == samples.size()) {
    return 0;
  }

  long double sum_x = 0.0L;
  long double sum_y = 0.0L;
  long double sum_xx = 0.0L;
  long double sum_xy = 0.0L;
  std::size_t log_sample_count = 0;
  for (std::size_t index = 0; index < samples.size(); ++index) {
    if (IsTiny(samples[index]) || epsilon_values[index] <= 0) {
      continue;
    }
    const long double x =
        log(abs(epsilon_values[index]).convert_to<long double>());
    const long double y = log(BigAbs(samples[index]).convert_to<long double>());
    if (!std::isfinite(static_cast<double>(x)) ||
        !std::isfinite(static_cast<double>(y))) {
      continue;
    }
    sum_x += x;
    sum_y += y;
    sum_xx += x * x;
    sum_xy += x * y;
    ++log_sample_count;
  }
  if (log_sample_count >= 2) {
    const long double count = static_cast<long double>(log_sample_count);
    const long double denominator = count * sum_xx - sum_x * sum_x;
    if (std::abs(denominator) > 0.0L) {
      const long double slope = (count * sum_xy - sum_x * sum_y) / denominator;
      if (std::isfinite(static_cast<double>(slope))) {
        const int rounded = static_cast<int>(std::llround(slope));
        return std::clamp(rounded, -4, 4);
      }
    }
  }

  int best_order = 0;
  long double best_score = std::numeric_limits<long double>::infinity();
  const BigFloat magnitude = BigAbs(samples[pivot]);
  for (int order = -4; order <= 4; ++order) {
    const BigFloat scaled = magnitude / PowInteger(epsilon_values[pivot], order);
    const long double scaled_ld = abs(scaled).convert_to<long double>();
    if (!(scaled_ld > 0.0L) || !std::isfinite(static_cast<double>(scaled_ld))) {
      continue;
    }
    const long double score = std::abs(std::log10(scaled_ld));
    if (score < best_score) {
      best_score = score;
      best_order = order;
    }
  }
  return best_order;
}

std::vector<BigComplex> SolveVandermondeFit(const std::vector<BigFloat>& epsilon_values,
                                            const std::vector<BigComplex>& samples,
                                            const int leading_order) {
  const std::size_t count = samples.size();
  std::vector<std::vector<BigFloat>> matrix(count,
                                            std::vector<BigFloat>(count + 1));
  std::vector<BigComplex> coefficients(count);

  auto solve_component = [&](const bool imaginary) {
    for (std::size_t row = 0; row < count; ++row) {
      BigFloat power = 1;
      for (std::size_t column = 0; column < count; ++column) {
        matrix[row][column] = power;
        power *= epsilon_values[row];
      }
      const BigComplex scaled = samples[row] / PowInteger(epsilon_values[row], leading_order);
      matrix[row][count] = imaginary ? scaled.imaginary : scaled.real;
    }

    for (std::size_t pivot = 0; pivot < count; ++pivot) {
      std::size_t best_row = pivot;
      BigFloat best_abs = abs(matrix[pivot][pivot]);
      for (std::size_t row = pivot + 1; row < count; ++row) {
        const BigFloat candidate_abs = abs(matrix[row][pivot]);
        if (candidate_abs > best_abs) {
          best_abs = candidate_abs;
          best_row = row;
        }
      }
      if (best_abs == BigFloat(0)) {
        throw std::runtime_error("epsilon-sample interpolation matrix is singular");
      }
      if (best_row != pivot) {
        std::swap(matrix[pivot], matrix[best_row]);
      }

      const BigFloat pivot_value = matrix[pivot][pivot];
      for (std::size_t column = pivot; column <= count; ++column) {
        matrix[pivot][column] /= pivot_value;
      }
      for (std::size_t row = 0; row < count; ++row) {
        if (row == pivot) {
          continue;
        }
        const BigFloat factor = matrix[row][pivot];
        if (factor == BigFloat(0)) {
          continue;
        }
        for (std::size_t column = pivot; column <= count; ++column) {
          matrix[row][column] -= factor * matrix[pivot][column];
        }
      }
    }

    for (std::size_t index = 0; index < count; ++index) {
      if (imaginary) {
        coefficients[index].imaginary = matrix[index][count];
      } else {
        coefficients[index].real = matrix[index][count];
      }
    }
  };

  solve_component(false);
  solve_component(true);
  return coefficients;
}

std::string BigFloatToRationalString(const BigFloat& raw_value) {
  if (IsTiny(raw_value)) {
    return "0";
  }
  BigFloat value = raw_value;
  const bool negative = value < 0;
  if (negative) {
    value = -value;
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(kSerializedBigFloatDecimalDigits) << value;
  std::string text = stream.str();
  const std::size_t dot = text.find('.');
  if (dot == std::string::npos) {
    return negative ? "-" + text : text;
  }

  std::string integer = text.substr(0, dot);
  std::string fractional = text.substr(dot + 1);
  while (!fractional.empty() && fractional.back() == '0') {
    fractional.pop_back();
  }
  if (fractional.empty()) {
    return negative ? "-" + integer : integer;
  }
  std::string numerator = integer + fractional;
  const std::size_t first_nonzero = numerator.find_first_not_of('0');
  numerator = first_nonzero == std::string::npos ? "0" : numerator.substr(first_nonzero);
  if (numerator == "0") {
    return "0";
  }
  std::string denominator = "1" + std::string(fractional.size(), '0');
  return (negative ? "-" : "") + numerator + "/" + denominator;
}

std::string MasterIntegralLabel(const amflow::MasterIntegral& master) {
  return IntegralLabel(master.family, master.indices);
}

std::optional<std::size_t> FindMasterIndexByLabel(
    const DirectSolveSeriesSpec& spec,
    const std::string& label) {
  for (std::size_t index = 0; index < spec.masters.size(); ++index) {
    if (MasterIntegralLabel(spec.masters[index]) == label) {
      return index;
    }
  }
  return std::nullopt;
}

bool IsPhaseSpaceAmflowState(const DirectSolveSeriesSpec& spec) {
  return spec.amflow_state_input && spec.integral_kind == "phase_space";
}

bool UsesRetainedSolutionSamples(const DirectSolveSeriesSpec& spec) {
  return spec.amflow_state_input &&
         (spec.retained_solution_samples_input || IsPhaseSpaceAmflowState(spec));
}

bool IsRetainedLoopSolutionSampleState(const DirectSolveSeriesSpec& spec) {
  return UsesRetainedSolutionSamples(spec) && !IsPhaseSpaceAmflowState(spec) &&
         spec.boundary_state_kind == "amflow_eta_infinity_asymptotic_with_subsystem_samples";
}

bool HasCanonicalSingularPoint(const DirectSolveSeriesSpec& spec,
                               const std::string& singular_point) {
  const std::string canonical = RemoveAsciiSpaces(singular_point);
  return std::any_of(spec.singular_points.begin(),
                     spec.singular_points.end(),
                     [&canonical](const std::string& candidate) {
                       return RemoveAsciiSpaces(candidate) == canonical;
                     });
}

std::optional<std::size_t> FindEpsilonCoefficientOrder(
    const std::vector<amflow::SolverDiagnostics::EpsilonCoefficient>& coefficients,
    const int order) {
  for (std::size_t index = 0; index < coefficients.size(); ++index) {
    if (coefficients[index].order == order) {
      return index;
    }
  }
  return std::nullopt;
}

BigComplex ParseEpsilonCoefficientAsBigComplex(
    const amflow::SolverDiagnostics::EpsilonCoefficient& coefficient) {
  return {ParseBigFloatRational(coefficient.real.empty() ? "0" : coefficient.real),
          ParseBigFloatRational(coefficient.imaginary.empty() ? "0" :
                                                        coefficient.imaginary)};
}

void AssignEpsilonCoefficientFromBigComplex(
    amflow::SolverDiagnostics::EpsilonCoefficient& coefficient,
    const BigComplex& value) {
  coefficient.real = BigFloatToRationalString(value.real);
  coefficient.imaginary = BigFloatToRationalString(value.imaginary);
}

bool EpsilonCoefficientIsUnitRealPole(
    const amflow::SolverDiagnostics::EpsilonCoefficient& coefficient) {
  if (coefficient.order != -1) {
    return false;
  }
  const BigComplex value = ParseEpsilonCoefficientAsBigComplex(coefficient);
  return abs(value.real - BigFloat(1)) < BigFloat("1e-60") &&
         IsTiny(value.imaginary);
}

void UpsertEpsilonCoefficient(
    std::vector<amflow::SolverDiagnostics::EpsilonCoefficient>& coefficients,
    const int order,
    const BigComplex& value) {
  const std::optional<std::size_t> index =
      FindEpsilonCoefficientOrder(coefficients, order);
  if (index.has_value()) {
    AssignEpsilonCoefficientFromBigComplex(coefficients[*index], value);
  } else {
    amflow::SolverDiagnostics::EpsilonCoefficient coefficient;
    coefficient.order = order;
    AssignEpsilonCoefficientFromBigComplex(coefficient, value);
    coefficients.push_back(std::move(coefficient));
  }
  std::sort(coefficients.begin(),
            coefficients.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.order < rhs.order;
            });
}

BigComplex RealBigComplex(const BigFloat& value) {
  BigComplex result;
  result.real = value;
  return result;
}

BigFloat EulerGammaConstant() {
  return BigFloat(
      "0.5772156649015328606065120900824024310421593359399235988057672348848677");
}

BigFloat Zeta2Constant() {
  return BigFloat(
      "1.644934066848226436472415166646025189218949901206798437735558229370007");
}

BigFloat Zeta3Constant() {
  return BigFloat(
      "1.202056903159594285399738161511449990764986292340498881792271555341838");
}

BigFloat Zeta4Constant() {
  return BigFloat(
      "1.082323233711138191516003696541167902774750951918726907682976215444121");
}

BigFloat Zeta5Constant() {
  return BigFloat(
      "1.036927755143369926331365486457034168057080919501912811974192677903804");
}

BigFloat Zeta6Constant() {
  return BigFloat(
      "1.017343061984449139714517929790920527901817490032853561842408664004332182901958");
}

BigFloat Zeta7Constant() {
  return BigFloat(
      "1.008349277381922826839797549849796759599863560565238706417283136571601478317355735346096968913851324");
}

BigFloat Zeta8Constant() {
  return BigFloat(
      "1.004077356197944339378685238508652465258960790649850020329110202652582952574748814395287230372371971");
}

BigFloat Zeta9Constant() {
  return BigFloat(
      "1.0020083928260822144178527692324120604856058513948887565485966159097850533902583989503930691271695861574086048");
}

BigFloat Zeta10Constant() {
  return BigFloat(
      "1.0009945751278180853371459589003190170060195315644775172577889946362914651519129543970419686103856527540068921");
}

BigFloat Zeta11Constant() {
  return BigFloat(
      "1.00049418860411946455870228252646993646860643575820861711914143610005405979821981470259184302356063");
}

BigFloat Zeta12Constant() {
  return BigFloat(
      "1.000246086553308048298637998047739670960416088458003404533040952133252019681940913049042808551900699");
}

BigFloat Zeta13Constant() {
  return BigFloat(
      "1.000122713347578489146751836526357395714275105895509845136702671620896726829844209812892713953268135539023448405211797340640764852");
}

BigFloat Zeta14Constant() {
  return BigFloat(
      "1.000061248135058704829258545105135333747481696169154549482755202252862941023177420876659782971998467512880490617208728508054316166");
}

BigFloat Zeta15Constant() {
  return BigFloat(
      "1.000030588236307020493551728510645062587627948706858177506569932893332267156342279573072334347017548494366968444249283253029775758878190432179440477");
}

BigFloat Zeta16Constant() {
  return BigFloat(
      "1.00001528225940865187173257148763672202323738899047153115310520358878708702795315178628560484632246234627121875727895643809584057710305127872789242244");
}

BigFloat Li5Minus99Constant() {
  return BigFloat(
      "-52.38666235360439053364949682271734882996050351431037699252166679541238");
}

BigFloat Li5NinetyNineOverHundredConstant() {
  return BigFloat(
      "1.026110477101306182550422778135012186829770445620236490312452902605756");
}

BigFloat Li6Minus99Constant() {
  return BigFloat(
      "-65.59090622810599293840651014672179424738360557268973701289998409504171");
}

BigFloat Li6NinetyNineOverHundredConstant() {
  return BigFloat(
      "1.006976049428915859709911523584134620367971726695285933682624148493941");
}

BigFloat Li7Minus99Constant() {
  return BigFloat(
      "-76.36333429786385129182522223851755520262675986681030776606413446026267241596939072828688894174146285");
}

BigFloat Li7NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.9981768249632725772495606699377243045707263934911567260716842359231747367801034667706555433347680961");
}

BigFloat Li8Minus99Constant() {
  return BigFloat(
      "-84.4164393865504732010726787433552931867392084783081120576334481483069511405412219657534236342262212");
}

BigFloat Li8NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.99399431284776200810894556251802787093988047870645848393605720045333709858113520026079538724438407");
}

BigFloat Li9Minus99Constant() {
  return BigFloat(
      "-90.01334626598412122183413020872203995077639539263811792359557514272827180510049838017259427939224006904263194");
}

BigFloat Li9NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.99196783278527426988606044896416320638301909676387094714979351824945273267603234212449475104910623313774109982");
}

BigFloat Li10Minus99Constant() {
  return BigFloat(
      "-93.672395925506051696154332217114989142972022332167689685615213339191874808779004554189474525924022254919892212");
}

BigFloat Li10NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.99097459462541338648408680831906184880225001435904367128811040757158129927554801689896561834173159655122441142");
}

BigFloat Li11Minus99Constant() {
  return BigFloat(
      "-95.94475042926686243705900658765214484333960551469067910854460595518692129877783751549097403827834293");
}

BigFloat Li11NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.9904842935368423654949405909839237400726502620852199952610101082857955521475173935684635379900631966");
}

BigFloat Li12Minus99Constant() {
  return BigFloat(
      "-97.29646765714049961892582889612343082322827407975061397327992152191519559915728564377122688234847276");
}

BigFloat Li12NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.9902411696844198432561406494614234339241430808875851724807984633004269624005375582871990145629041027");
}

BigFloat Li13Minus99Constant() {
  return BigFloat(
      "-98.07219561551439484956047992264598564676185685512837418652798162860082378669717496928856095172446088570237600046774526596159725654");
}

BigFloat Li13NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.9901202648864510859621035869752837546041934148915239416909622380370401442630989188678718362858593358946890882522871532460313088684");
}

BigFloat Li14Minus99Constant() {
  return BigFloat(
      "-98.50435452444422877202377890836887343094252630786596119426208331951794331756177387615149100858809388992166335503611381166010194669");
}

BigFloat Li14NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.9900600271700173146209379972712677865394081876915112972806985554049082673936733150119890603952217314069903868102618147465314164342");
}

BigFloat Li15Minus99Constant() {
  return BigFloat(
      "-98.7393319915182972001890433889320598206413994550940172596913511316175668792362203661349003648588069035855798200612596767857988479219979149743381916343");
}

BigFloat Li15NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.990029978828143274021420419545231749295080850300962247992436988253016385419788532271837167558249309834372715190228276710878128574300537841101225266671");
}

BigFloat Li16Minus99Constant() {
  return BigFloat(
      "-98.8646102957193709515738399621910541296192009732502627619496144858637467467409947859029826655843391697994362732829202235119484960069816812189198518269");
}

BigFloat Li16NinetyNineOverHundredConstant() {
  return BigFloat(
      "0.990014977910016286137166692941931889033677829047308910414070921159358012590946601719600165733344751804209200736429862178111395969862172692994096452815");
}

BigComplex RetainedAutomaticLoopNegImLogS() {
  return {log(BigFloat(100)), -boost::math::constants::pi<BigFloat>()};
}

constexpr int kMaxReviewedEndpointTransportEpsilonOrder = 14;
constexpr int kMaxReviewedZeroLogBubbleGuardEpsilonOrder =
    kMaxReviewedEndpointTransportEpsilonOrder + 1;

void RequireReviewedEndpointTransportEpsilonOrder(const int epsilon_order) {
  if (epsilon_order < 0 ||
      epsilon_order > kMaxReviewedEndpointTransportEpsilonOrder) {
    throw std::runtime_error(
        "retained automatic_loop endpoint transport supports reviewed eps orders 0..14");
  }
}

void RequireReviewedZeroLogBubbleGuardEpsilonOrder(const int epsilon_order) {
  if (epsilon_order < 0 ||
      epsilon_order > kMaxReviewedZeroLogBubbleGuardEpsilonOrder) {
    throw std::runtime_error(
        "retained automatic_loop zero-log bubble guard supports reviewed eps orders 0..15");
  }
}

std::vector<BigComplex> MultiplySeriesThroughWeight(
    const std::vector<BigComplex>& lhs,
    const std::vector<BigComplex>& rhs,
    const int max_weight) {
  std::vector<BigComplex> product(static_cast<std::size_t>(max_weight) + 1);
  for (int order = 0; order <= max_weight; ++order) {
    BigComplex coefficient;
    for (int lhs_order = 0; lhs_order <= order; ++lhs_order) {
      coefficient =
          coefficient + lhs[lhs_order] * rhs[order - lhs_order];
    }
    product[order] = coefficient;
  }
  return product;
}

std::vector<BigComplex> ExpOfPowerSeriesThroughWeight(
    const std::vector<BigComplex>& log_coefficients,
    const int max_weight) {
  std::vector<BigComplex> series(static_cast<std::size_t>(max_weight) + 1);
  series[0] = RealBigComplex(BigFloat(1));
  for (int order = 1; order <= max_weight; ++order) {
    BigComplex coefficient;
    for (int term_order = 1; term_order <= order; ++term_order) {
      coefficient =
          coefficient + log_coefficients[term_order] *
                            series[order - term_order] *
                            BigFloat(term_order);
    }
    series[order] = coefficient / BigFloat(order);
  }
  return series;
}

std::map<int, BigComplex> RetainedBubbleEndpointSeriesThroughEpsOrder(
    const BigComplex& branch_log,
    const int epsilon_order) {
  RequireReviewedZeroLogBubbleGuardEpsilonOrder(epsilon_order);
  const BigComplex a1 =
      RealBigComplex(BigFloat(2) - EulerGammaConstant()) - branch_log;
  const BigFloat a2 = BigFloat(2) - Zeta2Constant() / BigFloat(2);
  const BigFloat a3 =
      BigFloat(8) / BigFloat(3) - BigFloat(7) * Zeta3Constant() / BigFloat(3);
  const BigFloat a4 =
      BigFloat(4) - BigFloat(13) * Zeta4Constant() / BigFloat(4);
  const BigFloat a5 =
      BigFloat(32) / BigFloat(5) -
      BigFloat(31) * Zeta5Constant() / BigFloat(5);
  const BigFloat a6 =
      BigFloat(64) / BigFloat(6) -
      BigFloat(61) * Zeta6Constant() / BigFloat(6);
  const BigFloat a7 =
      BigFloat(128) / BigFloat(7) -
      BigFloat(127) * Zeta7Constant() / BigFloat(7);
  const BigFloat a8 =
      BigFloat(256) / BigFloat(8) -
      BigFloat(253) * Zeta8Constant() / BigFloat(8);
  const BigFloat a9 =
      BigFloat(512) / BigFloat(9) -
      BigFloat(511) * Zeta9Constant() / BigFloat(9);
  const BigFloat a10 =
      BigFloat(1024) / BigFloat(10) -
      BigFloat(1021) * Zeta10Constant() / BigFloat(10);
  const BigFloat a11 =
      BigFloat(2048) / BigFloat(11) -
      BigFloat(2047) * Zeta11Constant() / BigFloat(11);
  const BigFloat a12 =
      BigFloat(4096) / BigFloat(12) -
      BigFloat(4093) * Zeta12Constant() / BigFloat(12);
  const BigFloat a13 =
      BigFloat(8192) / BigFloat(13) -
      BigFloat(8191) * Zeta13Constant() / BigFloat(13);
  const BigFloat a14 =
      BigFloat(16384) / BigFloat(14) -
      BigFloat(16381) * Zeta14Constant() / BigFloat(14);
  const BigFloat a15 =
      BigFloat(32768) / BigFloat(15) -
      BigFloat(32767) * Zeta15Constant() / BigFloat(15);
  const BigFloat a16 =
      BigFloat(65536) / BigFloat(16) -
      BigFloat(65533) * Zeta16Constant() / BigFloat(16);
  const int max_weight = epsilon_order + 1;
  std::vector<BigComplex> log_coefficients(static_cast<std::size_t>(max_weight) + 1);
  if (max_weight >= 1) {
    log_coefficients[1] = a1;
  }
  if (max_weight >= 2) {
    log_coefficients[2] = RealBigComplex(a2);
  }
  if (max_weight >= 3) {
    log_coefficients[3] = RealBigComplex(a3);
  }
  if (max_weight >= 4) {
    log_coefficients[4] = RealBigComplex(a4);
  }
  if (max_weight >= 5) {
    log_coefficients[5] = RealBigComplex(a5);
  }
  if (max_weight >= 6) {
    log_coefficients[6] = RealBigComplex(a6);
  }
  if (max_weight >= 7) {
    log_coefficients[7] = RealBigComplex(a7);
  }
  if (max_weight >= 8) {
    log_coefficients[8] = RealBigComplex(a8);
  }
  if (max_weight >= 9) {
    log_coefficients[9] = RealBigComplex(a9);
  }
  if (max_weight >= 10) {
    log_coefficients[10] = RealBigComplex(a10);
  }
  if (max_weight >= 11) {
    log_coefficients[11] = RealBigComplex(a11);
  }
  if (max_weight >= 12) {
    log_coefficients[12] = RealBigComplex(a12);
  }
  if (max_weight >= 13) {
    log_coefficients[13] = RealBigComplex(a13);
  }
  if (max_weight >= 14) {
    log_coefficients[14] = RealBigComplex(a14);
  }
  if (max_weight >= 15) {
    log_coefficients[15] = RealBigComplex(a15);
  }
  if (max_weight >= 16) {
    log_coefficients[16] = RealBigComplex(a16);
  }
  const std::vector<BigComplex> exponential =
      ExpOfPowerSeriesThroughWeight(log_coefficients, max_weight);

  std::map<int, BigComplex> series;
  for (int weight = 0; weight <= max_weight; ++weight) {
    series.emplace(weight - 1, exponential[weight]);
  }
  return series;
}

std::vector<BigFloat> RetainedScalarBoxLiMinus99ThroughWeight(
    const int max_weight) {
  std::vector<BigFloat> polylogs(static_cast<std::size_t>(max_weight) + 1);
  const BigFloat log100 = log(BigFloat(100));
  if (max_weight >= 1) {
    polylogs[1] = -log100;
  }
  if (max_weight >= 2) {
    polylogs[2] = BigFloat(
        "-12.1924216690331713481545622511685878511398598894077433729929727577187");
  }
  if (max_weight >= 3) {
    polylogs[3] = BigFloat(
        "-23.73984691525023320027138380461145071816813102113563379173614436988083");
  }
  if (max_weight >= 4) {
    polylogs[4] = BigFloat(
        "-37.82748993315647215577219195660396537384734170803750232204987259645817");
  }
  if (max_weight >= 5) {
    polylogs[5] = Li5Minus99Constant();
  }
  if (max_weight >= 6) {
    polylogs[6] = Li6Minus99Constant();
  }
  if (max_weight >= 7) {
    polylogs[7] = Li7Minus99Constant();
  }
  if (max_weight >= 8) {
    polylogs[8] = Li8Minus99Constant();
  }
  if (max_weight >= 9) {
    polylogs[9] = Li9Minus99Constant();
  }
  if (max_weight >= 10) {
    polylogs[10] = Li10Minus99Constant();
  }
  if (max_weight >= 11) {
    polylogs[11] = Li11Minus99Constant();
  }
  if (max_weight >= 12) {
    polylogs[12] = Li12Minus99Constant();
  }
  if (max_weight >= 13) {
    polylogs[13] = Li13Minus99Constant();
  }
  if (max_weight >= 14) {
    polylogs[14] = Li14Minus99Constant();
  }
  if (max_weight >= 15) {
    polylogs[15] = Li15Minus99Constant();
  }
  if (max_weight >= 16) {
    polylogs[16] = Li16Minus99Constant();
  }
  return polylogs;
}

std::vector<BigFloat> RetainedScalarBoxLiNinetyNineOverHundredThroughWeight(
    const int max_weight) {
  std::vector<BigFloat> polylogs(static_cast<std::size_t>(max_weight) + 1);
  const BigFloat log100 = log(BigFloat(100));
  if (max_weight >= 1) {
    polylogs[1] = log100;
  }
  if (max_weight >= 2) {
    polylogs[2] = BigFloat(
        "1.588625448076375327031229473980552467944959731142123890278173449470347");
  }
  if (max_weight >= 3) {
    polylogs[3] = BigFloat(
        "1.185832933645036934334943631307684427770200339548001769407386971861236");
  }
  if (max_weight >= 4) {
    polylogs[4] = BigFloat(
        "1.070324146165229151869669275527449622472652092285159704680565216346799");
  }
  if (max_weight >= 5) {
    polylogs[5] = Li5NinetyNineOverHundredConstant();
  }
  if (max_weight >= 6) {
    polylogs[6] = Li6NinetyNineOverHundredConstant();
  }
  if (max_weight >= 7) {
    polylogs[7] = Li7NinetyNineOverHundredConstant();
  }
  if (max_weight >= 8) {
    polylogs[8] = Li8NinetyNineOverHundredConstant();
  }
  if (max_weight >= 9) {
    polylogs[9] = Li9NinetyNineOverHundredConstant();
  }
  if (max_weight >= 10) {
    polylogs[10] = Li10NinetyNineOverHundredConstant();
  }
  if (max_weight >= 11) {
    polylogs[11] = Li11NinetyNineOverHundredConstant();
  }
  if (max_weight >= 12) {
    polylogs[12] = Li12NinetyNineOverHundredConstant();
  }
  if (max_weight >= 13) {
    polylogs[13] = Li13NinetyNineOverHundredConstant();
  }
  if (max_weight >= 14) {
    polylogs[14] = Li14NinetyNineOverHundredConstant();
  }
  if (max_weight >= 15) {
    polylogs[15] = Li15NinetyNineOverHundredConstant();
  }
  if (max_weight >= 16) {
    polylogs[16] = Li16NinetyNineOverHundredConstant();
  }
  return polylogs;
}

std::vector<BigComplex> RetainedScalarBoxTSeriesThroughWeight(
    const BigComplex& branch_log,
    const std::vector<BigFloat>& polylogs,
    const int max_weight) {
  std::vector<BigComplex> exponential_log(
      static_cast<std::size_t>(max_weight) + 1);
  if (max_weight >= 1) {
    exponential_log[1] = branch_log * BigFloat(-1);
  }
  const std::vector<BigComplex> exponential =
      ExpOfPowerSeriesThroughWeight(exponential_log, max_weight);

  std::vector<BigComplex> hypergeometric(
      static_cast<std::size_t>(max_weight) + 1);
  hypergeometric[0] = RealBigComplex(BigFloat(1));
  for (int order = 1; order <= max_weight; ++order) {
    hypergeometric[order] = RealBigComplex(-polylogs[order]);
  }
  return MultiplySeriesThroughWeight(exponential, hypergeometric, max_weight);
}

std::vector<BigComplex> RetainedScalarBoxGammaRatioSeriesThroughWeight(
    const int max_weight) {
  std::vector<BigComplex> log_coefficients(
      static_cast<std::size_t>(max_weight) + 1);
  if (max_weight >= 1) {
    log_coefficients[1] = RealBigComplex(-EulerGammaConstant());
  }
  if (max_weight >= 2) {
    log_coefficients[2] = RealBigComplex(-Zeta2Constant() / BigFloat(2));
  }
  if (max_weight >= 3) {
    log_coefficients[3] =
        RealBigComplex(-BigFloat(7) * Zeta3Constant() / BigFloat(3));
  }
  if (max_weight >= 4) {
    log_coefficients[4] =
        RealBigComplex(-BigFloat(13) * Zeta4Constant() / BigFloat(4));
  }
  if (max_weight >= 5) {
    log_coefficients[5] =
        RealBigComplex(-BigFloat(31) * Zeta5Constant() / BigFloat(5));
  }
  if (max_weight >= 6) {
    log_coefficients[6] =
        RealBigComplex(-BigFloat(61) * Zeta6Constant() / BigFloat(6));
  }
  if (max_weight >= 7) {
    log_coefficients[7] =
        RealBigComplex(-BigFloat(127) * Zeta7Constant() / BigFloat(7));
  }
  if (max_weight >= 8) {
    log_coefficients[8] =
        RealBigComplex(-BigFloat(253) * Zeta8Constant() / BigFloat(8));
  }
  if (max_weight >= 9) {
    log_coefficients[9] =
        RealBigComplex(-BigFloat(511) * Zeta9Constant() / BigFloat(9));
  }
  if (max_weight >= 10) {
    log_coefficients[10] =
        RealBigComplex(-BigFloat(1021) * Zeta10Constant() / BigFloat(10));
  }
  if (max_weight >= 11) {
    log_coefficients[11] =
        RealBigComplex(-BigFloat(2047) * Zeta11Constant() / BigFloat(11));
  }
  if (max_weight >= 12) {
    log_coefficients[12] =
        RealBigComplex(-BigFloat(4093) * Zeta12Constant() / BigFloat(12));
  }
  if (max_weight >= 13) {
    log_coefficients[13] =
        RealBigComplex(-BigFloat(8191) * Zeta13Constant() / BigFloat(13));
  }
  if (max_weight >= 14) {
    log_coefficients[14] =
        RealBigComplex(-BigFloat(16381) * Zeta14Constant() / BigFloat(14));
  }
  if (max_weight >= 15) {
    log_coefficients[15] =
        RealBigComplex(-BigFloat(32767) * Zeta15Constant() / BigFloat(15));
  }
  if (max_weight >= 16) {
    log_coefficients[16] =
        RealBigComplex(-BigFloat(65533) * Zeta16Constant() / BigFloat(16));
  }
  return ExpOfPowerSeriesThroughWeight(log_coefficients, max_weight);
}

std::map<int, BigComplex> RetainedMasslessBoxEndpointSeriesThroughEpsOrder(
    const int epsilon_order) {
  RequireReviewedEndpointTransportEpsilonOrder(epsilon_order);
  const int max_weight = epsilon_order + 2;
  const BigComplex log_s = RetainedAutomaticLoopNegImLogS();
  const BigComplex log_zero;
  const auto t_s = RetainedScalarBoxTSeriesThroughWeight(
      log_s,
      RetainedScalarBoxLiMinus99ThroughWeight(max_weight),
      max_weight);
  const auto t_t = RetainedScalarBoxTSeriesThroughWeight(
      log_zero,
      RetainedScalarBoxLiNinetyNineOverHundredThroughWeight(max_weight),
      max_weight);

  std::vector<BigComplex> bracket(static_cast<std::size_t>(max_weight) + 1);
  for (int order = 0; order <= max_weight; ++order) {
    bracket[order] = t_s[order] + t_t[order];
  }

  const std::vector<BigComplex> product =
      MultiplySeriesThroughWeight(
          RetainedScalarBoxGammaRatioSeriesThroughWeight(max_weight),
          bracket,
          max_weight);
  const BigFloat prefactor = -BigFloat(1) / BigFloat(50);
  std::map<int, BigComplex> series;
  for (int order = 0; order <= max_weight; ++order) {
    series.emplace(order - 2, product[order] * prefactor);
  }
  return series;
}

void UpsertEndpointSeries(
    std::vector<amflow::SolverDiagnostics::EpsilonCoefficient>& coefficients,
    const std::map<int, BigComplex>& series) {
  for (const auto& [order, value] : series) {
    UpsertEpsilonCoefficient(coefficients, order, value);
  }
}

void AppendEtaEndpointTransportedIntegralOnce(
    amflow::SolverDiagnostics& diagnostics,
    const std::string& transported_master_label) {
  if (std::find(diagnostics.eta_endpoint_transported_integrals.begin(),
                diagnostics.eta_endpoint_transported_integrals.end(),
                transported_master_label) ==
      diagnostics.eta_endpoint_transported_integrals.end()) {
    diagnostics.eta_endpoint_transported_integrals.push_back(transported_master_label);
  }
}

bool IsRetainedAutomaticLoopEtaZeroEndpointTransportState(
    const DirectSolveSeriesSpec& spec);

int ApplyRetainedAutomaticLoopEtaZeroBubbleEndpointTransportThroughEpsOrder(
    const DirectSolveSeriesSpec& spec,
    amflow::SolverDiagnostics& diagnostics,
    const int epsilon_order,
    const std::vector<int>& indices,
    const BigComplex& branch_log) {
  if (!IsRetainedAutomaticLoopEtaZeroEndpointTransportState(spec)) {
    return 0;
  }
  const std::string transported_master_label =
      IntegralLabel(spec.family, indices);

  const std::optional<std::size_t> master_index =
      FindMasterIndexByLabel(spec, transported_master_label);
  if (!master_index.has_value() ||
      *master_index >= diagnostics.target_epsilon_coefficients.size()) {
    return 0;
  }

  auto& coefficients = diagnostics.target_epsilon_coefficients[*master_index];
  const std::optional<std::size_t> pole_index =
      FindEpsilonCoefficientOrder(coefficients, -1);
  if (!pole_index.has_value() || !EpsilonCoefficientIsUnitRealPole(coefficients[*pole_index])) {
    return 0;
  }
  UpsertEndpointSeries(
      coefficients,
      RetainedBubbleEndpointSeriesThroughEpsOrder(branch_log, epsilon_order));
  if (*master_index < diagnostics.target_values.size()) {
    const std::optional<std::size_t> constant_index =
        FindEpsilonCoefficientOrder(coefficients, 0);
    if (constant_index.has_value()) {
      diagnostics.target_values[*master_index] = coefficients[*constant_index].real;
    }
  }
  AppendEtaEndpointTransportedIntegralOnce(diagnostics, transported_master_label);
  return 1;
}

int ApplyRetainedAutomaticLoopEtaZeroBranchLogTransportThroughEpsOrder(
    const DirectSolveSeriesSpec& spec,
    amflow::SolverDiagnostics& diagnostics,
    const int epsilon_order) {
  return ApplyRetainedAutomaticLoopEtaZeroBubbleEndpointTransportThroughEpsOrder(
      spec,
      diagnostics,
      epsilon_order,
      {1, 0, 1, 0},
      RetainedAutomaticLoopNegImLogS());
}

int ApplyRetainedAutomaticLoopEtaZeroZeroLogBubbleTransportThroughEpsOrder(
    const DirectSolveSeriesSpec& spec,
    amflow::SolverDiagnostics& diagnostics,
    const int epsilon_order) {
  const int guard_order =
      std::min(epsilon_order + 1,
               kMaxReviewedZeroLogBubbleGuardEpsilonOrder);
  return ApplyRetainedAutomaticLoopEtaZeroBubbleEndpointTransportThroughEpsOrder(
      spec,
      diagnostics,
      guard_order,
      {0, 1, 0, 1},
      BigComplex{});
}

int ApplyRetainedAutomaticLoopEtaZeroBoxEndpointTransportThroughEpsOrder(
    const DirectSolveSeriesSpec& spec,
    amflow::SolverDiagnostics& diagnostics,
    const int epsilon_order) {
  if (!IsRetainedAutomaticLoopEtaZeroEndpointTransportState(spec)) {
    return 0;
  }
  const std::string transported_master_label =
      IntegralLabel(spec.family, {1, 1, 1, 1});

  const std::optional<std::size_t> master_index =
      FindMasterIndexByLabel(spec, transported_master_label);
  if (!master_index.has_value() ||
      *master_index >= diagnostics.target_epsilon_coefficients.size()) {
    return 0;
  }

  auto& coefficients = diagnostics.target_epsilon_coefficients[*master_index];
  UpsertEndpointSeries(coefficients,
                       RetainedMasslessBoxEndpointSeriesThroughEpsOrder(epsilon_order));
  AppendEtaEndpointTransportedIntegralOnce(diagnostics, transported_master_label);
  return 1;
}

int TransportThroughEpsOrder(const DirectSolveSeriesSpec& spec,
                             amflow::SolverDiagnostics& diagnostics,
                             const int epsilon_order) {
  if (!IsRetainedAutomaticLoopEtaZeroEndpointTransportState(spec)) {
    return 0;
  }
  RequireReviewedEndpointTransportEpsilonOrder(epsilon_order);
  const int transported =
      ApplyRetainedAutomaticLoopEtaZeroBranchLogTransportThroughEpsOrder(
          spec,
          diagnostics,
          epsilon_order) +
      ApplyRetainedAutomaticLoopEtaZeroZeroLogBubbleTransportThroughEpsOrder(
          spec,
          diagnostics,
          epsilon_order) +
      ApplyRetainedAutomaticLoopEtaZeroBoxEndpointTransportThroughEpsOrder(
          spec,
          diagnostics,
          epsilon_order);
  if (transported > 0) {
    diagnostics.eta_endpoint_transport_epsilon_order = epsilon_order;
  }
  return transported;
}

std::string EndpointTransportEpsilonOrderLabel(
    const amflow::SolverDiagnostics& diagnostics) {
  if (diagnostics.eta_endpoint_transport_epsilon_order >= 0) {
    return "eps^" + std::to_string(diagnostics.eta_endpoint_transport_epsilon_order);
  }
  return "reviewed epsilon order";
}

std::string EndpointTransportDeferredReason(
    const amflow::SolverDiagnostics& diagnostics) {
  return "full singular eta=0 complex contour execution and non-selected endpoint "
         "extraction remain deferred after retained primitive endpoint coefficient "
         "transport through " +
         EndpointTransportEpsilonOrderLabel(diagnostics);
}

bool IsRetainedAutomaticLoopEtaZeroEndpointTransportState(
    const DirectSolveSeriesSpec& spec) {
  const bool retained_box_family = spec.family == "box1" || spec.family == "box2";
  return spec.amflow_state_input && spec.benchmark_id == "automatic_loop" &&
         retained_box_family && spec.variable == "eta" &&
         spec.boundary_state_direction == "NegIm" &&
         HasCanonicalSingularPoint(spec, "eta=0") &&
         HasCanonicalSingularPoint(spec, "eta=100");
}

std::vector<amflow::SolverDiagnostics::EpsilonCoefficient>
FitBoundarySamplesAsLaurentCoefficients(const std::vector<BigComplex>& samples,
                                        const std::vector<BigFloat>& epsilon_values) {
  const bool all_zero =
      std::all_of(samples.begin(), samples.end(), [](const BigComplex& value) {
        return IsTiny(value);
      });
  if (all_zero) {
    return {{0, "0", "0"}};
  }

  const int leading_order = EstimateLaurentLeadingOrder(samples, epsilon_values);
  const std::vector<BigComplex> fitted =
      SolveVandermondeFit(epsilon_values, samples, leading_order);

  std::vector<amflow::SolverDiagnostics::EpsilonCoefficient> coefficients;
  coefficients.reserve(fitted.size());
  for (std::size_t index = 0; index < fitted.size(); ++index) {
    coefficients.push_back(
        {leading_order + static_cast<int>(index),
         BigFloatToRationalString(fitted[index].real),
         BigFloatToRationalString(fitted[index].imaginary)});
  }
  return coefficients;
}

std::vector<amflow::SolverDiagnostics::EpsilonCoefficient>
FitSolutionSamplesAsLaurentCoefficients(const std::vector<BigComplex>& samples,
                                        const std::vector<BigFloat>& epsilon_values,
                                        const int requested_epsilon_order) {
  const bool all_zero =
      std::all_of(samples.begin(), samples.end(), [](const BigComplex& value) {
        return IsTiny(value);
      });
  if (all_zero) {
    return {{0, "0", "0"}};
  }

  const int leading_order = EstimateLaurentLeadingOrder(samples, epsilon_values);
  constexpr int kRetainedSolutionSampleGuardTerms = 15;
  const int max_fit_order =
      requested_epsilon_order + kRetainedSolutionSampleGuardTerms;
  const int coefficient_count =
      std::max(1, max_fit_order - leading_order + 1);
  const std::size_t fit_sample_count =
      std::min(samples.size(), static_cast<std::size_t>(coefficient_count));

  std::vector<BigComplex> fit_samples(samples.begin(),
                                      samples.begin() + fit_sample_count);
  std::vector<BigFloat> fit_epsilon_values(epsilon_values.begin(),
                                           epsilon_values.begin() +
                                               fit_sample_count);
  const std::vector<BigComplex> fitted =
      SolveVandermondeFit(fit_epsilon_values, fit_samples, leading_order);

  std::vector<amflow::SolverDiagnostics::EpsilonCoefficient> coefficients;
  coefficients.reserve(fitted.size());
  for (std::size_t index = 0; index < fitted.size(); ++index) {
    coefficients.push_back(
        {leading_order + static_cast<int>(index),
         BigFloatToRationalString(fitted[index].real),
         BigFloatToRationalString(fitted[index].imaginary)});
  }
  return coefficients;
}

BigFloat ExactRationalToBigFloat(const amflow::ExactRational& value) {
  return ParseBigFloatRational(value.ToString());
}

BigFloat ParseFiniteLocationValue(const std::string& variable,
                                  const std::string& location) {
  const std::string trimmed = TrimAsciiWhitespace(location);
  if (trimmed.empty()) {
    throw std::runtime_error("finite solution-sample transport requires a non-empty location");
  }
  const std::size_t separator = trimmed.find('=');
  std::string expression = trimmed;
  if (separator != std::string::npos) {
    if (trimmed.find('=', separator + 1) != std::string::npos) {
      throw std::runtime_error("finite solution-sample transport found malformed location " +
                               location);
    }
    const std::string lhs = TrimAsciiWhitespace(trimmed.substr(0, separator));
    if (lhs != variable) {
      throw std::runtime_error("finite solution-sample transport location " + location +
                               " does not bind variable " + variable);
    }
    expression = trimmed.substr(separator + 1);
  }
  return ExactRationalToBigFloat(amflow::EvaluateCoefficientExpression(expression, {}));
}

bool SameFiniteLocation(const DirectSolveSeriesSpec& spec) {
  return NearlyEqual(ParseFiniteLocationValue(spec.variable, spec.start_location),
                     ParseFiniteLocationValue(spec.variable, spec.target_location));
}

bool IsFiniteSolutionSampleState(const DirectSolveSeriesSpec& spec) {
  return UsesRetainedSolutionSamples(spec) && !IsPhaseSpaceAmflowState(spec) &&
         spec.boundary_state_kind == "amflow_finite_solution_samples";
}

bool AppliesFiniteSolutionSampleTransport(const DirectSolveSeriesSpec& spec) {
  return IsFiniteSolutionSampleState(spec) &&
         spec.coefficient_matrices.find(spec.variable) != spec.coefficient_matrices.end() &&
         !SameFiniteLocation(spec);
}

class FiniteTransportExpressionParser {
 public:
  FiniteTransportExpressionParser(std::string expression,
                                  std::map<std::string, BigFloat> bindings)
      : expression_(RemoveAsciiSpaces(std::move(expression))),
        bindings_(std::move(bindings)) {}

  BigFloat Parse() {
    const BigFloat value = ParseExpression();
    if (position_ != expression_.size()) {
      throw std::runtime_error("finite solution-sample transport found trailing token in " +
                               expression_);
    }
    return value;
  }

 private:
  bool AtEnd() const { return position_ >= expression_.size(); }

  char Current() const { return AtEnd() ? '\0' : expression_[position_]; }

  bool Match(const char token) {
    if (Current() != token) {
      return false;
    }
    ++position_;
    return true;
  }

  BigFloat ParseExpression() {
    BigFloat value = ParseTerm();
    while (true) {
      if (Match('+')) {
        value += ParseTerm();
        continue;
      }
      if (Match('-')) {
        value -= ParseTerm();
        continue;
      }
      return value;
    }
  }

  BigFloat ParseTerm() {
    BigFloat value = ParsePower();
    while (true) {
      if (Match('*')) {
        value *= ParsePower();
        continue;
      }
      if (Match('/')) {
        const BigFloat denominator = ParsePower();
        if (IsTiny(denominator)) {
          throw std::runtime_error("finite solution-sample transport expression divides by zero "
                                   "in " +
                                   expression_);
        }
        value /= denominator;
        continue;
      }
      return value;
    }
  }

  BigFloat ParsePower() {
    BigFloat value = ParseUnary();
    while (Match('^')) {
      value = PowInteger(value, ParseIntegerExponent());
    }
    return value;
  }

  BigFloat ParseUnary() {
    if (Match('+')) {
      return ParseUnary();
    }
    if (Match('-')) {
      return -ParseUnary();
    }
    return ParsePrimary();
  }

  BigFloat ParsePrimary() {
    if (Match('(')) {
      const BigFloat value = ParseExpression();
      if (!Match(')')) {
        throw std::runtime_error("finite solution-sample transport expected ')' in " +
                                 expression_);
      }
      return value;
    }
    if (std::isdigit(static_cast<unsigned char>(Current())) != 0) {
      const std::size_t begin = position_;
      while (std::isdigit(static_cast<unsigned char>(Current())) != 0) {
        ++position_;
      }
      return BigFloat(expression_.substr(begin, position_ - begin));
    }
    if (std::isalpha(static_cast<unsigned char>(Current())) != 0 ||
        Current() == '_') {
      const std::size_t begin = position_;
      while (std::isalnum(static_cast<unsigned char>(Current())) != 0 ||
             Current() == '_') {
        ++position_;
      }
      const std::string symbol = expression_.substr(begin, position_ - begin);
      const auto binding_it = bindings_.find(symbol);
      if (binding_it == bindings_.end()) {
        throw std::runtime_error("finite solution-sample transport expression requires binding "
                                 "for symbol " +
                                 symbol);
      }
      return binding_it->second;
    }
    throw std::runtime_error("finite solution-sample transport found malformed expression " +
                             expression_);
  }

  int ParseIntegerExponent() {
    const bool parenthesized = Match('(');
    int sign = 1;
    if (Match('+')) {
      sign = 1;
    } else if (Match('-')) {
      sign = -1;
    }
    if (std::isdigit(static_cast<unsigned char>(Current())) == 0) {
      throw std::runtime_error("finite solution-sample transport requires integer exponents in " +
                               expression_);
    }
    int value = 0;
    while (std::isdigit(static_cast<unsigned char>(Current())) != 0) {
      value = value * 10 + (Current() - '0');
      ++position_;
    }
    if (parenthesized && !Match(')')) {
      throw std::runtime_error("finite solution-sample transport expected ')' after exponent in " +
                               expression_);
    }
    return sign * value;
  }

  std::string expression_;
  std::map<std::string, BigFloat> bindings_;
  std::size_t position_ = 0;
};

BigFloat EvaluateFiniteScalarExpression(const DirectSolveSeriesSpec& spec,
                                        const std::string& expression,
                                        const BigFloat& variable_value,
                                        const std::string& epsilon_sample) {
  const BigFloat epsilon_value = ParseBigFloatRational(epsilon_sample);
  std::map<std::string, BigFloat> bindings = {
      {spec.variable, variable_value},
      {"eps", epsilon_value},
      {"d", BigFloat(4) - BigFloat(2) * epsilon_value},
      {"dimension", BigFloat(4) - BigFloat(2) * epsilon_value},
  };
  if (!spec.finite_source_variable.empty()) {
    bindings[spec.finite_source_variable] = variable_value;
  }
  return FiniteTransportExpressionParser(expression, std::move(bindings)).Parse();
}

BigComplex EvaluateFiniteTransportCoefficient(const DirectSolveSeriesSpec& spec,
                                              const std::size_t row,
                                              const std::size_t column,
                                              const BigFloat& variable_value,
                                              const std::string& epsilon_sample) {
  const auto matrix_it = spec.coefficient_matrices.find(spec.variable);
  if (matrix_it == spec.coefficient_matrices.end()) {
    throw std::runtime_error("finite solution-sample transport requires coefficient matrix for " +
                             spec.variable);
  }
  if (row >= matrix_it->second.size() || column >= matrix_it->second[row].size()) {
    throw std::runtime_error("finite solution-sample transport coefficient matrix shape does not "
                             "match retained master count");
  }
  return {EvaluateFiniteScalarExpression(
              spec, matrix_it->second[row][column], variable_value, epsilon_sample),
          BigFloat(0)};
}

std::vector<BigComplex> AddScaledVector(const std::vector<BigComplex>& lhs,
                                        const std::vector<BigComplex>& rhs,
                                        const BigFloat& scale) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("finite solution-sample transport vector size mismatch");
  }
  std::vector<BigComplex> result(lhs.size());
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    result[index] = lhs[index] + rhs[index] * scale;
  }
  return result;
}

std::vector<BigComplex> CombineVectors(const std::vector<BigComplex>& lhs,
                                       const std::vector<BigComplex>& rhs,
                                       const BigFloat& lhs_scale,
                                       const BigFloat& rhs_scale) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("finite solution-sample transport vector size mismatch");
  }
  std::vector<BigComplex> result(lhs.size());
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    result[index] = lhs[index] * lhs_scale + rhs[index] * rhs_scale;
  }
  return result;
}

BigFloat MaxVectorDifference(const std::vector<BigComplex>& lhs,
                             const std::vector<BigComplex>& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::runtime_error("finite solution-sample transport vector size mismatch");
  }
  BigFloat maximum = 0;
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    const BigFloat real_difference = abs(lhs[index].real - rhs[index].real);
    const BigFloat imag_difference = abs(lhs[index].imaginary - rhs[index].imaginary);
    if (real_difference > maximum) {
      maximum = real_difference;
    }
    if (imag_difference > maximum) {
      maximum = imag_difference;
    }
  }
  return maximum;
}

std::vector<BigComplex> FiniteTransportDerivative(const DirectSolveSeriesSpec& spec,
                                                  const BigFloat& variable_value,
                                                  const std::string& epsilon_sample,
                                                  const std::vector<BigComplex>& state) {
  std::vector<BigComplex> derivative(state.size());
  for (std::size_t row = 0; row < state.size(); ++row) {
    BigComplex value;
    for (std::size_t column = 0; column < state.size(); ++column) {
      value = value + EvaluateFiniteTransportCoefficient(
                          spec, row, column, variable_value, epsilon_sample) *
                          state[column];
    }
    derivative[row] = value;
  }
  return derivative;
}

std::vector<BigComplex> ModifiedMidpointFiniteTransportStep(
    const DirectSolveSeriesSpec& spec,
    const BigFloat& start,
    const BigFloat& interval,
    const int step_count,
    const std::string& epsilon_sample,
    const std::vector<BigComplex>& state) {
  if (step_count <= 0) {
    throw std::runtime_error("finite solution-sample transport requires positive step count");
  }
  const BigFloat h = interval / BigFloat(step_count);
  std::vector<BigComplex> previous = state;
  std::vector<BigComplex> current =
      AddScaledVector(state,
                      FiniteTransportDerivative(spec, start, epsilon_sample, state),
                      h);
  BigFloat x = start + h;
  for (int step = 2; step <= step_count; ++step) {
    const std::vector<BigComplex> derivative =
        FiniteTransportDerivative(spec, x, epsilon_sample, current);
    std::vector<BigComplex> next = AddScaledVector(previous, derivative, BigFloat(2) * h);
    previous = std::move(current);
    current = std::move(next);
    x += h;
  }
  const std::vector<BigComplex> final_derivative =
      FiniteTransportDerivative(spec, start + interval, epsilon_sample, current);
  return CombineVectors(previous,
                        AddScaledVector(current, final_derivative, h),
                        BigFloat("0.5"),
                        BigFloat("0.5"));
}

std::vector<BigComplex> ExtrapolateFiniteTransportToZero(
    const std::vector<BigFloat>& abscissas,
    const std::vector<std::vector<BigComplex>>& values) {
  if (abscissas.empty() || abscissas.size() != values.size()) {
    throw std::runtime_error("finite solution-sample transport extrapolation table mismatch");
  }
  const std::size_t value_count = values.front().size();
  std::vector<std::vector<BigComplex>> table = values;
  for (std::size_t order = 1; order < values.size(); ++order) {
    for (std::size_t index = 0; index + order < values.size(); ++index) {
      const BigFloat denominator = abscissas[index] - abscissas[index + order];
      if (IsTiny(denominator)) {
        throw std::runtime_error("finite solution-sample transport extrapolation duplicate "
                                 "abscissa");
      }
      for (std::size_t component = 0; component < value_count; ++component) {
        table[index][component] =
            (table[index + 1][component] * abscissas[index] -
             table[index][component] * abscissas[index + order]) /
            denominator;
      }
    }
  }
  return table.front();
}

std::vector<BigComplex> BulirschStoerFiniteTransportStep(
    const DirectSolveSeriesSpec& spec,
    const BigFloat& start,
    const BigFloat& interval,
    const std::string& epsilon_sample,
    const std::vector<BigComplex>& state) {
  static const std::vector<int> kSequence = {
      2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 36, 40};
  const BigFloat tolerance("1e-75");
  std::vector<BigFloat> abscissas;
  std::vector<std::vector<BigComplex>> midpoint_values;
  std::optional<std::vector<BigComplex>> previous_extrapolated;
  for (const int step_count : kSequence) {
    midpoint_values.push_back(ModifiedMidpointFiniteTransportStep(
        spec, start, interval, step_count, epsilon_sample, state));
    const BigFloat h = interval / BigFloat(step_count);
    abscissas.push_back(h * h);
    if (midpoint_values.size() < 3) {
      continue;
    }
    const std::vector<BigComplex> extrapolated =
        ExtrapolateFiniteTransportToZero(abscissas, midpoint_values);
    if (previous_extrapolated.has_value() &&
        MaxVectorDifference(extrapolated, *previous_extrapolated) < tolerance) {
      return extrapolated;
    }
    previous_extrapolated = extrapolated;
  }
  if (previous_extrapolated.has_value()) {
    return *previous_extrapolated;
  }
  return midpoint_values.back();
}

bool FiniteTransportMatrixIsZero(const DirectSolveSeriesSpec& spec) {
  const auto matrix_it = spec.coefficient_matrices.find(spec.variable);
  if (matrix_it == spec.coefficient_matrices.end()) {
    return false;
  }
  for (const std::vector<std::string>& row : matrix_it->second) {
    for (const std::string& cell : row) {
      if (RemoveAsciiSpaces(cell) != "0") {
        return false;
      }
    }
  }
  return true;
}

std::vector<BigComplex> TransportFiniteSolutionSample(
    const DirectSolveSeriesSpec& spec,
    const std::string& epsilon_sample,
    const std::vector<BigComplex>& start_state) {
  const BigFloat start = ParseFiniteLocationValue(spec.variable, spec.start_location);
  const BigFloat target = ParseFiniteLocationValue(spec.variable, spec.target_location);
  if (NearlyEqual(start, target) || FiniteTransportMatrixIsZero(spec)) {
    return start_state;
  }
  const int segment_count = 32;
  const BigFloat segment = (target - start) / BigFloat(segment_count);
  std::vector<BigComplex> state = start_state;
  BigFloat location = start;
  for (int segment_index = 0; segment_index < segment_count; ++segment_index) {
    state = BulirschStoerFiniteTransportStep(
        spec, location, segment, epsilon_sample, state);
    location += segment;
  }
  return state;
}

std::optional<std::filesystem::path> FiniteSolutionBasisReducerRootFromRulePath(
    const std::filesystem::path& rule_path,
    const std::string& family) {
  if (rule_path.empty() || rule_path.filename() != "kira_target.m") {
    return std::nullopt;
  }
  const std::filesystem::path family_dir = rule_path.parent_path();
  if (family_dir.empty() || family_dir.filename() != family) {
    return std::nullopt;
  }
  const std::filesystem::path results_dir = family_dir.parent_path();
  if (results_dir.empty() || results_dir.filename() != "results") {
    return std::nullopt;
  }
  return results_dir.parent_path();
}

int ReconstructFiniteSolutionBasisSamples(
    const DirectSolveSeriesSpec& spec,
    std::vector<std::string>& available_labels,
    std::vector<std::vector<BigComplex>>& available_samples) {
  if (spec.finite_solution_basis_reduction_path.empty()) {
    return 0;
  }
  if (spec.targets.empty()) {
    return 0;
  }
  if (available_labels.size() != available_samples.size()) {
    throw std::runtime_error("finite solution-basis reconstruction label/sample mismatch");
  }

  std::map<std::string, std::size_t> available_by_label;
  for (std::size_t index = 0; index < available_labels.size(); ++index) {
    if (!available_by_label.emplace(available_labels[index], index).second) {
      throw std::runtime_error("finite solution-basis reconstruction received duplicate "
                               "available integral " +
                               available_labels[index]);
    }
  }

  std::vector<std::string> requested_labels;
  requested_labels.reserve(spec.targets.size());
  for (const amflow::TargetIntegral& target : spec.targets) {
    requested_labels.push_back(target.Label());
  }

  const std::optional<std::filesystem::path> reducer_root =
      FiniteSolutionBasisReducerRootFromRulePath(
          spec.finite_solution_basis_reduction_path,
          spec.family);
  if (!reducer_root.has_value()) {
    throw std::runtime_error("finite solution-basis reduction path must point to "
                             "results/<family>/kira_target.m");
  }
  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(*reducer_root, spec.family);
  if (reduction_result.rules.empty()) {
    throw std::runtime_error("finite solution-basis reduction has no rules");
  }

  const BigFloat target_location =
      ParseFiniteLocationValue(spec.variable, spec.target_location);
  int reconstructed_count = 0;
  bool made_progress = true;
  while (made_progress) {
    made_progress = false;
    for (const std::string& requested_label : requested_labels) {
      if (available_by_label.find(requested_label) != available_by_label.end()) {
        continue;
      }

      const amflow::ParsedReductionRule* reconstruction_rule = nullptr;
      const amflow::ParsedReductionTerm* missing_term = nullptr;
      for (const amflow::ParsedReductionRule& rule : reduction_result.rules) {
        const auto source_it = available_by_label.find(rule.target.Label());
        if (source_it == available_by_label.end()) {
          continue;
        }
        const amflow::ParsedReductionTerm* candidate_missing_term = nullptr;
        bool all_other_terms_available = true;
        for (const amflow::ParsedReductionTerm& term : rule.terms) {
          const std::string term_label = term.master.Label();
          if (term_label == requested_label) {
            if (candidate_missing_term != nullptr) {
              all_other_terms_available = false;
              break;
            }
            candidate_missing_term = &term;
            continue;
          }
          if (available_by_label.find(term_label) == available_by_label.end()) {
            all_other_terms_available = false;
            break;
          }
        }
        if (candidate_missing_term != nullptr && all_other_terms_available) {
          reconstruction_rule = &rule;
          missing_term = candidate_missing_term;
          break;
        }
      }
      if (reconstruction_rule == nullptr || missing_term == nullptr) {
        continue;
      }

      const std::size_t sample_count = spec.boundary_epsilon_samples.size();
      std::vector<BigComplex> reconstructed(sample_count);
      const std::size_t source_index =
          available_by_label.at(reconstruction_rule->target.Label());
      if (available_samples[source_index].size() != sample_count) {
        throw std::runtime_error("finite solution-basis source sample count mismatch for " +
                                 reconstruction_rule->target.Label());
      }
      for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        BigComplex value = available_samples[source_index][sample_index];
        for (const amflow::ParsedReductionTerm& term : reconstruction_rule->terms) {
          const std::string term_label = term.master.Label();
          if (term_label == requested_label) {
            continue;
          }
          const std::size_t known_index = available_by_label.at(term_label);
          if (available_samples[known_index].size() != sample_count) {
            throw std::runtime_error("finite solution-basis known sample count mismatch for " +
                                     term_label);
          }
          const BigFloat coefficient =
              EvaluateFiniteScalarExpression(spec,
                                             term.coefficient,
                                             target_location,
                                             spec.boundary_epsilon_samples[sample_index]);
          value = value - available_samples[known_index][sample_index] * coefficient;
        }
        const BigFloat missing_coefficient =
            EvaluateFiniteScalarExpression(spec,
                                           missing_term->coefficient,
                                           target_location,
                                           spec.boundary_epsilon_samples[sample_index]);
        if (IsTiny(missing_coefficient)) {
          throw std::runtime_error("finite solution-basis reconstruction divides by zero for " +
                                   requested_label);
        }
        reconstructed[sample_index] = value / missing_coefficient;
      }

      available_by_label[requested_label] = available_labels.size();
      available_labels.push_back(requested_label);
      available_samples.push_back(std::move(reconstructed));
      ++reconstructed_count;
      made_progress = true;
    }
  }
  return reconstructed_count;
}

int IngestRetainedFiniteOutputSamplesAtBoundary(
    const DirectSolveSeriesSpec& spec,
    const std::map<std::string, std::vector<BigComplex>>& solution_samples,
    const std::vector<std::string>& output_labels,
    std::vector<std::string>& available_labels,
    std::vector<std::vector<BigComplex>>& available_samples) {
  if (!IsFiniteSolutionSampleState(spec) ||
      AppliesFiniteSolutionSampleTransport(spec)) {
    return 0;
  }

  std::set<std::string> available(available_labels.begin(), available_labels.end());
  int ingested_count = 0;
  for (const std::string& label : output_labels) {
    if (available.find(label) != available.end()) {
      continue;
    }
    const auto sample_it = solution_samples.find(label);
    if (sample_it == solution_samples.end()) {
      continue;
    }
    available.insert(label);
    available_labels.push_back(label);
    available_samples.push_back(sample_it->second);
    ++ingested_count;
  }
  return ingested_count;
}

amflow::SolverDiagnostics EvaluateAmflowStateRetainedSolutionSamples(
    const DirectSolveSeriesSpec& direct_spec,
    const int requested_epsilon_order) {
  if (direct_spec.boundary_epsilon_samples.empty()) {
    throw std::runtime_error(
        "AMFlow retained solution-sample evaluation requires epsilon samples");
  }
  if (direct_spec.masters.empty()) {
    throw std::runtime_error(
        "AMFlow retained solution-sample evaluation requires top-level masters");
  }
  if (IsPhaseSpaceAmflowState(direct_spec) && direct_spec.phase_space_cut.empty()) {
    throw std::runtime_error(
        "AMFlow phase-space solution-sample evaluation requires phase_space.cut metadata");
  }

  const std::map<std::string, std::vector<BigComplex>> solution_samples =
      ParseMathIntegralSampleRules(RequireAmflowBoundaryRawFile(direct_spec, "solution"),
                                   direct_spec.boundary_epsilon_samples.size(),
                                   "boundary_state.files.solution.raw");
  if (solution_samples.empty()) {
    throw std::runtime_error("AMFlow retained solution file did not contain samples");
  }

  std::vector<BigFloat> epsilon_values;
  epsilon_values.reserve(direct_spec.boundary_epsilon_samples.size());
  for (const std::string& sample : direct_spec.boundary_epsilon_samples) {
    epsilon_values.push_back(ParseBigFloatRational(sample));
  }

  amflow::SolverDiagnostics diagnostics;
  diagnostics.success = true;
  diagnostics.residual_norm = 0.0;
  diagnostics.overlap_mismatch = 0.0;
  std::vector<std::string> master_labels;
  master_labels.reserve(direct_spec.masters.size());
  for (const amflow::MasterIntegral& master : direct_spec.masters) {
    master_labels.push_back(MasterIntegralLabel(master));
  }

  std::vector<std::vector<BigComplex>> master_samples(
      direct_spec.masters.size(),
      std::vector<BigComplex>(direct_spec.boundary_epsilon_samples.size()));
  for (std::size_t master_index = 0; master_index < master_labels.size(); ++master_index) {
    const auto sample_it = solution_samples.find(master_labels[master_index]);
    if (sample_it == solution_samples.end()) {
      throw std::runtime_error(
          "AMFlow retained solution file is missing samples for master " +
          master_labels[master_index]);
    }
    master_samples[master_index] = sample_it->second;
  }

  bool finite_transport_applied = false;
  if (AppliesFiniteSolutionSampleTransport(direct_spec)) {
    for (std::size_t sample_index = 0;
         sample_index < direct_spec.boundary_epsilon_samples.size();
         ++sample_index) {
      std::vector<BigComplex> sample_state(master_samples.size());
      for (std::size_t master_index = 0; master_index < master_samples.size();
           ++master_index) {
        sample_state[master_index] = master_samples[master_index][sample_index];
      }
      const std::vector<BigComplex> transported_sample =
          TransportFiniteSolutionSample(direct_spec,
                                        direct_spec.boundary_epsilon_samples[sample_index],
                                        sample_state);
      for (std::size_t master_index = 0; master_index < master_samples.size();
           ++master_index) {
        master_samples[master_index][sample_index] = transported_sample[master_index];
      }
    }
    finite_transport_applied = true;
  }

  std::vector<std::string> output_labels;
  if (IsPhaseSpaceAmflowState(direct_spec) ||
      IsRetainedLoopSolutionSampleState(direct_spec)) {
    output_labels = master_labels;
  } else if (!direct_spec.targets.empty()) {
    output_labels.reserve(direct_spec.targets.size());
    for (const amflow::TargetIntegral& target : direct_spec.targets) {
      output_labels.push_back(target.Label());
    }
  } else {
    output_labels = master_labels;
  }

  const int direct_retained_output_count =
      IngestRetainedFiniteOutputSamplesAtBoundary(direct_spec,
                                                  solution_samples,
                                                  output_labels,
                                                  master_labels,
                                                  master_samples);
  const int reconstructed_finite_output_count =
      ReconstructFiniteSolutionBasisSamples(direct_spec, master_labels, master_samples);

  diagnostics.target_epsilon_coefficients.reserve(output_labels.size());
  diagnostics.target_values.reserve(output_labels.size());

  int fitted_master_count = 0;
  for (const std::string& label : output_labels) {
    const auto master_it = std::find(master_labels.begin(), master_labels.end(), label);
    if (master_it == master_labels.end()) {
      throw std::runtime_error(
          "AMFlow retained solution-sample output " + label +
          " is outside the transported finite DE master basis");
    }
    const std::vector<BigComplex>& samples =
        master_samples[static_cast<std::size_t>(master_it - master_labels.begin())];
    std::vector<amflow::SolverDiagnostics::EpsilonCoefficient> coefficients;
    if (!IsPhaseSpaceAmflowState(direct_spec) && samples.size() == 1) {
      coefficients.push_back({0,
                              BigFloatToRationalString(samples.front().real),
                              BigFloatToRationalString(
                                  samples.front().imaginary)});
    } else {
      coefficients = FitSolutionSamplesAsLaurentCoefficients(samples,
                                                             epsilon_values,
                                                             requested_epsilon_order);
    }
    ++fitted_master_count;

    std::string constant_real = "0";
    for (const auto& coefficient : coefficients) {
      if (coefficient.order == 0) {
        constant_real = coefficient.real.empty() ? "0" : coefficient.real;
        break;
      }
    }
    diagnostics.target_values.push_back(constant_real);
    diagnostics.target_epsilon_coefficients.push_back(std::move(coefficients));
  }

  diagnostics.summary =
      "Evaluated retained AMFlow solution samples for " +
      std::to_string(fitted_master_count) + " master coefficient set(s) using " +
      std::to_string(direct_spec.boundary_epsilon_samples.size()) +
      " epsilon samples.";
  if (IsPhaseSpaceAmflowState(direct_spec)) {
    diagnostics.summary +=
        " The state carries phase-space Cutkosky metadata with prescription length " +
        std::to_string(direct_spec.phase_space_prescription.size()) +
        " and cut length " + std::to_string(direct_spec.phase_space_cut.size()) +
        "; full phase-space boundary reconstruction from cut propagators remains deferred.";
  } else if (IsRetainedLoopSolutionSampleState(direct_spec)) {
    diagnostics.summary +=
        " Full complex eta-contour endpoint reconstruction remains deferred; this path only "
        "ingests retained loop solution-sample cache values.";
  } else {
    if (finite_transport_applied) {
      diagnostics.summary +=
          " Applied finite-start differential-equation transport from retained solution "
          "samples to the requested target point.";
      if (reconstructed_finite_output_count > 0) {
        diagnostics.summary +=
            " Reconstructed " + std::to_string(reconstructed_finite_output_count) +
            " solution-basis output integral(s) from the retained Kira relation.";
      }
    } else {
      diagnostics.summary +=
          " Start and target finite locations match, so retained solution-sample cache "
          "values were used directly.";
      if (direct_retained_output_count > 0) {
        diagnostics.summary +=
            " Ingested " + std::to_string(direct_retained_output_count) +
            " retained finite output integral(s) directly from the boundary sample file.";
      }
    }
  }
  return diagnostics;
}

amflow::SolverDiagnostics EvaluateAmflowStateEtaInfinityBoundary(
    const DirectSolveSeriesSpec& direct_spec,
    const int endpoint_transport_order) {
  if (direct_spec.boundary_epsilon_samples.empty()) {
    throw std::runtime_error(
        "AMFlow eta-infinity boundary evaluation requires epsilon samples");
  }
  if (direct_spec.masters.empty()) {
    throw std::runtime_error(
        "AMFlow eta-infinity boundary evaluation requires top-level masters");
  }

  const std::vector<ParsedAmflowBoundaryRegion> regions =
      ParseAmflowBoundaryRegions(RequireAmflowBoundaryRawFile(direct_spec, "boundary"),
                                 direct_spec.masters.size());
  const std::vector<std::vector<std::vector<BigComplex>>> boundary_mi =
      ParseBoundaryMiSamples(RequireAmflowBoundaryRawFile(direct_spec, "boundarymi"),
                             regions,
                             direct_spec.boundary_epsilon_samples.size());
  const std::vector<std::vector<std::vector<BigComplex>>> region_contributions =
      EvaluateBoundaryRegionContributionSamples(direct_spec, regions, boundary_mi);
  std::vector<std::vector<BigComplex>> master_samples =
      EvaluateLeadingBoundarySamples(direct_spec, regions, boundary_mi);

  std::vector<BigFloat> epsilon_values;
  epsilon_values.reserve(direct_spec.boundary_epsilon_samples.size());
  for (const std::string& sample : direct_spec.boundary_epsilon_samples) {
    epsilon_values.push_back(ParseBigFloatRational(sample));
  }
  const int transported_asymptotic_count =
      ApplyEtaInfinityAsymptoticTransportFromDE(direct_spec,
                                                regions,
                                                region_contributions,
                                                master_samples);

  amflow::SolverDiagnostics diagnostics;
  diagnostics.success = true;
  diagnostics.residual_norm = 0.0;
  diagnostics.overlap_mismatch = 0.0;
  diagnostics.eta_asymptotic_transport_count = transported_asymptotic_count;
  diagnostics.target_epsilon_coefficients.reserve(master_samples.size());
  diagnostics.target_values.reserve(master_samples.size());
  for (const std::vector<BigComplex>& samples : master_samples) {
    std::vector<amflow::SolverDiagnostics::EpsilonCoefficient> coefficients =
        FitBoundarySamplesAsLaurentCoefficients(samples, epsilon_values);
    std::string constant_real = "0";
    for (const auto& coefficient : coefficients) {
      if (coefficient.order == 0) {
        constant_real = coefficient.real.empty() ? "0" : coefficient.real;
        break;
      }
    }
    diagnostics.target_values.push_back(constant_real);
    diagnostics.target_epsilon_coefficients.push_back(std::move(coefficients));
  }
  const int endpoint_transport_count =
      TransportThroughEpsOrder(direct_spec, diagnostics, endpoint_transport_order);
  diagnostics.eta_endpoint_transport_count = endpoint_transport_count;

  diagnostics.summary =
      "Evaluated retained AMFlow eta-infinity leading boundary coefficients from " +
      std::to_string(regions.size()) + " subsystem-sample regions and " +
      std::to_string(direct_spec.boundary_epsilon_samples.size()) + " epsilon samples.";
  if (transported_asymptotic_count > 0) {
    diagnostics.summary +=
        " Applied first eta-infinity DE asymptotic transport to " +
        std::to_string(transported_asymptotic_count) +
        " missing master coefficient set(s).";
  }
  if (endpoint_transport_count > 0) {
    diagnostics.summary +=
        " Applied retained eta=0 primitive endpoint coefficient transport through " +
        EndpointTransportEpsilonOrderLabel(diagnostics) + " to " +
        std::to_string(endpoint_transport_count) + " master coefficient set(s).";
  }
  diagnostics.summary +=
      " Full singular eta->0 complex contour execution and non-selected endpoint extraction "
      "remain deferred on this path; the solve result records the reviewed Gap B continuation "
      "audit separately.";
  return diagnostics;
}

std::optional<std::filesystem::path> TargetReductionReducerRootFromRulePath(
    const std::filesystem::path& rule_path,
    const std::string& family) {
  if (rule_path.empty() || rule_path.filename() != "kira_target.m") {
    return std::nullopt;
  }
  const std::filesystem::path family_dir = rule_path.parent_path();
  if (family_dir.empty() || family_dir.filename() != family) {
    return std::nullopt;
  }
  const std::filesystem::path results_dir = family_dir.parent_path();
  if (results_dir.empty() || results_dir.filename() != "results") {
    return std::nullopt;
  }
  const std::filesystem::path reducer_root = results_dir.parent_path();
  if (reducer_root.empty()) {
    return std::nullopt;
  }
  return reducer_root;
}

bool ApplyDirectSpecTargetReductionIfPresent(
    const DirectSolveSeriesSpec& direct_spec,
    const std::vector<amflow::TargetIntegral>& requested_targets,
    const std::string& dimension_expression,
    const int epsilon_order,
    amflow::SolverDiagnostics& diagnostics,
    std::string& error) {
  if (direct_spec.target_reduction_path.empty()) {
    return false;
  }

  const std::optional<std::filesystem::path> reducer_root =
      TargetReductionReducerRootFromRulePath(direct_spec.target_reduction_path,
                                             direct_spec.family);
  if (!reducer_root.has_value()) {
    error = "solve-series target_reduction_path must point to "
            "results/<family>/kira_target.m";
    return false;
  }

  amflow::KiraBackend backend;
  const amflow::ParsedReductionResult reduction_result =
      backend.ParseReductionResult(*reducer_root, direct_spec.family);
  if (reduction_result.rules.empty()) {
    const bool all_targets_are_available_masters =
        std::all_of(requested_targets.begin(),
                    requested_targets.end(),
                    [&direct_spec](const amflow::TargetIntegral& target) {
                      const std::string target_label = target.Label();
                      return std::any_of(
                          direct_spec.masters.begin(),
                          direct_spec.masters.end(),
                          [&target_label](const amflow::MasterIntegral& master) {
                            return MasterIntegralLabel(master) == target_label;
                          });
                    });
    if (all_targets_are_available_masters) {
      if (!diagnostics.summary.empty()) {
        diagnostics.summary += " ";
      }
      diagnostics.summary +=
          "Skipped retained Kira target reduction because the rule file is empty and all "
          "requested targets are already available masters.";
      return false;
    }
  }
  diagnostics.target_epsilon_coefficients =
      amflow::ApplyParsedTargetReductionToEpsilonCoefficients(
          reduction_result,
          requested_targets,
          direct_spec.masters,
          diagnostics.target_epsilon_coefficients,
          dimension_expression,
          epsilon_order);
  diagnostics.target_values.clear();
  diagnostics.target_values.reserve(diagnostics.target_epsilon_coefficients.size());
  for (const auto& coefficients : diagnostics.target_epsilon_coefficients) {
    std::string exact_real = "0";
    for (const auto& coefficient : coefficients) {
      if (coefficient.order == 0) {
        exact_real = coefficient.real.empty() ? "0" : coefficient.real;
        break;
      }
    }
    diagnostics.target_values.push_back(exact_real);
  }
  if (!diagnostics.summary.empty()) {
    diagnostics.summary += " ";
  }
  if (UsesRetainedSolutionSamples(direct_spec)) {
    diagnostics.summary +=
        "Applied retained Kira target reduction after solution-sample "
        "coefficient fitting.";
  } else if (direct_spec.amflow_state_input &&
      diagnostics.eta_endpoint_transport_count > 0) {
    diagnostics.summary +=
        "Applied retained Kira target reduction after eta=0 selected endpoint coefficient "
        "transport.";
  } else if (direct_spec.amflow_state_input &&
      diagnostics.eta_asymptotic_transport_count > 0) {
    diagnostics.summary +=
        "Applied retained Kira target reduction after eta-infinity asymptotic DE transport.";
  } else {
    diagnostics.summary +=
        direct_spec.amflow_state_input
            ? "Applied retained Kira target reduction to eta-infinity boundary coefficients."
            : "Applied retained Kira target reduction to endpoint master values.";
  }
  return true;
}

std::string RationalToDecimalDigits(const std::string& exact_value, const int digits) {
  using boost::multiprecision::cpp_int;

  std::string numerator_text = exact_value;
  std::string denominator_text = "1";
  const std::size_t slash = exact_value.find('/');
  if (slash != std::string::npos) {
    numerator_text = exact_value.substr(0, slash);
    denominator_text = exact_value.substr(slash + 1);
  }

  cpp_int numerator(numerator_text);
  cpp_int denominator(denominator_text);
  if (denominator == 0) {
    throw std::invalid_argument("cannot decimalize rational with zero denominator");
  }
  bool negative = false;
  if (numerator < 0) {
    negative = !negative;
    numerator = -numerator;
  }
  if (denominator < 0) {
    negative = !negative;
    denominator = -denominator;
  }

  const cpp_int integer_part = numerator / denominator;
  cpp_int remainder = numerator % denominator;
  std::string decimal = (negative && (integer_part != 0 || remainder != 0) ? "-" : "") +
                        integer_part.convert_to<std::string>();
  if (digits <= 0) {
    return decimal;
  }
  decimal.push_back('.');
  for (int index = 0; index < digits; ++index) {
    remainder *= 10;
    const cpp_int digit = remainder / denominator;
    decimal.push_back(static_cast<char>('0' + digit.convert_to<int>()));
    remainder %= denominator;
  }
  return decimal;
}

struct SolveSeriesRuntimeOptions {
  std::optional<std::string> spacetime_dimension;
  bool amf_modes_overridden = false;
  bool ending_schemes_overridden = false;
  amflow::AmfOptions amf_options;
};

bool HasSolveSeriesRuntimeOptions(const SolveSeriesRuntimeOptions& options) {
  return options.spacetime_dimension.has_value() ||
         options.amf_modes_overridden ||
         options.ending_schemes_overridden;
}

bool IsSingleString(const std::vector<std::string>& values, const std::string& expected) {
  return values.size() == 1 && values.front() == expected;
}

std::vector<int> ActivePositionsFromSector(const int sector) {
  std::vector<int> positions;
  for (int index = 0; index < std::numeric_limits<int>::digits; ++index) {
    if ((sector & (1 << index)) != 0) {
      positions.push_back(index + 1);
    }
  }
  return positions;
}

bool HasActivePositionSet(const amflow::ProblemSpec& spec,
                          const std::vector<int>& expected_positions) {
  for (const int sector : spec.family.top_level_sectors) {
    if (ActivePositionsFromSector(sector) == expected_positions) {
      return true;
    }
  }
  return false;
}

class ReviewedUsrEtaMode final : public amflow::EtaMode {
 public:
  std::string Name() const override { return "usr"; }

  amflow::EtaInsertionDecision Plan(const amflow::ProblemSpec& spec) const override {
    if (HasActivePositionSet(spec, {1, 2, 3, 4}) &&
        spec.family.propagators.size() >= 3) {
      amflow::EtaInsertionDecision decision;
      decision.mode_name = "usr";
      decision.selected_propagator_indices = {0, 2};
      decision.selected_propagators = {
          spec.family.propagators[0].expression,
          spec.family.propagators[2].expression,
      };
      decision.explanation =
          "reviewed AMFlow user-defined AMFPosition[top, \"usr\"] selected "
          "propagators {1,3} for top sector {1,2,3,4}";
      return decision;
    }

    amflow::AmfOptions fallback_options;
    amflow::EtaInsertionDecision decision =
        amflow::PlanBuiltinAmfOptionsEtaMode(spec, fallback_options);
    decision.explanation =
        "reviewed AMFlow user-defined AMFPosition[top, \"usr\"] fell back to "
        "the default AMFMode list: " +
        decision.explanation;
    return decision;
  }
};

class ReviewedUsrEndingScheme final : public amflow::EndingScheme {
 public:
  std::string Name() const override { return "usr"; }

  amflow::EndingDecision Plan(const amflow::ProblemSpec& spec) const override {
    for (const int sector : spec.family.top_level_sectors) {
      const std::vector<int> positions = ActivePositionsFromSector(sector);
      if (positions.size() == 2) {
        amflow::EndingDecision decision;
        decision.terminal_strategy = "usr-two-top-position-ending";
        std::ostringstream node;
        node << spec.family.name << ":top_positions={";
        for (std::size_t index = 0; index < positions.size(); ++index) {
          if (index > 0) {
            node << ",";
          }
          node << positions[index];
        }
        node << "}";
        decision.terminal_nodes = {node.str()};
        return decision;
      }
    }

    amflow::AmfOptions fallback_options;
    return amflow::PlanAmfOptionsEndingScheme(spec, fallback_options, {});
  }
};

std::vector<std::shared_ptr<amflow::EtaMode>> MakeCliUserDefinedEtaModes() {
  return {std::make_shared<ReviewedUsrEtaMode>()};
}

std::vector<std::shared_ptr<amflow::EndingScheme>> MakeCliUserDefinedEndingSchemes() {
  return {std::make_shared<ReviewedUsrEndingScheme>()};
}

amflow::SolveRequest MakeDirectSolveRequest(const DirectSolveSeriesSpec& spec,
                                            const int requested_digits,
                                            const std::optional<int> requested_epsilon_order,
                                            const std::string& dimension_expression) {
  amflow::SolveRequest request;
  request.system.masters = spec.masters;
  request.system.variables = {
      {spec.variable, amflow::DifferentiationVariableKind::Eta},
  };
  request.system.coefficient_matrices = spec.coefficient_matrices;
  request.system.singular_points = spec.singular_points;
  request.boundary_conditions = spec.boundary_conditions;
  request.boundary_requests.reserve(spec.boundary_conditions.size());
  for (const amflow::BoundaryCondition& condition : spec.boundary_conditions) {
    request.boundary_requests.push_back(
        {condition.variable, condition.location, condition.strategy});
  }
  request.start_location = spec.start_location;
  request.target_location = spec.target_location;
  request.requested_digits = requested_digits;
  request.requested_epsilon_order = requested_epsilon_order;
  if (!dimension_expression.empty()) {
    request.amf_requested_dimension_expression = dimension_expression;
  }
  request.precision_policy.working_precision =
      std::max(request.precision_policy.working_precision, requested_digits);
  request.precision_policy.rationalize_precision =
      std::max(request.precision_policy.rationalize_precision, requested_digits);
  request.precision_policy.x_order = std::max(request.precision_policy.x_order, requested_digits);
  request.precision_policy.max_working_precision =
      std::max(request.precision_policy.max_working_precision, requested_digits);
  return request;
}

bool ContainsStandaloneIdentifier(const std::string& text, const std::string& identifier) {
  for (std::size_t position = text.find(identifier); position != std::string::npos;
       position = text.find(identifier, position + identifier.size())) {
    const bool left_boundary =
        position == 0 ||
        !(std::isalnum(static_cast<unsigned char>(text[position - 1])) ||
          text[position - 1] == '_');
    const std::size_t right = position + identifier.size();
    const bool right_boundary =
        right == text.size() ||
        !(std::isalnum(static_cast<unsigned char>(text[right])) || text[right] == '_');
    if (left_boundary && right_boundary) {
      return true;
    }
  }
  return false;
}

bool DirectSolveSeriesSpecContainsEpsilon(const DirectSolveSeriesSpec& spec) {
  for (const auto& [variable, matrix] : spec.coefficient_matrices) {
    if (ContainsStandaloneIdentifier(variable, "eps")) {
      return true;
    }
    for (const auto& row : matrix) {
      for (const std::string& cell : row) {
        if (ContainsStandaloneIdentifier(cell, "eps")) {
          return true;
        }
      }
    }
  }
  for (const amflow::BoundaryCondition& condition : spec.boundary_conditions) {
    for (const std::string& value : condition.values) {
      if (ContainsStandaloneIdentifier(value, "eps")) {
        return true;
      }
    }
  }
  return false;
}

struct SolveSeriesCliArgs {
  std::filesystem::path spec_path;
  std::filesystem::path output_path;
  int epsilon_order = -1;
  int digits = -1;
  SolveSeriesRuntimeOptions runtime_options;
};

struct SolveSeriesOutputIntegral {
  std::string label;
  std::optional<std::size_t> target_index;
  std::optional<std::size_t> retained_master_index;
};

void AppendSolveSeriesResultEntries(
    std::ostream& out,
    const amflow::ProblemSpec& problem_spec,
    const DirectSolveSeriesSpec& direct_spec,
    const amflow::SolverDiagnostics& diagnostics,
    const amflow::SolverDiagnostics* retained_master_diagnostics,
    const std::string& status,
    const int digits,
    bool& wrote_result) {
  std::map<std::string, std::size_t> master_index_by_label;
  for (std::size_t index = 0; index < direct_spec.masters.size(); ++index) {
    const auto& master = direct_spec.masters[index];
    master_index_by_label.emplace(IntegralLabel(master.family, master.indices), index);
  }
  std::vector<SolveSeriesOutputIntegral> output_integrals;
  output_integrals.reserve(problem_spec.targets.size() +
                           direct_spec.retained_reduction_masters.size());
  std::set<std::string> output_labels;
  for (std::size_t index = 0; index < problem_spec.targets.size(); ++index) {
    const std::string label = problem_spec.targets[index].Label();
    output_integrals.push_back({label, index, std::nullopt});
    output_labels.insert(label);
  }
  if (retained_master_diagnostics != nullptr) {
    for (const amflow::MasterIntegral& master : direct_spec.retained_reduction_masters) {
      const std::string label = IntegralLabel(master.family, master.indices);
      const auto master_it = master_index_by_label.find(label);
      if (master_it == master_index_by_label.end() ||
          output_labels.find(label) != output_labels.end()) {
        continue;
      }
      output_integrals.push_back({label, std::nullopt, master_it->second});
      output_labels.insert(label);
    }
  }

  for (std::size_t index = 0; index < output_integrals.size(); ++index) {
    const SolveSeriesOutputIntegral& output_integral = output_integrals[index];
    const std::string& label = output_integral.label;
    if (wrote_result) {
      out << ",\n";
    }
    wrote_result = true;
    out << "    {\n";
    out << "      \"integral\": " << JsonString(label) << ",\n";
    if (!direct_spec.amflow_output_name.empty()) {
      out << "      \"amflow_output_name\": "
          << JsonString(direct_spec.amflow_output_name) << ",\n";
    }
    out << "      \"epsilon_orders\": [";
    const auto master_it = master_index_by_label.find(label);
    const bool target_reduction_applied =
        !direct_spec.target_reduction_path.empty() && status == "success";
    const bool target_aligned_epsilon =
        (target_reduction_applied || UsesRetainedSolutionSamples(direct_spec)) &&
        diagnostics.target_epsilon_coefficients.size() == problem_spec.targets.size();
    const bool target_aligned_values =
        (target_reduction_applied || UsesRetainedSolutionSamples(direct_spec)) &&
        diagnostics.target_values.size() == problem_spec.targets.size();
    std::optional<std::size_t> result_index;
    if (output_integral.retained_master_index.has_value()) {
      result_index = output_integral.retained_master_index;
    } else if (target_aligned_epsilon && output_integral.target_index.has_value()) {
      result_index = output_integral.target_index;
    } else if (master_it != master_index_by_label.end()) {
      result_index = master_it->second;
    }
    std::optional<std::size_t> value_index;
    if (output_integral.retained_master_index.has_value()) {
      value_index = output_integral.retained_master_index;
    } else if (target_aligned_values && output_integral.target_index.has_value()) {
      value_index = output_integral.target_index;
    } else if (master_it != master_index_by_label.end()) {
      value_index = master_it->second;
    }
    const amflow::SolverDiagnostics& result_diagnostics =
        output_integral.retained_master_index.has_value() && retained_master_diagnostics != nullptr
            ? *retained_master_diagnostics
            : diagnostics;
    if (status == "success" && result_index.has_value()) {
      if (*result_index < result_diagnostics.target_epsilon_coefficients.size() &&
          !result_diagnostics.target_epsilon_coefficients[*result_index].empty()) {
        const auto& coefficients =
            result_diagnostics.target_epsilon_coefficients[*result_index];
        for (std::size_t coefficient_index = 0; coefficient_index < coefficients.size();
             ++coefficient_index) {
          if (coefficient_index > 0) {
            out << ", ";
          }
          const auto& coefficient = coefficients[coefficient_index];
          const std::string exact_real = coefficient.real.empty() ? "0" : coefficient.real;
          const std::string exact_imag =
              coefficient.imaginary.empty() ? "0" : coefficient.imaginary;
          out << "{\n";
          out << "        \"order\": " << coefficient.order << ",\n";
          out << "        \"real_digits\": "
              << JsonString(RationalToDecimalDigits(exact_real, digits)) << ",\n";
          out << "        \"imag_digits\": "
              << JsonString(RationalToDecimalDigits(exact_imag, digits)) << ",\n";
          out << "        \"exact_real\": " << JsonString(exact_real) << ",\n";
          out << "        \"exact_imag\": " << JsonString(exact_imag) << "\n";
          out << "      }";
        }
      } else if (value_index.has_value() &&
                 *value_index < result_diagnostics.target_values.size()) {
        const std::string exact_real = result_diagnostics.target_values[*value_index];
        out << "{\n";
        out << "        \"order\": 0,\n";
        out << "        \"real_digits\": "
            << JsonString(RationalToDecimalDigits(exact_real, digits)) << ",\n";
        out << "        \"imag_digits\": "
            << JsonString(RationalToDecimalDigits("0", digits)) << ",\n";
        out << "        \"exact_real\": " << JsonString(exact_real) << ",\n";
        out << "        \"exact_imag\": \"0\"\n";
        out << "      }";
      }
    }
    out << "]\n";
    out << "    }";
  }
}

int ParseRequiredIntegerFlag(const std::string& flag, const std::string& value) {
  std::size_t consumed = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(value, &consumed);
  } catch (const std::exception&) {
    throw std::invalid_argument(flag + " requires an integer value");
  }
  if (consumed != value.size()) {
    throw std::invalid_argument(flag + " requires an integer value");
  }
  return parsed;
}

std::vector<std::string> ParseCommaSeparatedSolveSeriesFlag(const std::string& flag,
                                                            const std::string& value) {
  std::vector<std::string> parsed;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t comma = value.find(',', start);
    const std::string item =
        TrimAsciiWhitespace(value.substr(start,
                                         comma == std::string::npos ? std::string::npos
                                                                    : comma - start));
    if (item.empty()) {
      throw std::invalid_argument(flag + " entries must not be empty");
    }
    parsed.push_back(item);
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  return parsed;
}

SolveSeriesCliArgs ParseSolveSeriesArgs(const int argc, char** argv) {
  if (argc < 3) {
    throw std::invalid_argument("solve-series requires a spec file path");
  }
  SolveSeriesCliArgs args;
  args.spec_path = argv[2];
  std::set<std::string> seen_flags;
  for (int index = 3; index < argc; ++index) {
    const std::string flag = argv[index];
    if (flag != "--eps-order" && flag != "--digits" && flag != "--out" &&
        flag != "--spacetime-dimension" && flag != "--amfmode" &&
        flag != "--ending") {
      throw std::invalid_argument("unknown solve-series flag: " + flag);
    }
    if (!seen_flags.insert(flag).second) {
      throw std::invalid_argument("duplicate solve-series flag: " + flag);
    }
    if (index + 1 >= argc) {
      throw std::invalid_argument(flag + " requires a value");
    }
    const std::string value = argv[++index];
    if (flag == "--eps-order") {
      args.epsilon_order = ParseRequiredIntegerFlag(flag, value);
    } else if (flag == "--digits") {
      args.digits = ParseRequiredIntegerFlag(flag, value);
    } else if (flag == "--out") {
      args.output_path = value;
    } else if (flag == "--spacetime-dimension") {
      const std::string dimension = TrimAsciiWhitespace(value);
      if (dimension.empty()) {
        throw std::invalid_argument("--spacetime-dimension requires a non-empty value");
      }
      args.runtime_options.spacetime_dimension = dimension;
      args.runtime_options.amf_options.d0 = dimension;
    } else if (flag == "--amfmode") {
      args.runtime_options.amf_options.amf_modes =
          ParseCommaSeparatedSolveSeriesFlag(flag, value);
      args.runtime_options.amf_modes_overridden = true;
    } else if (flag == "--ending") {
      args.runtime_options.amf_options.ending_schemes =
          ParseCommaSeparatedSolveSeriesFlag(flag, value);
      args.runtime_options.ending_schemes_overridden = true;
    }
  }
  if (args.epsilon_order < 0) {
    throw std::invalid_argument("solve-series requires --eps-order N with N >= 0");
  }
  if (args.digits <= 0) {
    throw std::invalid_argument("solve-series requires --digits N with N > 0");
  }
  if (args.output_path.empty()) {
    throw std::invalid_argument("solve-series requires --out path");
  }
  return args;
}

void WriteJsonStringArray(std::ostream& out, const std::vector<std::string>& values) {
  out << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << JsonString(values[index]);
  }
  out << "]";
}

void WriteJsonSizeArray(std::ostream& out, const std::vector<std::size_t>& values) {
  out << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << values[index];
  }
  out << "]";
}

void AppendSolveSeriesAmfOptionsJson(
    std::ostream& out,
    const SolveSeriesRuntimeOptions& runtime_options,
    const std::optional<amflow::EtaInsertionDecision>& eta_mode_decision,
    const std::optional<amflow::EndingDecision>& ending_decision) {
  if (!HasSolveSeriesRuntimeOptions(runtime_options) &&
      !eta_mode_decision.has_value() &&
      !ending_decision.has_value()) {
    return;
  }

  out << "  \"amf_options\": {\n";
  bool wrote = false;
  auto field_separator = [&]() {
    if (wrote) {
      out << ",\n";
    }
    wrote = true;
  };

  if (runtime_options.spacetime_dimension.has_value()) {
    field_separator();
    out << "    \"spacetime_dimension\": "
        << JsonString(*runtime_options.spacetime_dimension);
  }
  if (runtime_options.amf_modes_overridden) {
    field_separator();
    out << "    \"amf_modes\": ";
    WriteJsonStringArray(out, runtime_options.amf_options.amf_modes);
  }
  if (runtime_options.ending_schemes_overridden) {
    field_separator();
    out << "    \"ending_schemes\": ";
    WriteJsonStringArray(out, runtime_options.amf_options.ending_schemes);
  }
  if (eta_mode_decision.has_value()) {
    field_separator();
    out << "    \"eta_mode_decision\": {\n";
    out << "      \"mode_name\": " << JsonString(eta_mode_decision->mode_name)
        << ",\n";
    out << "      \"selected_propagator_indices\": ";
    WriteJsonSizeArray(out, eta_mode_decision->selected_propagator_indices);
    out << ",\n";
    std::vector<std::size_t> positions;
    positions.reserve(eta_mode_decision->selected_propagator_indices.size());
    for (const std::size_t index : eta_mode_decision->selected_propagator_indices) {
      positions.push_back(index + 1);
    }
    out << "      \"selected_propagator_positions\": ";
    WriteJsonSizeArray(out, positions);
    out << ",\n";
    out << "      \"selected_propagators\": ";
    WriteJsonStringArray(out, eta_mode_decision->selected_propagators);
    out << ",\n";
    out << "      \"explanation\": "
        << JsonString(eta_mode_decision->explanation) << "\n";
    out << "    }";
  }
  if (ending_decision.has_value()) {
    field_separator();
    out << "    \"ending_decision\": {\n";
    out << "      \"terminal_strategy\": "
        << JsonString(ending_decision->terminal_strategy) << ",\n";
    out << "      \"terminal_nodes\": ";
    WriteJsonStringArray(out, ending_decision->terminal_nodes);
    out << "\n";
    out << "    }";
  }
  out << "\n";
  out << "  },\n";
}

std::string SerializeSolveSeriesJson(const amflow::ProblemSpec& problem_spec,
                                     const DirectSolveSeriesSpec& direct_spec,
                                     const amflow::SolverDiagnostics& diagnostics,
                                     const amflow::SolverDiagnostics* retained_master_diagnostics,
                                     const int epsilon_order,
                                     const int digits,
                                     const std::string& status,
                                     const std::string& error,
                                     const double duration_seconds,
                                     const SolveSeriesRuntimeOptions& runtime_options,
                                     const std::optional<amflow::EtaInsertionDecision>&
                                         eta_mode_decision,
                                     const std::optional<amflow::EndingDecision>&
                                         ending_decision) {
  std::map<std::string, std::size_t> master_index_by_label;
  for (std::size_t index = 0; index < direct_spec.masters.size(); ++index) {
    const auto& master = direct_spec.masters[index];
    master_index_by_label.emplace(IntegralLabel(master.family, master.indices), index);
  }
  std::vector<SolveSeriesOutputIntegral> output_integrals;
  output_integrals.reserve(problem_spec.targets.size() +
                           direct_spec.retained_reduction_masters.size());
  std::set<std::string> output_labels;
  for (std::size_t index = 0; index < problem_spec.targets.size(); ++index) {
    const std::string label = problem_spec.targets[index].Label();
    output_integrals.push_back({label, index, std::nullopt});
    output_labels.insert(label);
  }
  if (retained_master_diagnostics != nullptr) {
    for (const amflow::MasterIntegral& master : direct_spec.retained_reduction_masters) {
      const std::string label = IntegralLabel(master.family, master.indices);
      const auto master_it = master_index_by_label.find(label);
      if (master_it == master_index_by_label.end() ||
          output_labels.find(label) != output_labels.end()) {
        continue;
      }
      output_integrals.push_back({label, std::nullopt, master_it->second});
      output_labels.insert(label);
    }
  }

  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(6);
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  if (!direct_spec.benchmark_id.empty()) {
    out << "  \"benchmark_id\": " << JsonString(direct_spec.benchmark_id) << ",\n";
  }
  out << "  \"family\": " << JsonString(problem_spec.family.name) << ",\n";
  out << "  \"targets\": [";
  for (std::size_t index = 0; index < output_integrals.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << JsonString(output_integrals[index].label);
  }
  out << "],\n";
  out << "  \"solver\": {\n";
  out << "    \"precision_digits\": " << digits << ",\n";
  out << "    \"epsilon_order\": " << epsilon_order << "\n";
  out << "  },\n";
  AppendSolveSeriesAmfOptionsJson(out,
                                  runtime_options,
                                  eta_mode_decision,
                                  ending_decision);
  if (direct_spec.amflow_state_input) {
    const bool phase_space_state = IsPhaseSpaceAmflowState(direct_spec);
    const bool solution_sample_state = UsesRetainedSolutionSamples(direct_spec);
    const bool loop_solution_state = IsRetainedLoopSolutionSampleState(direct_spec);
    const bool finite_transport_applied =
        IsFiniteSolutionSampleState(direct_spec) &&
        AppliesFiniteSolutionSampleTransport(direct_spec);
    out << "  \"boundary_state\": {\n";
    out << "    \"kind\": " << JsonString(direct_spec.boundary_state_kind) << ",\n";
    out << "    \"location\": " << JsonString(direct_spec.start_location) << ",\n";
    out << "    \"direction\": " << JsonString(direct_spec.boundary_state_direction) << ",\n";
    out << "    \"epsilon_sample_count\": "
        << direct_spec.boundary_epsilon_samples.size() << ",\n";
    if (!direct_spec.integral_kind.empty()) {
      out << "    \"integral_kind\": " << JsonString(direct_spec.integral_kind) << ",\n";
    }
    out << "    \"accepted_by_solve_series\": true,\n";
    out << "    \"runtime_boundary_provider\": "
        << JsonString(solution_sample_state
                          ? (status == "success"
                                 ? (phase_space_state
                                        ? "retained-phase-space-solution-sample-cache-"
                                          "laurent-fit"
                                    : loop_solution_state
                                        ? "retained-loop-solution-sample-cache-laurent-fit"
                                    : finite_transport_applied
                                        ? "retained-finite-solution-sample-boundary+"
                                          "finite-de-transport"
                                        : "retained-finite-solution-sample-cache")
                                 : (phase_space_state
                                        ? "deferred-phase-space-solution-sample-provider"
                                    : loop_solution_state
                                        ? "deferred-loop-solution-sample-provider"
                                        : "deferred-finite-solution-sample-provider"))
                      : status == "success"
                          ? (diagnostics.eta_endpoint_transport_count > 0
                                 ? "retained-asymptotic-subsystem-sample-boundary-evaluator+"
                                   "eta-infinity-de-asymptotic-transport+"
                                   "eta-zero-selected-endpoint-transport"
                             : diagnostics.eta_asymptotic_transport_count > 0
                                 ? "retained-asymptotic-subsystem-sample-boundary-evaluator+"
                                   "eta-infinity-de-asymptotic-transport"
                                 : "retained-asymptotic-subsystem-sample-boundary-evaluator")
                          : "deferred-asymptotic-subsystem-sample-provider")
        << "\n";
    out << "  },\n";
    out << "  \"continuation\": {\n";
    out << "    \"variable\": " << JsonString(direct_spec.variable) << ",\n";
    out << "    \"start_location\": " << JsonString(direct_spec.start_location) << ",\n";
    out << "    \"target_location\": " << JsonString(direct_spec.target_location) << ",\n";
    out << "    \"singular_points\": [";
    for (std::size_t index = 0; index < direct_spec.singular_points.size(); ++index) {
      if (index > 0) {
        out << ", ";
      }
      out << JsonString(direct_spec.singular_points[index]);
    }
    out << "],\n";
    out << "    \"transport_applied\": "
        << (diagnostics.eta_endpoint_transport_count > 0 || finite_transport_applied
                ? "true"
                : "false")
        << ",\n";
    out << "    \"transport_scope\": "
        << JsonString(solution_sample_state
                          ? (phase_space_state ? "phase-space-solution-samples"
                             : loop_solution_state ? "loop-solution-samples"
                                               : "finite-solution-samples")
                      : diagnostics.eta_endpoint_transport_count > 0
                          ? "eta-zero-selected-endpoint-coefficients"
                      : diagnostics.eta_asymptotic_transport_count > 0
                          ? "eta-infinity-asymptotic-only"
                          : "none")
        << ",\n";
    out << "    \"full_eta_zero_contour_applied\": false,\n";
    out << "    \"eta_infinity_asymptotic_transport_applied\": "
        << (diagnostics.eta_asymptotic_transport_count > 0 ? "true" : "false") << ",\n";
    out << "    \"eta_infinity_asymptotic_transported_master_count\": "
        << diagnostics.eta_asymptotic_transport_count << ",\n";
    out << "    \"eta_zero_endpoint_transport_applied\": "
        << (diagnostics.eta_endpoint_transport_count > 0 ? "true" : "false") << ",\n";
    out << "    \"eta_zero_endpoint_transported_master_count\": "
        << diagnostics.eta_endpoint_transport_count << ",\n";
    out << "    \"eta_zero_endpoint_transported_integrals\": [";
    for (std::size_t index = 0;
         index < diagnostics.eta_endpoint_transported_integrals.size();
         ++index) {
      if (index > 0) {
        out << ", ";
      }
      out << JsonString(diagnostics.eta_endpoint_transported_integrals[index]);
    }
    out << "],\n";
    out << "    \"runtime_application\": "
        << JsonString(solution_sample_state
                          ? (phase_space_state ? "phase-space-solution-sample-laurent-fit"
                             : loop_solution_state ? "loop-solution-sample-laurent-fit"
                             : finite_transport_applied ? "finite-de-transport"
                                                        : "finite-solution-sample-ingest")
                      : diagnostics.eta_endpoint_transport_count > 0
                          ? "eta-infinity-de-asymptotic-first-coefficient+"
                            "eta-zero-selected-endpoint-coefficients"
                      : diagnostics.eta_asymptotic_transport_count > 0
                          ? "eta-infinity-de-asymptotic-first-coefficient"
                          : "not-applied-boundary-only")
        << ",\n";
    out << "    \"blocked_reason\": "
        << JsonString(solution_sample_state
                          ? (phase_space_state
                             ? "full Cutkosky phase-space boundary reconstruction from cut "
                               "propagators remains deferred after retained solution-sample "
                               "coefficient fitting"
                         : loop_solution_state
                             ? "full complex eta-contour endpoint reconstruction remains "
                               "deferred after retained loop solution-sample coefficient "
                               "fitting"
                         : finite_transport_applied
                             ? (direct_spec.finite_solution_basis_reduction_path.empty()
                                    ? "solution-only integrals outside the transported finite "
                                      "DE master basis remain deferred"
                                    : "full live finite-start boundary solving remains "
                                      "deferred after retained finite DE transport and "
                                      "solution-basis reconstruction")
                             : "full AMFlow loop-boundary reconstruction and endpoint "
                               "contour execution remain deferred after retained finite "
                               "solution-sample ingestion")
                      : diagnostics.eta_endpoint_transport_count > 0
                          ? EndpointTransportDeferredReason(diagnostics)
                      : diagnostics.eta_asymptotic_transport_count > 0
                          ? "singular eta=0 complex contour execution and endpoint extraction "
                            "remain deferred after first eta-infinity asymptotic DE transport"
                          : "eta-infinity start, complex contour execution, and singular eta=0 "
                            "endpoint extraction remain deferred")
        << "\n";
    out << "  },\n";
  }
  if (!direct_spec.target_reduction_path.empty()) {
    out << "  \"target_reduction\": {\n";
    out << "    \"path\": " << JsonString(direct_spec.target_reduction_path) << ",\n";
    out << "    \"accepted_by_solve_series\": true,\n";
    out << "    \"runtime_application\": "
        << JsonString(status == "success"
                          ? (direct_spec.amflow_state_input
                                 ? (UsesRetainedSolutionSamples(direct_spec)
                                        ? (IsPhaseSpaceAmflowState(direct_spec)
                                            ? "applied-after-phase-space-solution-sample-fit"
                                        : IsRetainedLoopSolutionSampleState(direct_spec)
                                            ? "applied-after-loop-solution-sample-fit"
                                        : AppliesFiniteSolutionSampleTransport(direct_spec)
                                            ? "applied-after-finite-de-transport"
                                            : "applied-after-finite-solution-sample-ingest")
                                    : diagnostics.eta_endpoint_transport_count > 0
                                        ? "applied-after-eta-zero-selected-endpoint-transport"
                                    : diagnostics.eta_asymptotic_transport_count > 0
                                        ? "applied-after-eta-infinity-asymptotic-de-transport"
                                        : "applied-after-eta-infinity-boundary-evaluation")
                                 : "applied-after-master-solve")
                          : "deferred-until-master-values")
        << "\n";
    out << "  },\n";
  }
  out << "  \"results\": [\n";
  bool wrote_result = false;
  AppendSolveSeriesResultEntries(out,
                                 problem_spec,
                                 direct_spec,
                                 diagnostics,
                                 retained_master_diagnostics,
                                 status,
                                 digits,
                                 wrote_result);
  out << "\n";
  out << "  ],\n";
  out << "  \"status\": " << JsonString(status) << ",\n";
  out << "  \"duration_seconds\": " << duration_seconds;
  if (!diagnostics.failure_code.empty()) {
    out << ",\n  \"failure_code\": " << JsonString(diagnostics.failure_code);
  }
  if (!diagnostics.summary.empty()) {
    out << ",\n  \"summary\": " << JsonString(diagnostics.summary);
  }
  if (!error.empty()) {
    out << ",\n  \"error\": " << JsonString(error);
  }
  out << "\n}\n";
  return out.str();
}

struct SolveSeriesEvaluation {
  amflow::ProblemSpec problem_spec;
  DirectSolveSeriesSpec direct_spec;
  amflow::SolverDiagnostics diagnostics;
  std::optional<amflow::SolverDiagnostics> retained_master_diagnostics;
  SolveSeriesRuntimeOptions runtime_options;
  std::optional<amflow::EtaInsertionDecision> eta_mode_decision;
  std::optional<amflow::EndingDecision> ending_decision;
  std::string status = "failed";
  std::string error;
  int exit_code = 0;
};

SolveSeriesEvaluation EvaluateSolveSeriesInput(
    amflow::ProblemSpec problem_spec,
    DirectSolveSeriesSpec direct_spec,
    const int epsilon_order,
    const int digits,
    const SolveSeriesRuntimeOptions& runtime_options) {
  if (direct_spec.family.empty()) {
    direct_spec.family = problem_spec.family.name;
  }

  SolveSeriesEvaluation evaluation;
  evaluation.problem_spec = std::move(problem_spec);
  evaluation.direct_spec = std::move(direct_spec);
  evaluation.runtime_options = runtime_options;
  if (runtime_options.spacetime_dimension.has_value()) {
    evaluation.problem_spec.dimension = *runtime_options.spacetime_dimension;
  }

  try {
    if (evaluation.direct_spec.benchmark_id == "user_defined_amfmode" &&
        !IsSingleString(runtime_options.amf_options.amf_modes, "usr")) {
      throw std::runtime_error(
          "user_defined_amfmode retained state requires --amfmode usr to bind "
          "the reviewed AMFlow user-defined AMFPosition hook");
    }
    if (evaluation.direct_spec.benchmark_id == "user_defined_ending" &&
        !IsSingleString(runtime_options.amf_options.ending_schemes, "usr")) {
      throw std::runtime_error(
          "user_defined_ending retained state requires --ending usr to bind "
          "the reviewed AMFlow user-defined EndingScheme hook");
    }
    if (runtime_options.amf_modes_overridden) {
      evaluation.eta_mode_decision =
          amflow::PlanAmfOptionsEtaMode(evaluation.problem_spec,
                                        runtime_options.amf_options,
                                        MakeCliUserDefinedEtaModes());
    }
    if (runtime_options.ending_schemes_overridden) {
      evaluation.ending_decision =
          amflow::PlanAmfOptionsEndingScheme(evaluation.problem_spec,
                                             runtime_options.amf_options,
                                             MakeCliUserDefinedEndingSchemes());
    }
    ValidateDirectSolveSeriesSpec(evaluation.direct_spec);
    if (evaluation.direct_spec.amflow_state_input) {
      evaluation.diagnostics =
          UsesRetainedSolutionSamples(evaluation.direct_spec)
              ? EvaluateAmflowStateRetainedSolutionSamples(evaluation.direct_spec,
                                                           epsilon_order)
              : EvaluateAmflowStateEtaInfinityBoundary(evaluation.direct_spec,
                                                       epsilon_order);
      evaluation.retained_master_diagnostics = evaluation.diagnostics;
      const bool applied_target_reduction =
          ApplyDirectSpecTargetReductionIfPresent(evaluation.direct_spec,
                                                  evaluation.problem_spec.targets,
                                                  evaluation.problem_spec.dimension,
                                                  epsilon_order,
                                                  evaluation.diagnostics,
                                                  evaluation.error);
      if (!evaluation.error.empty()) {
        evaluation.status = "failed";
        evaluation.exit_code = 2;
      } else {
        const std::size_t required_result_count =
            (applied_target_reduction ||
             UsesRetainedSolutionSamples(evaluation.direct_spec))
                ? evaluation.problem_spec.targets.size()
                : evaluation.direct_spec.masters.size();
        const bool has_all_target_values =
            evaluation.diagnostics.target_values.size() >= required_result_count;
        const bool has_all_epsilon_coefficients =
            evaluation.diagnostics.target_epsilon_coefficients.size() >=
            required_result_count;
        if (evaluation.diagnostics.success && has_all_target_values &&
            has_all_epsilon_coefficients) {
          evaluation.status = "success";
          evaluation.exit_code = 0;
        } else {
          evaluation.status = "failed";
          evaluation.error =
              UsesRetainedSolutionSamples(evaluation.direct_spec)
                  ? "AMFlow retained solution-sample evaluation completed without "
                    "enough coefficients for all requested results"
                  : "AMFlow eta-infinity boundary evaluation completed without enough "
                    "coefficients for all requested results";
          evaluation.exit_code = 4;
        }
      }
    } else {
      const bool needs_epsilon_expansion =
          epsilon_order > 0 || DirectSolveSeriesSpecContainsEpsilon(evaluation.direct_spec);
      const std::optional<int> requested_epsilon_order =
          needs_epsilon_expansion ? std::optional<int>{epsilon_order} : std::nullopt;
      const amflow::SolveRequest request =
          MakeDirectSolveRequest(evaluation.direct_spec,
                                 digits,
                                 requested_epsilon_order,
                                 evaluation.problem_spec.dimension);
      const std::unique_ptr<amflow::SeriesSolver> solver =
          amflow::MakeBootstrapSeriesSolver();
      evaluation.diagnostics = solver->Solve(request);
      if (evaluation.diagnostics.success) {
        const bool applied_target_reduction =
            ApplyDirectSpecTargetReductionIfPresent(evaluation.direct_spec,
                                                    evaluation.problem_spec.targets,
                                                    evaluation.problem_spec.dimension,
                                                    epsilon_order,
                                                    evaluation.diagnostics,
                                                    evaluation.error);
        if (!evaluation.error.empty()) {
          evaluation.status = "failed";
          evaluation.exit_code = 2;
        } else {
          const std::size_t required_result_count =
              applied_target_reduction ? evaluation.problem_spec.targets.size()
                                       : evaluation.direct_spec.masters.size();
          const bool has_all_target_values =
              evaluation.diagnostics.target_values.size() >= required_result_count;
          const bool has_all_epsilon_coefficients =
              !requested_epsilon_order.has_value() ||
              evaluation.diagnostics.target_epsilon_coefficients.size() >=
                  required_result_count;
          if (has_all_target_values && has_all_epsilon_coefficients) {
            evaluation.status = "success";
            evaluation.exit_code = 0;
          } else {
            evaluation.status = "failed";
            evaluation.error =
                applied_target_reduction
                    ? "series solver succeeded and target reduction ran, but reduced target "
                      "coefficients were incomplete"
                    : "series solver succeeded but did not expose transported epsilon "
                      "coefficients for all masters on this path";
            evaluation.exit_code = 4;
          }
        }
      } else {
        evaluation.status = "failed";
        evaluation.exit_code = 4;
      }
    }
  } catch (const std::exception& solve_error) {
    evaluation.status = "failed";
    evaluation.error = solve_error.what();
    evaluation.exit_code = 2;
  }
  return evaluation;
}

std::vector<std::string> BundleOutputLabels(const SolveSeriesEvaluation& evaluation) {
  std::vector<std::string> labels;
  std::set<std::string> seen;
  for (const amflow::TargetIntegral& target : evaluation.problem_spec.targets) {
    const std::string label = target.Label();
    if (seen.insert(label).second) {
      labels.push_back(label);
    }
  }
  if (evaluation.retained_master_diagnostics.has_value()) {
    for (const amflow::MasterIntegral& master :
         evaluation.direct_spec.retained_reduction_masters) {
      const std::string label = IntegralLabel(master.family, master.indices);
      if (seen.insert(label).second) {
        labels.push_back(label);
      }
    }
  }
  return labels;
}

std::string SerializeSolveSeriesBundleJson(
    const std::vector<SolveSeriesEvaluation>& evaluations,
    const int epsilon_order,
    const int digits,
    const double duration_seconds,
    const SolveSeriesRuntimeOptions& runtime_options) {
  const bool all_success = std::all_of(
      evaluations.begin(),
      evaluations.end(),
      [](const SolveSeriesEvaluation& evaluation) {
        return evaluation.status == "success";
      });
  std::vector<std::string> families;
  std::vector<std::string> targets;
  std::set<std::string> seen_families;
  for (const SolveSeriesEvaluation& evaluation : evaluations) {
    if (seen_families.insert(evaluation.problem_spec.family.name).second) {
      families.push_back(evaluation.problem_spec.family.name);
    }
    const std::vector<std::string> labels = BundleOutputLabels(evaluation);
    targets.insert(targets.end(), labels.begin(), labels.end());
  }

  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(6);
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  if (!evaluations.empty() && !evaluations.front().direct_spec.benchmark_id.empty()) {
    out << "  \"benchmark_id\": "
        << JsonString(evaluations.front().direct_spec.benchmark_id) << ",\n";
  }
  out << "  \"family\": \"multiple\",\n";
  out << "  \"families\": [";
  for (std::size_t index = 0; index < families.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << JsonString(families[index]);
  }
  out << "],\n";
  out << "  \"state_count\": " << evaluations.size() << ",\n";
  out << "  \"targets\": [";
  for (std::size_t index = 0; index < targets.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << JsonString(targets[index]);
  }
  out << "],\n";
  out << "  \"solver\": {\n";
  out << "    \"precision_digits\": " << digits << ",\n";
  out << "    \"epsilon_order\": " << epsilon_order << "\n";
  out << "  },\n";
  AppendSolveSeriesAmfOptionsJson(out, runtime_options, std::nullopt, std::nullopt);
  out << "  \"results\": [\n";
  bool wrote_result = false;
  for (const SolveSeriesEvaluation& evaluation : evaluations) {
    AppendSolveSeriesResultEntries(
        out,
        evaluation.problem_spec,
        evaluation.direct_spec,
        evaluation.diagnostics,
        evaluation.retained_master_diagnostics.has_value()
            ? &*evaluation.retained_master_diagnostics
            : nullptr,
        evaluation.status,
        digits,
        wrote_result);
  }
  out << "\n";
  out << "  ],\n";
  out << "  \"status\": " << JsonString(all_success ? "success" : "failed") << ",\n";
  out << "  \"duration_seconds\": " << duration_seconds << ",\n";
  out << "  \"state_results\": [";
  for (std::size_t index = 0; index < evaluations.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    const SolveSeriesEvaluation& evaluation = evaluations[index];
    const DirectSolveSeriesSpec& direct_spec = evaluation.direct_spec;
    const amflow::SolverDiagnostics& diagnostics = evaluation.diagnostics;
    const bool phase_space_state = IsPhaseSpaceAmflowState(direct_spec);
    const bool solution_sample_state = UsesRetainedSolutionSamples(direct_spec);
    const bool loop_solution_state = IsRetainedLoopSolutionSampleState(direct_spec);
    const bool finite_transport_applied =
        IsFiniteSolutionSampleState(direct_spec) &&
        AppliesFiniteSolutionSampleTransport(direct_spec);
    out << "{"
        << "\"family\": " << JsonString(evaluation.problem_spec.family.name)
        << ", \"amflow_output_name\": " << JsonString(direct_spec.amflow_output_name)
        << ", \"status\": " << JsonString(evaluation.status)
        << ", \"boundary_state\": {"
        << "\"kind\": " << JsonString(direct_spec.boundary_state_kind)
        << ", \"location\": " << JsonString(direct_spec.start_location)
        << ", \"direction\": " << JsonString(direct_spec.boundary_state_direction)
        << ", \"epsilon_sample_count\": "
        << direct_spec.boundary_epsilon_samples.size()
        << ", \"accepted_by_solve_series\": true"
        << ", \"runtime_boundary_provider\": "
        << JsonString(solution_sample_state
                          ? (evaluation.status == "success"
                                 ? (phase_space_state
                                        ? "retained-phase-space-solution-sample-cache-"
                                          "laurent-fit"
                                    : loop_solution_state
                                        ? "retained-loop-solution-sample-cache-laurent-fit"
                                    : finite_transport_applied
                                        ? "retained-finite-solution-sample-boundary+"
                                          "finite-de-transport"
                                        : "retained-finite-solution-sample-cache")
                                 : (phase_space_state
                                        ? "deferred-phase-space-solution-sample-provider"
                                    : loop_solution_state
                                        ? "deferred-loop-solution-sample-provider"
                                        : "deferred-finite-solution-sample-provider"))
                      : evaluation.status == "success"
                          ? (diagnostics.eta_endpoint_transport_count > 0
                                 ? "retained-asymptotic-subsystem-sample-boundary-evaluator+"
                                   "eta-infinity-de-asymptotic-transport+"
                                   "eta-zero-selected-endpoint-transport"
                             : diagnostics.eta_asymptotic_transport_count > 0
                                 ? "retained-asymptotic-subsystem-sample-boundary-evaluator+"
                                   "eta-infinity-de-asymptotic-transport"
                                 : "retained-asymptotic-subsystem-sample-boundary-evaluator")
                          : "deferred-asymptotic-subsystem-sample-provider")
        << "}, \"continuation\": {"
        << "\"variable\": " << JsonString(direct_spec.variable)
        << ", \"start_location\": " << JsonString(direct_spec.start_location)
        << ", \"target_location\": " << JsonString(direct_spec.target_location)
        << ", \"singular_points\": [";
    for (std::size_t singular_index = 0;
         singular_index < direct_spec.singular_points.size();
         ++singular_index) {
      if (singular_index > 0) {
        out << ", ";
      }
      out << JsonString(direct_spec.singular_points[singular_index]);
    }
    out << "]"
        << ", \"transport_applied\": "
        << (diagnostics.eta_endpoint_transport_count > 0 || finite_transport_applied
                ? "true"
                : "false")
        << ", \"transport_scope\": "
        << JsonString(solution_sample_state
                          ? (phase_space_state ? "phase-space-solution-samples"
                             : loop_solution_state ? "loop-solution-samples"
                                               : "finite-solution-samples")
                      : diagnostics.eta_endpoint_transport_count > 0
                          ? "eta-zero-selected-endpoint-coefficients"
                      : diagnostics.eta_asymptotic_transport_count > 0
                          ? "eta-infinity-asymptotic-only"
                          : "none")
        << ", \"full_eta_zero_contour_applied\": false"
        << ", \"eta_infinity_asymptotic_transport_applied\": "
        << (diagnostics.eta_asymptotic_transport_count > 0 ? "true" : "false")
        << ", \"eta_infinity_asymptotic_transported_master_count\": "
        << diagnostics.eta_asymptotic_transport_count
        << ", \"eta_zero_endpoint_transport_applied\": "
        << (diagnostics.eta_endpoint_transport_count > 0 ? "true" : "false")
        << ", \"eta_zero_endpoint_transported_master_count\": "
        << diagnostics.eta_endpoint_transport_count
        << ", \"eta_zero_endpoint_transported_integrals\": [";
    for (std::size_t transport_index = 0;
         transport_index <
         diagnostics.eta_endpoint_transported_integrals.size();
         ++transport_index) {
      if (transport_index > 0) {
        out << ", ";
      }
      out << JsonString(diagnostics.eta_endpoint_transported_integrals[transport_index]);
    }
    out << "]"
        << ", \"runtime_application\": "
        << JsonString(solution_sample_state
                          ? (phase_space_state ? "phase-space-solution-sample-laurent-fit"
                             : loop_solution_state ? "loop-solution-sample-laurent-fit"
                             : finite_transport_applied ? "finite-de-transport"
                                                        : "finite-solution-sample-ingest")
                      : diagnostics.eta_endpoint_transport_count > 0
                          ? "eta-infinity-de-asymptotic-first-coefficient+"
                            "eta-zero-selected-endpoint-coefficients"
                      : diagnostics.eta_asymptotic_transport_count > 0
                          ? "eta-infinity-de-asymptotic-first-coefficient"
                          : "not-applied-boundary-only")
        << ", \"blocked_reason\": "
        << JsonString(solution_sample_state
                          ? (phase_space_state
                             ? "full Cutkosky phase-space boundary reconstruction from cut "
                               "propagators remains deferred after retained solution-sample "
                               "coefficient fitting"
                         : loop_solution_state
                             ? "full complex eta-contour endpoint reconstruction remains "
                               "deferred after retained loop solution-sample coefficient "
                               "fitting"
                         : finite_transport_applied
                             ? (direct_spec.finite_solution_basis_reduction_path.empty()
                                    ? "solution-only integrals outside the transported finite "
                                      "DE master basis remain deferred"
                                    : "full live finite-start boundary solving remains "
                                      "deferred after retained finite DE transport and "
                                      "solution-basis reconstruction")
                             : "full AMFlow loop-boundary reconstruction and endpoint "
                               "contour execution remain deferred after retained finite "
                               "solution-sample ingestion")
                      : diagnostics.eta_endpoint_transport_count > 0
                          ? EndpointTransportDeferredReason(diagnostics)
                      : diagnostics.eta_asymptotic_transport_count > 0
                          ? "singular eta=0 complex contour execution and endpoint extraction "
                            "remain deferred after first eta-infinity asymptotic DE transport"
                          : "eta-infinity start, complex contour execution, and singular eta=0 "
                            "endpoint extraction remain deferred")
        << "}";
    if (!direct_spec.target_reduction_path.empty()) {
      out << ", \"target_reduction\": {"
          << "\"path\": " << JsonString(direct_spec.target_reduction_path)
          << ", \"accepted_by_solve_series\": true"
          << ", \"runtime_application\": "
          << JsonString(evaluation.status == "success"
                            ? (solution_sample_state
                                   ? (phase_space_state
                                          ? "applied-after-phase-space-solution-sample-fit"
                                      : loop_solution_state
                                          ? "applied-after-loop-solution-sample-fit"
                                      : finite_transport_applied
                                          ? "applied-after-finite-de-transport"
                                          : "applied-after-finite-solution-sample-ingest")
                               : diagnostics.eta_endpoint_transport_count > 0
                                   ? "applied-after-eta-zero-selected-endpoint-transport"
                               : diagnostics.eta_asymptotic_transport_count > 0
                                   ? "applied-after-eta-infinity-asymptotic-de-transport"
                                   : "applied-after-eta-infinity-boundary-evaluation")
                            : "deferred-until-master-values")
          << "}";
    }
    out << "}";
  }
  out << "]";
  std::vector<std::string> errors;
  std::vector<std::string> summaries;
  for (const SolveSeriesEvaluation& evaluation : evaluations) {
    if (!evaluation.error.empty()) {
      errors.push_back(evaluation.problem_spec.family.name + ": " + evaluation.error);
    }
    if (!evaluation.diagnostics.summary.empty()) {
      summaries.push_back(evaluation.problem_spec.family.name + ": " +
                          evaluation.diagnostics.summary);
    }
  }
  if (!summaries.empty()) {
    out << ",\n  \"summary\": ";
    std::string summary;
    for (std::size_t index = 0; index < summaries.size(); ++index) {
      if (index > 0) {
        summary += " ";
      }
      summary += summaries[index];
    }
    out << JsonString(summary);
  }
  if (!errors.empty()) {
    out << ",\n  \"errors\": [";
    for (std::size_t index = 0; index < errors.size(); ++index) {
      if (index > 0) {
        out << ", ";
      }
      out << JsonString(errors[index]);
    }
    out << "]";
  }
  out << "\n}\n";
  return out.str();
}

int RunSolveSeriesCommand(const int argc, char** argv) {
  const auto start = std::chrono::steady_clock::now();
  const SolveSeriesCliArgs args = ParseSolveSeriesArgs(argc, argv);
  const std::string raw_spec = ReadTextFile(args.spec_path);

  amflow::ProblemSpec problem_spec;
  DirectSolveSeriesSpec direct_spec;
  if (LooksLikeJsonObject(raw_spec)) {
    const CliJsonValue root = CliJsonParser(raw_spec).Parse();
    const ParsedSolveSeriesJsonInput parsed_json =
        ParseSolveSeriesJsonInputRoot(root, "$", args.spec_path);
    if (parsed_json.bundle) {
      std::vector<SolveSeriesEvaluation> evaluations;
      evaluations.reserve(parsed_json.specs.size());
      int exit_code = 0;
      for (const DirectSolveSeriesSpec& state_spec : parsed_json.specs) {
        evaluations.push_back(EvaluateSolveSeriesInput(
            MakeProblemSpecForAmflowState(state_spec),
            state_spec,
            args.epsilon_order,
            args.digits,
            args.runtime_options));
        exit_code = std::max(exit_code, evaluations.back().exit_code);
      }
      const auto end = std::chrono::steady_clock::now();
      const double duration_seconds =
          std::chrono::duration<double>(end - start).count();
      WriteTextFile(args.output_path,
                    SerializeSolveSeriesBundleJson(evaluations,
                                                   args.epsilon_order,
                                                   args.digits,
                                                   duration_seconds,
                                                   args.runtime_options));
      for (const SolveSeriesEvaluation& evaluation : evaluations) {
        if (!evaluation.error.empty()) {
          std::cerr << evaluation.problem_spec.family.name << ": "
                    << evaluation.error << "\n";
        } else if (!evaluation.diagnostics.success &&
                   !evaluation.diagnostics.summary.empty()) {
          std::cerr << evaluation.problem_spec.family.name << ": "
                    << evaluation.diagnostics.summary << "\n";
        }
      }
      return exit_code;
    }
    direct_spec = parsed_json.specs.front();
    problem_spec = MakeProblemSpecForAmflowState(direct_spec);
  } else {
    problem_spec = amflow::LoadProblemSpecFile(args.spec_path);
    const auto messages = LoadedSpecValidationMessages(problem_spec);
    if (!messages.empty()) {
      PrintMessages(std::cerr, messages);
      return 2;
    }
    direct_spec = ParseDirectSolveSeriesSpec(raw_spec);
    if (!direct_spec.present && IsAutomaticVsManualSmokeProblemSpec(problem_spec)) {
      direct_spec = LoadAutomaticVsManualAmflowStateFallback(args.spec_path);
      problem_spec = MakeProblemSpecForAmflowState(direct_spec);
    }
  }

  const SolveSeriesEvaluation evaluation =
      EvaluateSolveSeriesInput(std::move(problem_spec),
                               std::move(direct_spec),
                               args.epsilon_order,
                               args.digits,
                               args.runtime_options);
  const auto end = std::chrono::steady_clock::now();
  const double duration_seconds =
      std::chrono::duration<double>(end - start).count();
  WriteTextFile(args.output_path,
                SerializeSolveSeriesJson(evaluation.problem_spec,
                                         evaluation.direct_spec,
                                         evaluation.diagnostics,
                                         evaluation.retained_master_diagnostics.has_value()
                                             ? &*evaluation.retained_master_diagnostics
                                             : nullptr,
                                         args.epsilon_order,
                                         args.digits,
                                         evaluation.status,
                                         evaluation.error,
                                         duration_seconds,
                                         evaluation.runtime_options,
                                         evaluation.eta_mode_decision,
                                         evaluation.ending_decision));
  if (!evaluation.error.empty()) {
    std::cerr << evaluation.error << "\n";
  } else if (!evaluation.diagnostics.success && !evaluation.diagnostics.summary.empty()) {
    std::cerr << evaluation.diagnostics.summary << "\n";
  }
  return evaluation.exit_code;
}

void PrintUsage() {
  std::cout << "Usage: amflow-cli <command> [args]\n"
            << "Commands:\n"
            << "  sample-problem           Print the bootstrap ProblemSpec YAML\n"
            << "  emit-kira [dir]          Emit Kira job files for the sample problem\n"
            << "  run-kira <kira> <fermat> [dir]\n"
            << "                           Emit and execute Kira for the sample problem\n"
            << "  load-spec <file>         Load a bootstrap YAML spec and print canonical YAML\n"
            << "  emit-kira-from-file <file> [dir]\n"
            << "                           Emit Kira job files for a file-backed ProblemSpec\n"
            << "  parse-kira-results <artifact-root> <family>\n"
            << "                           Parse Kira results/<family>/masters and kira_target.m\n"
            << "  run-kira-from-file <file> <kira> <fermat> [dir]\n"
            << "                           Emit and execute Kira for a file-backed ProblemSpec\n"
            << "  solve-series <file> --eps-order N --digits N --out path\n"
            << "                           [--spacetime-dimension D] [--amfmode X[,Y]] [--ending X[,Y]]\n"
            << "                           Run a reviewed embedded direct solve_series request or AMFlow state JSON/bundle\n"
            << "  show-defaults            Print bootstrap AMF and reduction defaults\n"
            << "  write-manifest <dir>     Create an artifact layout and write a sample/demo manifest\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      PrintUsage();
      return 1;
    }

    const std::string command = argv[1];
    const amflow::ProblemSpec sample_spec = amflow::MakeSampleProblemSpec();

    if (command == "sample-problem") {
      std::cout << amflow::SerializeProblemSpecYaml(sample_spec);
      return 0;
    }

    if (command == "load-spec") {
      if (argc < 3) {
        std::cerr << "load-spec requires a spec file path\n";
        return 1;
      }
      const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(argv[2]);
      const auto messages = LoadedSpecValidationMessages(spec);
      if (!messages.empty()) {
        PrintMessages(std::cerr, messages);
        return 2;
      }
      std::cout << amflow::SerializeProblemSpecYaml(spec);
      return 0;
    }

    if (command == "show-defaults") {
      std::cout << amflow::SerializeAmfOptionsYaml(amflow::AmfOptions{}) << "\n";
      std::cout << amflow::SerializeReductionOptionsYaml(amflow::ReductionOptions{}) << "\n";
      return 0;
    }

    if (command == "emit-kira") {
      const std::filesystem::path root = argc >= 3 ? argv[2] : "artifacts/bootstrap";
      return EmitKiraArtifacts(sample_spec, root);
    }

    if (command == "run-kira") {
      if (argc < 4) {
        std::cerr << "run-kira requires kira and fermat executable paths\n";
        return 1;
      }
      const std::filesystem::path root = argc >= 5 ? argv[4] : "artifacts/bootstrap";
      return RunKiraForSpec(sample_spec, root, argv[2], argv[3]);
    }

    if (command == "emit-kira-from-file") {
      if (argc < 3) {
        std::cerr << "emit-kira-from-file requires a spec file path\n";
        return 1;
      }

      const std::filesystem::path spec_path = argv[2];
      const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(spec_path);
      const auto messages = LoadedSpecValidationMessages(spec);
      if (!messages.empty()) {
        PrintMessages(std::cerr, messages);
        return 2;
      }

      const std::filesystem::path root =
          argc >= 4 ? std::filesystem::path(argv[3]) : DefaultArtifactRootForSpec(spec_path);
      return EmitKiraArtifacts(spec, root);
    }

    if (command == "parse-kira-results") {
      if (argc < 4) {
        std::cerr << "parse-kira-results requires an artifact root and family name\n";
        return 1;
      }
      amflow::KiraBackend backend;
      const amflow::ParsedReductionResult result =
          backend.ParseReductionResult(argv[2], argv[3]);
      PrintParsedReductionResult(std::cout, result);
      return 0;
    }

    if (command == "run-kira-from-file") {
      if (argc < 5) {
        std::cerr << "run-kira-from-file requires a spec file, kira path, and fermat path\n";
        return 1;
      }

      const std::filesystem::path spec_path = argv[2];
      const amflow::ProblemSpec spec = amflow::LoadProblemSpecFile(spec_path);
      const auto messages = LoadedSpecValidationMessages(spec);

      const std::filesystem::path root =
          argc >= 6 ? std::filesystem::path(argv[5]) : DefaultArtifactRootForSpec(spec_path);
      return RunKiraForSpec(spec, root, argv[3], argv[4], spec_path, messages);
    }

    if (command == "solve-series") {
      return RunSolveSeriesCommand(argc, argv);
    }

    if (command == "write-manifest") {
      if (argc < 3) {
        std::cerr << "write-manifest requires a target directory\n";
        return 1;
      }
      const std::filesystem::path root = argv[2];
      const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(root);
      const std::filesystem::path path =
          amflow::WriteArtifactManifest(layout, amflow::MakeBootstrapManifest());
      std::cout << path.string() << "\n";
      return 0;
    }

    PrintUsage();
    return 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 3;
  }
}
