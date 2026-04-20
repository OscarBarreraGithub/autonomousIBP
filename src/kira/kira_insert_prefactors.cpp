#include "amflow/kira/kira_insert_prefactors.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace amflow {

namespace {

bool ContainsNewline(const std::string& text) {
  return text.find_first_of("\r\n") != std::string::npos;
}

}  // namespace

std::vector<std::string> ValidateKiraInsertPrefactorsSurface(
    const KiraInsertPrefactorsSurface& surface) {
  std::vector<std::string> messages;

  if (surface.entries.empty()) {
    messages.emplace_back("kira insert_prefactors entries must not be empty");
    return messages;
  }

  const std::string expected_family = surface.entries.front().integral.family;
  if (surface.entries.front().denominator != "1") {
    messages.emplace_back("kira insert_prefactors first denominator must be exact \"1\"");
  }

  for (std::size_t index = 0; index < surface.entries.size(); ++index) {
    const auto& entry = surface.entries[index];
    const std::string entry_index = std::to_string(index);

    if (entry.integral.family.empty()) {
      messages.emplace_back("kira insert_prefactors family must not be empty at entry " +
                            entry_index);
    } else if (!expected_family.empty() && entry.integral.family != expected_family) {
      messages.emplace_back("kira insert_prefactors family mismatch at entry " + entry_index +
                            ": expected \"" + expected_family + "\", found \"" +
                            entry.integral.family + "\"");
    }

    if (entry.denominator.empty()) {
      messages.emplace_back("kira insert_prefactors denominator must not be empty at entry " +
                            entry_index);
    } else if (ContainsNewline(entry.denominator)) {
      messages.emplace_back("kira insert_prefactors denominator must not contain newlines at entry " +
                            entry_index);
    }
  }

  return messages;
}

std::string SerializeKiraInsertPrefactorsSurface(
    const KiraInsertPrefactorsSurface& surface) {
  std::ostringstream out;
  for (const auto& entry : surface.entries) {
    out << entry.integral.Label() << "*1/(" << entry.denominator << ")\n";
  }
  return out.str();
}

}  // namespace amflow
