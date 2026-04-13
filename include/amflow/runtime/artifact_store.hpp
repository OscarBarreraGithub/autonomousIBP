#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>

namespace amflow {

struct ArtifactManifest {
  std::string manifest_kind;
  std::string run_id;
  std::string spec_provenance;
  std::filesystem::path spec_path;
  std::string spec_fingerprint;
  std::string family;
  std::size_t target_count = 0;
  std::filesystem::path artifact_root;
  std::filesystem::path execution_working_directory;
  std::filesystem::path parse_artifact_root;
  std::filesystem::path results_family_root;
  std::filesystem::path masters_path;
  std::filesystem::path rule_path;
  std::filesystem::path stdout_log_path;
  std::filesystem::path stderr_log_path;
  std::filesystem::path kira_executable;
  std::filesystem::path fermat_executable;
  std::string rendered_command;
  std::string execution_status;
  int exit_code = -1;
  std::filesystem::path repo_root;
  std::string amflow_commit;
  std::string git_status_short;
  int threads = 1;
  std::map<std::string, std::string> effective_reduction_options;
  std::map<std::string, std::string> non_default_options;
};

struct FileBackedKiraRunManifestInput {
  std::filesystem::path spec_path;
  std::string spec_provenance;
  std::string spec_yaml;
  std::string family;
  std::size_t target_count = 0;
  std::filesystem::path artifact_root;
  std::filesystem::path execution_working_directory;
  std::filesystem::path kira_executable;
  std::filesystem::path fermat_executable;
  std::string rendered_command;
  std::string execution_status;
  int exit_code = -1;
  std::filesystem::path repo_root;
  std::string amflow_commit;
  std::string git_status_short;
  int threads = 1;
  std::map<std::string, std::string> effective_reduction_options;
  std::map<std::string, std::string> non_default_options;
  std::filesystem::path stdout_log_path;
  std::filesystem::path stderr_log_path;
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
std::string ComputeArtifactFingerprint(const std::string& text);
std::filesystem::path ResolveReductionFamilyRoot(const std::filesystem::path& artifact_root,
                                                 const std::string& family);
ArtifactManifest MakeFileBackedKiraRunManifest(const FileBackedKiraRunManifestInput& input);
std::string SerializeArtifactManifestYaml(const ArtifactManifest& manifest);
std::filesystem::path WriteArtifactManifest(const ArtifactLayout& layout,
                                           const ArtifactManifest& manifest);

}  // namespace amflow
