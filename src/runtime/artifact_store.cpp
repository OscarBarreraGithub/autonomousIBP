#include "amflow/runtime/artifact_store.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace amflow {

namespace {

std::string EscapeDoubleQuotedYaml(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped.push_back(character);
        break;
    }
  }
  return escaped;
}

std::string Quote(const std::string& value) {
  return "\"" + EscapeDoubleQuotedYaml(value) + "\"";
}

std::string SanitizeCommandName(const std::string& command_name) {
  std::string sanitized;
  sanitized.reserve(command_name.size());
  for (const char character : command_name) {
    const bool safe = (character >= 'a' && character <= 'z') ||
                      (character >= 'A' && character <= 'Z') ||
                      (character >= '0' && character <= '9') || character == '-' ||
                      character == '_' || character == '.';
    sanitized.push_back(safe ? character : '-');
  }
  if (sanitized.empty()) {
    return "command";
  }
  return sanitized;
}

std::string FormatAttemptSuffix(const std::size_t attempt_number) {
  std::ostringstream out;
  out << ".attempt-" << std::setw(4) << std::setfill('0') << attempt_number;
  return out.str();
}

std::string ToPortableString(const std::filesystem::path& path) {
  return path.empty() ? std::string() : path.lexically_normal().string();
}

void WriteQuotedLine(std::ostringstream& out,
                     const std::string& key,
                     const std::string& value,
                     const int indent = 0) {
  if (value.empty()) {
    return;
  }
  out << std::string(static_cast<std::size_t>(indent), ' ') << key << ": " << Quote(value)
      << "\n";
}

void WriteQuotedPathLine(std::ostringstream& out,
                         const std::string& key,
                         const std::filesystem::path& value,
                         const int indent = 0) {
  WriteQuotedLine(out, key, ToPortableString(value), indent);
}

void WriteIntegerLine(std::ostringstream& out, const std::string& key, const int value, const int indent = 0) {
  out << std::string(static_cast<std::size_t>(indent), ' ') << key << ": " << value << "\n";
}

void WriteSizeLine(std::ostringstream& out,
                   const std::string& key,
                   const std::size_t value,
                   const int indent = 0) {
  out << std::string(static_cast<std::size_t>(indent), ' ') << key << ": " << value << "\n";
}

std::uint64_t Fnv1a64(const std::string& text) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char character : text) {
    hash ^= static_cast<std::uint64_t>(character);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string FormatHex64(const std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

bool HasFamilyMastersFile(const std::filesystem::path& family_results_dir) {
  return std::filesystem::exists(family_results_dir / "masters");
}

bool HasFamilyRulesFile(const std::filesystem::path& family_results_dir) {
  return std::filesystem::exists(family_results_dir / "kira_target.m");
}

int FamilyResultsCompletenessScore(const std::filesystem::path& family_results_dir) {
  const bool has_masters = HasFamilyMastersFile(family_results_dir);
  const bool has_rules = HasFamilyRulesFile(family_results_dir);
  if (has_masters && has_rules) {
    return 3;
  }
  if (has_masters) {
    return 2;
  }
  if (has_rules) {
    return 1;
  }
  return 0;
}

void WriteStringMap(std::ostringstream& out,
                    const std::string& key,
                    const std::map<std::string, std::string>& values,
                    const int indent = 0) {
  if (values.empty()) {
    return;
  }
  out << std::string(static_cast<std::size_t>(indent), ' ') << key << ":\n";
  for (const auto& [entry_key, entry_value] : values) {
    out << std::string(static_cast<std::size_t>(indent + 2), ' ') << entry_key << ": "
        << Quote(entry_value) << "\n";
  }
}

}  // namespace

ArtifactLayout EnsureArtifactLayout(const std::filesystem::path& root) {
  ArtifactLayout layout;
  layout.root = root;
  layout.manifests_dir = root / "manifests";
  layout.logs_dir = root / "logs";
  layout.generated_config_dir = root / "generated-config";
  layout.results_dir = root / "results";
  layout.comparisons_dir = root / "comparisons";

  std::filesystem::create_directories(layout.manifests_dir);
  std::filesystem::create_directories(layout.logs_dir);
  std::filesystem::create_directories(layout.generated_config_dir);
  std::filesystem::create_directories(layout.results_dir);
  std::filesystem::create_directories(layout.comparisons_dir);
  return layout;
}

CommandLogPaths MakeCommandLogPaths(const ArtifactLayout& layout,
                                    const std::string& command_name) {
  const std::string stem = SanitizeCommandName(command_name);
  std::filesystem::create_directories(layout.logs_dir);

  for (std::size_t attempt_number = 1;; ++attempt_number) {
    const std::string suffix = FormatAttemptSuffix(attempt_number);
    const std::filesystem::path stdout_path =
        layout.logs_dir / (stem + suffix + ".stdout.log");
    const std::filesystem::path stderr_path =
        layout.logs_dir / (stem + suffix + ".stderr.log");
    if (!std::filesystem::exists(stdout_path) && !std::filesystem::exists(stderr_path)) {
      return {stdout_path, stderr_path, attempt_number};
    }
  }
}

std::string ComputeArtifactFingerprint(const std::string& text) {
  return "fnv1a64:" + FormatHex64(Fnv1a64(text));
}

std::filesystem::path ResolveReductionFamilyRoot(const std::filesystem::path& artifact_root,
                                                 const std::string& family) {
  const std::filesystem::path generated_config_results_dir =
      artifact_root / "generated-config" / "results" / family;
  const std::filesystem::path direct_results_dir = artifact_root / "results" / family;

  const int generated_config_score = FamilyResultsCompletenessScore(generated_config_results_dir);
  const int direct_results_score = FamilyResultsCompletenessScore(direct_results_dir);

  if (generated_config_score >= direct_results_score && generated_config_score > 0) {
    return generated_config_results_dir;
  }
  if (direct_results_score > 0) {
    return direct_results_dir;
  }
  return direct_results_dir;
}

ArtifactManifest MakeFileBackedKiraRunManifest(const FileBackedKiraRunManifestInput& input) {
  ArtifactManifest manifest;
  manifest.manifest_kind = "file-backed-kira-run";
  manifest.run_id = "bootstrap-run";
  manifest.spec_provenance = input.spec_provenance;
  manifest.spec_path = std::filesystem::absolute(input.spec_path);
  manifest.spec_fingerprint = ComputeArtifactFingerprint(input.spec_yaml);
  manifest.family = input.family;
  manifest.target_count = input.target_count;
  manifest.artifact_root = std::filesystem::absolute(input.artifact_root);
  manifest.execution_working_directory =
      input.execution_working_directory.empty()
          ? std::filesystem::path{}
          : std::filesystem::absolute(input.execution_working_directory);
  manifest.parse_artifact_root = manifest.artifact_root;
  manifest.results_family_root = ResolveReductionFamilyRoot(manifest.parse_artifact_root, input.family);
  if (!manifest.results_family_root.empty()) {
    const std::filesystem::path masters_path = manifest.results_family_root / "masters";
    if (std::filesystem::exists(masters_path)) {
      manifest.masters_path = masters_path;
    }
    const std::filesystem::path rule_path = manifest.results_family_root / "kira_target.m";
    if (std::filesystem::exists(rule_path)) {
      manifest.rule_path = rule_path;
    }
  }
  manifest.stdout_log_path = input.stdout_log_path.empty() ? std::filesystem::path{}
                                                           : std::filesystem::absolute(input.stdout_log_path);
  manifest.stderr_log_path = input.stderr_log_path.empty() ? std::filesystem::path{}
                                                           : std::filesystem::absolute(input.stderr_log_path);
  manifest.kira_executable = input.kira_executable.empty() ? std::filesystem::path{}
                                                           : std::filesystem::absolute(input.kira_executable);
  manifest.fermat_executable = input.fermat_executable.empty() ? std::filesystem::path{}
                                                               : std::filesystem::absolute(input.fermat_executable);
  manifest.rendered_command = input.rendered_command;
  manifest.execution_status = input.execution_status;
  manifest.exit_code = input.exit_code;
  manifest.repo_root = input.repo_root.empty() ? std::filesystem::path{}
                                               : std::filesystem::absolute(input.repo_root);
  manifest.amflow_commit = input.amflow_commit;
  manifest.git_status_short = input.git_status_short;
  manifest.threads = input.threads;
  manifest.effective_reduction_options = input.effective_reduction_options;
  manifest.non_default_options = input.non_default_options;
  return manifest;
}

std::string SerializeArtifactManifestYaml(const ArtifactManifest& manifest) {
  std::ostringstream out;
  WriteQuotedLine(out, "manifest_kind", manifest.manifest_kind);
  WriteQuotedLine(out, "run_id", manifest.run_id);

  out << "spec:\n";
  WriteQuotedLine(out, "provenance", manifest.spec_provenance, 2);
  WriteQuotedPathLine(out, "path", manifest.spec_path, 2);
  WriteQuotedLine(out, "fingerprint", manifest.spec_fingerprint, 2);
  WriteQuotedLine(out, "family", manifest.family, 2);
  WriteSizeLine(out, "target_count", manifest.target_count, 2);

  out << "artifacts:\n";
  WriteQuotedPathLine(out, "artifact_root", manifest.artifact_root, 2);
  WriteQuotedPathLine(out, "execution_working_directory", manifest.execution_working_directory, 2);
  WriteQuotedPathLine(out, "parse_artifact_root", manifest.parse_artifact_root, 2);
  WriteQuotedPathLine(out, "results_family_root", manifest.results_family_root, 2);
  WriteQuotedPathLine(out, "masters_path", manifest.masters_path, 2);
  WriteQuotedPathLine(out, "rule_path", manifest.rule_path, 2);
  WriteQuotedPathLine(out, "stdout_log", manifest.stdout_log_path, 2);
  WriteQuotedPathLine(out, "stderr_log", manifest.stderr_log_path, 2);

  out << "execution:\n";
  WriteQuotedPathLine(out, "kira_executable", manifest.kira_executable, 2);
  WriteQuotedPathLine(out, "fermat_executable", manifest.fermat_executable, 2);
  WriteQuotedLine(out, "command", manifest.rendered_command, 2);
  WriteQuotedLine(out, "status", manifest.execution_status, 2);
  WriteIntegerLine(out, "exit_code", manifest.exit_code, 2);
  WriteIntegerLine(out, "threads", manifest.threads, 2);

  out << "repository:\n";
  WriteQuotedPathLine(out, "root", manifest.repo_root, 2);
  WriteQuotedLine(out, "commit", manifest.amflow_commit, 2);
  WriteQuotedLine(out, "git_status_short", manifest.git_status_short, 2);

  WriteStringMap(out, "effective_reduction_options", manifest.effective_reduction_options);
  WriteStringMap(out, "non_default_options", manifest.non_default_options);
  return out.str();
}

std::filesystem::path WriteArtifactManifest(const ArtifactLayout& layout,
                                           const ArtifactManifest& manifest) {
  const std::filesystem::path path = layout.manifests_dir / (manifest.run_id + ".yaml");
  std::ofstream stream(path);
  stream << SerializeArtifactManifestYaml(manifest);
  return path;
}

}  // namespace amflow
