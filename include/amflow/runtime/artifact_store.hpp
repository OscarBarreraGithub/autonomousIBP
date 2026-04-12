#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>

namespace amflow {

struct ArtifactManifest {
  std::string run_id;
  std::string spec_fingerprint;
  std::string upstream_reference;
  std::string platform;
  std::string amflow_commit;
  std::string kira_version;
  std::string fermat_version;
  std::string mathematica_version;
  int threads = 1;
  std::map<std::string, std::string> non_default_options;
};

struct ArtifactLayout {
  std::filesystem::path root;
  std::filesystem::path manifests_dir;
  std::filesystem::path logs_dir;
  std::filesystem::path generated_config_dir;
  std::filesystem::path results_dir;
  std::filesystem::path comparisons_dir;
};

struct CommandLogPaths {
  std::filesystem::path stdout_path;
  std::filesystem::path stderr_path;
  std::size_t attempt_number = 0;
};

ArtifactLayout EnsureArtifactLayout(const std::filesystem::path& root);
CommandLogPaths MakeCommandLogPaths(const ArtifactLayout& layout,
                                    const std::string& command_name);
std::string SerializeArtifactManifestYaml(const ArtifactManifest& manifest);
std::filesystem::path WriteArtifactManifest(const ArtifactLayout& layout,
                                           const ArtifactManifest& manifest);

}  // namespace amflow
