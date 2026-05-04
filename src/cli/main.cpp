#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <exception>
#include <iostream>
#include <map>
#include <optional>
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "amflow/core/options.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/io/problem_spec_io.hpp"
#include "amflow/io/sample_data.hpp"
#include "amflow/kira/kira_backend.hpp"
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
  std::vector<amflow::BoundaryCondition> boundary_conditions;
  std::string boundary_state_kind;
  std::string boundary_state_direction;
  std::vector<std::string> boundary_epsilon_samples;
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

  if (const CliJsonValue* reduction = FindJsonField(root, "reduction")) {
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

amflow::SolverDiagnostics MakeAmflowStateDeferredBoundaryDiagnostics(
    const DirectSolveSeriesSpec& direct_spec) {
  amflow::SolverDiagnostics diagnostics;
  diagnostics.success = false;
  diagnostics.residual_norm = 1.0;
  diagnostics.overlap_mismatch = 1.0;
  diagnostics.failure_code = "boundary_unsolved";
  diagnostics.summary =
      "boundary_unsolved: solve-series parsed AMFlow eta-infinity asymptotic boundary state " +
      direct_spec.boundary_state_kind + " for " + direct_spec.variable +
      " @ infinity with " + std::to_string(direct_spec.boundary_epsilon_samples.size()) +
      " epsilon samples";
  if (!direct_spec.boundary_state_direction.empty()) {
    diagnostics.summary += "; direction=" + direct_spec.boundary_state_direction;
  }
  diagnostics.summary +=
      "; C++ asymptotic/subsystem-sample boundary evaluation and singular eta->0 complex "
      "continuation remain deferred on this runtime path";
  return diagnostics;
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
        << JsonString("deferred-asymptotic-subsystem-sample-provider") << "\n";
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
    if (status == "success" && master_it != master_index_by_label.end()) {
      const std::size_t master_index = master_it->second;
      if (master_index < diagnostics.target_epsilon_coefficients.size() &&
          !diagnostics.target_epsilon_coefficients[master_index].empty()) {
        const auto& coefficients = diagnostics.target_epsilon_coefficients[master_index];
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
      } else if (master_index < diagnostics.target_values.size()) {
        const std::string exact_real = diagnostics.target_values[master_index];
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

  amflow::SolverDiagnostics diagnostics;
  std::string status = "failed";
  std::string error;
  int exit_code = 0;

  try {
    ValidateDirectSolveSeriesSpec(direct_spec);
    if (direct_spec.amflow_state_input) {
      diagnostics = MakeAmflowStateDeferredBoundaryDiagnostics(direct_spec);
      status = "failed";
      exit_code = 4;
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
        const bool has_all_target_values =
            diagnostics.target_values.size() >= direct_spec.masters.size();
        const bool has_all_epsilon_coefficients =
            !requested_epsilon_order.has_value() ||
            diagnostics.target_epsilon_coefficients.size() >= direct_spec.masters.size();
        if (has_all_target_values && has_all_epsilon_coefficients) {
          status = "success";
          exit_code = 0;
        } else {
          status = "failed";
          error = "series solver succeeded but did not expose transported epsilon coefficients "
                  "for all masters on this path";
          exit_code = 4;
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
