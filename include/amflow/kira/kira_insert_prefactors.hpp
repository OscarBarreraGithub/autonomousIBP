#pragma once

#include <string>
#include <vector>

#include "amflow/core/problem_spec.hpp"

namespace amflow {

struct KiraInsertPrefactorEntry {
  TargetIntegral integral;
  std::string denominator = "1";
};

struct KiraInsertPrefactorsSurface {
  std::vector<KiraInsertPrefactorEntry> entries;
};

std::vector<std::string> ValidateKiraInsertPrefactorsSurface(
    const KiraInsertPrefactorsSurface& surface);
std::string SerializeKiraInsertPrefactorsSurface(
    const KiraInsertPrefactorsSurface& surface);

}  // namespace amflow
