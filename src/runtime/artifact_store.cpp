#include "amflow/runtime/artifact_store.hpp"

#include <iomanip>
#include <fstream>
#include <sstream>

namespace amflow {

namespace {

std::string Quote(const std::string& value) {
  return "\"" + value + "\"";
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

std::string SerializeArtifactManifestYaml(const ArtifactManifest& manifest) {
  std::ostringstream out;
  out << "run_id: " << Quote(manifest.run_id) << "\n";
  out << "spec_fingerprint: " << Quote(manifest.spec_fingerprint) << "\n";
  out << "upstream_reference: " << Quote(manifest.upstream_reference) << "\n";
  out << "platform: " << Quote(manifest.platform) << "\n";
  out << "amflow_commit: " << Quote(manifest.amflow_commit) << "\n";
  out << "kira_version: " << Quote(manifest.kira_version) << "\n";
  out << "fermat_version: " << Quote(manifest.fermat_version) << "\n";
  out << "mathematica_version: " << Quote(manifest.mathematica_version) << "\n";
  out << "threads: " << manifest.threads << "\n";
  if (!manifest.non_default_options.empty()) {
    out << "non_default_options:\n";
    for (const auto& [key, value] : manifest.non_default_options) {
      out << "  " << key << ": " << Quote(value) << "\n";
    }
  }
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
