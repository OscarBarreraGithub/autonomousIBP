#include <filesystem>
#include <exception>
#include <iostream>
#include <string>

#include "amflow/core/options.hpp"
#include "amflow/core/problem_spec.hpp"
#include "amflow/io/problem_spec_io.hpp"
#include "amflow/io/sample_data.hpp"
#include "amflow/kira/kira_backend.hpp"
#include "amflow/runtime/artifact_store.hpp"

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
                   const std::vector<std::string>& additional_validation_messages = {}) {
  const amflow::ArtifactLayout layout = amflow::EnsureArtifactLayout(root);
  amflow::KiraBackend backend;
  amflow::BackendPreparation preparation =
      backend.Prepare(spec, MakeBootstrapReductionOptions(), layout);
  preparation.validation_messages.insert(preparation.validation_messages.end(),
                                         additional_validation_messages.begin(),
                                         additional_validation_messages.end());

  const amflow::CommandExecutionResult result =
      backend.ExecutePrepared(preparation, layout, kira_executable, fermat_executable);
  if (result.Succeeded()) {
    PrintExecutionResult(std::cout, result);
    return 0;
  }

  PrintExecutionResult(std::cerr, result);
  return result.status == amflow::CommandExecutionStatus::InvalidConfiguration ? 2 : 4;
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
            << "  show-defaults            Print bootstrap AMF and reduction defaults\n"
            << "  write-manifest <dir>     Create an artifact layout and write a bootstrap manifest\n";
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
      return RunKiraForSpec(spec, root, argv[3], argv[4], messages);
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
