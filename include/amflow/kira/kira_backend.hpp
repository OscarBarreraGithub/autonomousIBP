#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"
#include "amflow/kira/reduction_backend.hpp"

namespace amflow {

struct KiraJobFiles {
  std::string integralfamilies_yaml;
  std::string kinematics_yaml;
  std::string jobs_yaml;
  std::string preferred_masters;
  std::string target_list;
  std::string xints;
};

struct ParsedMasterList {
  std::string family;
  std::filesystem::path source_path;
  std::vector<TargetIntegral> masters;
};

struct ParsedReductionTerm {
  std::string coefficient = "1";
  TargetIntegral master;
};

struct ParsedReductionRule {
  TargetIntegral target;
  std::vector<ParsedReductionTerm> terms;
};

enum class ParsedReductionStatus {
  ParsedRules,
  IdentityFallback,
};

struct ParsedReductionResult {
  ParsedMasterList master_list;
  std::filesystem::path rule_path;
  std::vector<ParsedReductionRule> rules;
  std::size_t explicit_rule_count = 0;
  ParsedReductionStatus status = ParsedReductionStatus::ParsedRules;
};

class KiraBackend final : public ReductionBackend {
 public:
  std::string Name() const override;
  std::vector<std::string> Validate(const ProblemSpec& spec,
                                    const ReductionOptions& options) const override;
  BackendPreparation Prepare(const ProblemSpec& spec,
                             const ReductionOptions& options,
                             const ArtifactLayout& layout) const override;
  BackendPreparation PrepareForTargets(const ProblemSpec& spec,
                                       const ReductionOptions& options,
                                       const ArtifactLayout& layout,
                                       const std::vector<TargetIntegral>& targets) const;

  KiraJobFiles EmitJobFiles(const ProblemSpec& spec, const ReductionOptions& options) const;
  KiraJobFiles EmitJobFilesForTargets(const ProblemSpec& spec,
                                      const ReductionOptions& options,
                                      const std::vector<TargetIntegral>& targets) const;
  PreparedCommand MakeExecutionCommand(const ArtifactLayout& layout,
                                       const std::filesystem::path& kira_executable,
                                       const std::filesystem::path& fermat_executable) const;
  CommandExecutionResult ExecutePrepared(const BackendPreparation& preparation,
                                         const ArtifactLayout& layout,
                                         const std::filesystem::path& kira_executable,
                                         const std::filesystem::path& fermat_executable) const;
  ParsedMasterList ParseMasterList(const std::filesystem::path& artifact_root,
                                   const std::string& family) const;
  ParsedReductionResult ParseReductionResult(const std::filesystem::path& artifact_root,
                                             const std::string& family) const;
};

}  // namespace amflow
