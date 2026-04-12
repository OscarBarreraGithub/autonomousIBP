#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

ProblemSpec ParseProblemSpecYaml(const std::string& yaml);
ProblemSpec LoadProblemSpecFile(const std::filesystem::path& path);
std::vector<std::string> ValidateLoadedProblemSpec(const ProblemSpec& spec);

}  // namespace amflow
