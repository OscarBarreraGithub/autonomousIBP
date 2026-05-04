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
#include <optional>
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/math/special_functions/gamma.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include "amflow/core/options.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/io/problem_spec_io.hpp"
#include "amflow/io/sample_data.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/kira/target_reduction.hpp"
#include "amflow/runtime/artifact_store.hpp"
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
  std::string benchmark_id;
  std::string family;
  std::string variable;
  std::string start_location;
  std::string target_location;
  std::vector<amflow::MasterIntegral> masters;
  std::vector<amflow::TargetIntegral> targets;
  std::map<std::string, std::vector<std::vector<std::string>>> coefficient_matrices;
  std::vector<std::string> singular_points;
  std::vector<amflow::BoundaryCondition> boundary_conditions;
  std::string boundary_state_kind;
  std::string boundary_state_direction;
  std::vector<std::string> boundary_epsilon_samples;
  std::map<std::string, std::string> boundary_state_raw_files;
  std::string target_reduction_path;
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
  if (spec.coefficient_matrices.find(spec.variable) == spec.coefficient_matrices.end()) {
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

DirectSolveSeriesSpec ParseAmflowSolveSeriesStateJson(const std::string& json) {
  const CliJsonValue root = CliJsonParser(json).Parse();
  RequireJsonObject(root, "$");
  const std::string kind = RequireJsonString(RequireJsonField(root, "kind", "$"), "$.kind");
  if (kind != "amflow_solve_series_state") {
    throw std::invalid_argument(
        "solve-series JSON input must have kind \"amflow_solve_series_state\"");
  }
  if (RequireJsonInteger(RequireJsonField(root, "schema_version", "$"),
                         "$.schema_version") != 1) {
    throw std::invalid_argument("solve-series AMFlow state JSON schema_version must be 1");
  }

  DirectSolveSeriesSpec spec;
  spec.present = true;
  spec.amflow_state_input = true;
  spec.benchmark_id = OptionalJsonStringField(root, "benchmark_id", "");
  spec.family = RequireJsonString(RequireJsonField(root, "family", "$"), "$.family");
  spec.variable = RequireJsonString(RequireJsonField(root, "variable", "$"), "$.variable");
  spec.start_location = "infinity";
  spec.target_location = spec.variable + "=0";
  spec.masters = ParseAmflowStateMasters(RequireJsonField(root, "masters", "$"),
                                         "$.masters");
  spec.coefficient_matrices = ParseAmflowStateCoefficientMatrices(
      RequireJsonField(root, "coefficient_matrices", "$"), "$.coefficient_matrices");
  if (const CliJsonValue* singular_points = FindJsonField(root, "singular_points")) {
    spec.singular_points = RequireJsonStringArray(*singular_points, "$.singular_points");
    for (const std::string& singular_point : spec.singular_points) {
      if (TrimAsciiWhitespace(singular_point).empty()) {
        throw std::invalid_argument("$.singular_points entries must not be empty");
      }
    }
  }

  const CliJsonValue& boundary_state = RequireJsonField(root, "boundary_state", "$");
  spec.boundary_state_kind = RequireJsonString(
      RequireJsonField(boundary_state, "kind", "$.boundary_state"),
      "$.boundary_state.kind");
  if (spec.boundary_state_kind != "amflow_eta_infinity_asymptotic_with_subsystem_samples") {
    throw std::invalid_argument(
        "solve-series AMFlow state JSON carries unsupported boundary_state.kind: " +
        spec.boundary_state_kind);
  }
  if (const CliJsonValue* direction = FindJsonField(boundary_state, "direction")) {
    spec.boundary_state_direction =
        StripWrappedQuoteLiteral(RequireJsonString(*direction, "$.boundary_state.direction"));
  }
  if (const CliJsonValue* epsilon_samples = FindJsonField(boundary_state, "epsilon_samples")) {
    spec.boundary_epsilon_samples =
        RequireJsonStringArray(*epsilon_samples, "$.boundary_state.epsilon_samples");
  }
  if (const CliJsonValue* files = FindJsonField(boundary_state, "files")) {
    spec.boundary_state_raw_files =
        ParseAmflowStateBoundaryRawFiles(*files, "$.boundary_state.files");
  }

  if (const CliJsonValue* reduction = FindJsonField(root, "reduction")) {
    if (const CliJsonValue* target_reduction_path =
            FindJsonField(*reduction, "target_reduction_path")) {
      spec.target_reduction_path =
          RequireJsonString(*target_reduction_path, "$.reduction.target_reduction_path");
    }
    if (const CliJsonValue* targets = FindJsonField(*reduction, "targets")) {
      spec.targets = ParseAmflowStateTargets(*targets, "$.reduction.targets");
    }
  }
  if (spec.targets.empty()) {
    for (const amflow::MasterIntegral& master : spec.masters) {
      spec.targets.push_back({master.family, master.indices});
    }
  }
  return spec;
}

amflow::ProblemSpec MakeProblemSpecForAmflowState(const DirectSolveSeriesSpec& direct_spec) {
  amflow::ProblemSpec problem_spec;
  problem_spec.family.name = direct_spec.family;
  problem_spec.targets = direct_spec.targets;
  problem_spec.dimension = "4 - 2*eps";
  problem_spec.complex_mode = true;
  problem_spec.notes =
      "generated in-memory from amflow_solve_series_state for solve-series ingestion";
  return problem_spec;
}

using BigFloat = boost::multiprecision::cpp_dec_float_100;

struct BigComplex {
  BigFloat real = 0;
  BigFloat imaginary = 0;
};

BigComplex operator+(const BigComplex& lhs, const BigComplex& rhs) {
  return {lhs.real + rhs.real, lhs.imaginary + rhs.imaginary};
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

std::string RequireAmflowBoundaryRawFile(const DirectSolveSeriesSpec& spec,
                                         const std::string& name) {
  const auto it = spec.boundary_state_raw_files.find(name);
  if (it == spec.boundary_state_raw_files.end() || it->second.empty()) {
    throw std::runtime_error("AMFlow eta-infinity boundary state is missing raw file " +
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

BigFloat ParseRealBoundaryAtom(const std::string& raw) {
  std::string value = NormalizeMathematicaNumericAtom(raw);
  if (value.empty() || value == "+") {
    return BigFloat(1);
  }
  if (value == "-") {
    return BigFloat(-1);
  }
  if (value.rfind("+", 0) == 0) {
    value.erase(value.begin());
  }
  if (value.rfind("-Gamma[", 0) == 0 && value.back() == ']') {
    const std::string argument = value.substr(7, value.size() - 8);
    return -boost::math::tgamma(ParseBigFloatRational(argument));
  }
  if (value.rfind("Gamma[", 0) == 0 && value.back() == ']') {
    const std::string argument = value.substr(6, value.size() - 7);
    return boost::math::tgamma(ParseBigFloatRational(argument));
  }
  return ParseBigFloatRational(value);
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
      if (IsTiny(best_abs)) {
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
        if (IsTiny(factor)) {
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
  stream << std::fixed << std::setprecision(70) << value;
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

amflow::SolverDiagnostics EvaluateAmflowStateEtaInfinityBoundary(
    const DirectSolveSeriesSpec& direct_spec) {
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
  const std::vector<std::vector<BigComplex>> master_samples =
      EvaluateLeadingBoundarySamples(direct_spec, regions, boundary_mi);

  std::vector<BigFloat> epsilon_values;
  epsilon_values.reserve(direct_spec.boundary_epsilon_samples.size());
  for (const std::string& sample : direct_spec.boundary_epsilon_samples) {
    epsilon_values.push_back(ParseBigFloatRational(sample));
  }

  amflow::SolverDiagnostics diagnostics;
  diagnostics.success = true;
  diagnostics.residual_norm = 0.0;
  diagnostics.overlap_mismatch = 0.0;
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

  diagnostics.summary =
      "Evaluated retained AMFlow eta-infinity leading boundary coefficients from " +
      std::to_string(regions.size()) + " subsystem-sample regions and " +
      std::to_string(direct_spec.boundary_epsilon_samples.size()) +
      " epsilon samples. Singular eta->0 complex continuation is not applied on this path; "
      "the solve result records the reviewed Gap B continuation audit separately.";
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
  diagnostics.summary +=
      direct_spec.amflow_state_input
          ? "Applied retained Kira target reduction to eta-infinity boundary coefficients."
          : "Applied retained Kira target reduction to endpoint master values.";
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
  if (requested_epsilon_order.has_value() && !dimension_expression.empty()) {
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
};

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

SolveSeriesCliArgs ParseSolveSeriesArgs(const int argc, char** argv) {
  if (argc < 3) {
    throw std::invalid_argument("solve-series requires a spec file path");
  }
  SolveSeriesCliArgs args;
  args.spec_path = argv[2];
  std::set<std::string> seen_flags;
  for (int index = 3; index < argc; ++index) {
    const std::string flag = argv[index];
    if (flag != "--eps-order" && flag != "--digits" && flag != "--out") {
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
    } else {
      args.output_path = value;
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

std::string SerializeSolveSeriesJson(const amflow::ProblemSpec& problem_spec,
                                     const DirectSolveSeriesSpec& direct_spec,
                                     const amflow::SolverDiagnostics& diagnostics,
                                     const int epsilon_order,
                                     const int digits,
                                     const std::string& status,
                                     const std::string& error,
                                     const double duration_seconds) {
  std::map<std::string, std::size_t> master_index_by_label;
  for (std::size_t index = 0; index < direct_spec.masters.size(); ++index) {
    const auto& master = direct_spec.masters[index];
    master_index_by_label.emplace(IntegralLabel(master.family, master.indices), index);
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
  for (std::size_t index = 0; index < problem_spec.targets.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << JsonString(problem_spec.targets[index].Label());
  }
  out << "],\n";
  out << "  \"solver\": {\n";
  out << "    \"precision_digits\": " << digits << ",\n";
  out << "    \"epsilon_order\": " << epsilon_order << "\n";
  out << "  },\n";
  if (direct_spec.amflow_state_input) {
    out << "  \"boundary_state\": {\n";
    out << "    \"kind\": " << JsonString(direct_spec.boundary_state_kind) << ",\n";
    out << "    \"location\": " << JsonString(direct_spec.start_location) << ",\n";
    out << "    \"direction\": " << JsonString(direct_spec.boundary_state_direction) << ",\n";
    out << "    \"epsilon_sample_count\": "
        << direct_spec.boundary_epsilon_samples.size() << ",\n";
    out << "    \"accepted_by_solve_series\": true,\n";
    out << "    \"runtime_boundary_provider\": "
        << JsonString(status == "success"
                          ? "retained-asymptotic-subsystem-sample-boundary-evaluator"
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
    out << "    \"transport_applied\": false,\n";
    out << "    \"runtime_application\": \"not-applied-boundary-only\",\n";
    out << "    \"blocked_reason\": "
        << JsonString("eta-infinity start, complex contour execution, and singular eta=0 "
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
                                 ? "applied-after-eta-infinity-boundary-evaluation"
                                 : "applied-after-master-solve")
                          : "deferred-until-master-values")
        << "\n";
    out << "  },\n";
  }
  out << "  \"results\": [\n";
  for (std::size_t index = 0; index < problem_spec.targets.size(); ++index) {
    const std::string label = problem_spec.targets[index].Label();
    if (index > 0) {
      out << ",\n";
    }
    out << "    {\n";
    out << "      \"integral\": " << JsonString(label) << ",\n";
    out << "      \"epsilon_orders\": [";
    const auto master_it = master_index_by_label.find(label);
    const bool target_reduction_applied =
        !direct_spec.target_reduction_path.empty() && status == "success";
    const bool target_aligned_epsilon =
        target_reduction_applied &&
        diagnostics.target_epsilon_coefficients.size() == problem_spec.targets.size();
    const bool target_aligned_values =
        target_reduction_applied &&
        diagnostics.target_values.size() == problem_spec.targets.size();
    const std::optional<std::size_t> result_index =
        target_aligned_epsilon
            ? std::optional<std::size_t>{index}
            : (master_it != master_index_by_label.end()
                   ? std::optional<std::size_t>{master_it->second}
                   : std::nullopt);
    const std::optional<std::size_t> value_index =
        target_aligned_values
            ? std::optional<std::size_t>{index}
            : (master_it != master_index_by_label.end()
                   ? std::optional<std::size_t>{master_it->second}
                   : std::nullopt);
    if (status == "success" && result_index.has_value()) {
      if (*result_index < diagnostics.target_epsilon_coefficients.size() &&
          !diagnostics.target_epsilon_coefficients[*result_index].empty()) {
        const auto& coefficients = diagnostics.target_epsilon_coefficients[*result_index];
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
      } else if (value_index.has_value() && *value_index < diagnostics.target_values.size()) {
        const std::string exact_real = diagnostics.target_values[*value_index];
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

int RunSolveSeriesCommand(const int argc, char** argv) {
  const auto start = std::chrono::steady_clock::now();
  const SolveSeriesCliArgs args = ParseSolveSeriesArgs(argc, argv);
  const std::string raw_spec = ReadTextFile(args.spec_path);

  amflow::ProblemSpec problem_spec;
  DirectSolveSeriesSpec direct_spec;
  if (LooksLikeJsonObject(raw_spec)) {
    direct_spec = ParseAmflowSolveSeriesStateJson(raw_spec);
    problem_spec = MakeProblemSpecForAmflowState(direct_spec);
  } else {
    problem_spec = amflow::LoadProblemSpecFile(args.spec_path);
    const auto messages = LoadedSpecValidationMessages(problem_spec);
    if (!messages.empty()) {
      PrintMessages(std::cerr, messages);
      return 2;
    }
    direct_spec = ParseDirectSolveSeriesSpec(raw_spec);
  }
  if (direct_spec.family.empty()) {
    direct_spec.family = problem_spec.family.name;
  }

  amflow::SolverDiagnostics diagnostics;
  std::string status = "failed";
  std::string error;
  int exit_code = 0;

  try {
    ValidateDirectSolveSeriesSpec(direct_spec);
    if (direct_spec.amflow_state_input) {
      diagnostics = EvaluateAmflowStateEtaInfinityBoundary(direct_spec);
      const bool applied_target_reduction =
          ApplyDirectSpecTargetReductionIfPresent(direct_spec,
                                                  problem_spec.targets,
                                                  problem_spec.dimension,
                                                  args.epsilon_order,
                                                  diagnostics,
                                                  error);
      if (!error.empty()) {
        status = "failed";
        exit_code = 2;
      } else {
        const std::size_t required_result_count =
            applied_target_reduction ? problem_spec.targets.size() : direct_spec.masters.size();
        const bool has_all_target_values =
            diagnostics.target_values.size() >= required_result_count;
        const bool has_all_epsilon_coefficients =
            diagnostics.target_epsilon_coefficients.size() >= required_result_count;
        if (diagnostics.success && has_all_target_values && has_all_epsilon_coefficients) {
          status = "success";
          exit_code = 0;
        } else {
          status = "failed";
          error =
              "AMFlow eta-infinity boundary evaluation completed without enough coefficients "
              "for all requested results";
          exit_code = 4;
        }
      }
    } else {
      const bool needs_epsilon_expansion =
          args.epsilon_order > 0 || DirectSolveSeriesSpecContainsEpsilon(direct_spec);
      const std::optional<int> requested_epsilon_order =
          needs_epsilon_expansion ? std::optional<int>{args.epsilon_order} : std::nullopt;
      const amflow::SolveRequest request =
          MakeDirectSolveRequest(direct_spec,
                                 args.digits,
                                 requested_epsilon_order,
                                 problem_spec.dimension);
      const std::unique_ptr<amflow::SeriesSolver> solver = amflow::MakeBootstrapSeriesSolver();
      diagnostics = solver->Solve(request);
      if (diagnostics.success) {
        const bool applied_target_reduction =
            ApplyDirectSpecTargetReductionIfPresent(direct_spec,
                                                    problem_spec.targets,
                                                    problem_spec.dimension,
                                                    args.epsilon_order,
                                                    diagnostics,
                                                    error);
        if (!error.empty()) {
          status = "failed";
          exit_code = 2;
        } else {
          const std::size_t required_result_count =
              applied_target_reduction ? problem_spec.targets.size()
                                       : direct_spec.masters.size();
          const bool has_all_target_values =
              diagnostics.target_values.size() >= required_result_count;
          const bool has_all_epsilon_coefficients =
              !requested_epsilon_order.has_value() ||
              diagnostics.target_epsilon_coefficients.size() >= required_result_count;
          if (has_all_target_values && has_all_epsilon_coefficients) {
            status = "success";
            exit_code = 0;
          } else {
            status = "failed";
            error =
                applied_target_reduction
                    ? "series solver succeeded and target reduction ran, but reduced target "
                      "coefficients were incomplete"
                    : "series solver succeeded but did not expose transported epsilon "
                      "coefficients for all masters on this path";
            exit_code = 4;
          }
        }
      } else {
        status = "failed";
        exit_code = 4;
      }
    }
  } catch (const std::exception& solve_error) {
    status = "failed";
    error = solve_error.what();
    exit_code = 2;
  }

  const auto end = std::chrono::steady_clock::now();
  const double duration_seconds =
      std::chrono::duration<double>(end - start).count();
  WriteTextFile(args.output_path,
                SerializeSolveSeriesJson(problem_spec,
                                         direct_spec,
                                         diagnostics,
                                         args.epsilon_order,
                                         args.digits,
                                         status,
                                         error,
                                         duration_seconds));
  if (!error.empty()) {
    std::cerr << error << "\n";
  } else if (!diagnostics.success && !diagnostics.summary.empty()) {
    std::cerr << diagnostics.summary << "\n";
  }
  return exit_code;
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
            << "                           Run a reviewed embedded direct solve_series request or AMFlow state JSON\n"
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
