#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "amflow/core/options.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

struct BackendPreparation {
  std::string backend_name;
  std::vector<std::string> validation_messages;
  std::map<std::string, std::string> generated_files;
  std::vector<std::string> command_arguments;
  std::vector<std::string> commands;
};

enum class CommandExecutionStatus {
  NotRun,
  Completed,
  FailedToStart,
  InvalidConfiguration,
  Signaled,
};

struct PreparedCommand {
  std::string label;
  std::filesystem::path executable;
  std::vector<std::string> arguments;
  std::filesystem::path working_directory;
  std::map<std::string, std::string> environment_overrides;
};

struct CommandExecutionResult {
  std::string command;
  std::filesystem::path working_directory;
  int exit_code = -1;
  std::filesystem::path stdout_log_path;
  std::filesystem::path stderr_log_path;
  std::size_t attempt_number = 0;
  std::map<std::string, std::string> environment_overrides;
  CommandExecutionStatus status = CommandExecutionStatus::NotRun;
  std::string error_message;

  bool Succeeded() const {
    return status == CommandExecutionStatus::Completed && exit_code == 0;
  }
};

class ReductionBackend {
 public:
  virtual ~ReductionBackend() = default;

  virtual std::string Name() const = 0;
  virtual std::vector<std::string> Validate(const ProblemSpec& spec,
                                            const ReductionOptions& options) const = 0;
  virtual BackendPreparation Prepare(const ProblemSpec& spec,
                                     const ReductionOptions& options,
                                     const ArtifactLayout& layout) const = 0;
};

std::vector<std::string> WritePreparationFiles(const BackendPreparation& preparation,
                                               const ArtifactLayout& layout);

}  // namespace amflow
