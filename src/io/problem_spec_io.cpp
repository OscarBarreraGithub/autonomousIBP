#include "amflow/io/problem_spec_io.hpp"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace amflow {

namespace {

struct ParsedLine {
  std::size_t number = 0;
  int indent = 0;
  std::string text;
};

[[noreturn]] void Fail(std::size_t line_number, const std::string& message) {
  throw std::runtime_error("problem spec parse error at line " + std::to_string(line_number) +
                           ": " + message);
}

std::string Trim(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(begin, end - begin);
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::vector<ParsedLine> Tokenize(const std::string& yaml) {
  std::vector<ParsedLine> lines;
  std::istringstream stream(yaml);
  std::string raw;
  std::size_t line_number = 0;
  while (std::getline(stream, raw)) {
    ++line_number;
    if (!raw.empty() && raw.back() == '\r') {
      raw.pop_back();
    }

    const std::size_t first_non_space = raw.find_first_not_of(' ');
    if (first_non_space == std::string::npos) {
      continue;
    }
    if (raw[first_non_space] == '#') {
      continue;
    }
    if (first_non_space % 2 != 0) {
      Fail(line_number, "indentation must use multiples of two spaces");
    }

    ParsedLine line;
    line.number = line_number;
    line.indent = static_cast<int>(first_non_space);
    line.text = raw.substr(first_non_space);
    lines.push_back(line);
  }
  return lines;
}

std::pair<std::string, std::string> SplitKeyValue(const ParsedLine& line,
                                                  const std::string& text) {
  const std::size_t separator = text.find(':');
  if (separator == std::string::npos) {
    Fail(line.number, "expected key/value pair");
  }
  return {Trim(text.substr(0, separator)), Trim(text.substr(separator + 1))};
}

std::string ParseStringValue(const ParsedLine& line, const std::string& value) {
  const std::string trimmed = Trim(value);
  if (trimmed.empty()) {
    Fail(line.number, "expected a scalar value");
  }
  if (trimmed.front() != '"') {
    return trimmed;
  }
  if (trimmed.size() < 2 || trimmed.back() != '"') {
    Fail(line.number, "unterminated quoted string");
  }

  std::string result;
  result.reserve(trimmed.size() - 2);
  for (std::size_t index = 1; index + 1 < trimmed.size(); ++index) {
    const char current = trimmed[index];
    if (current == '\\') {
      if (index + 1 >= trimmed.size() - 1) {
        Fail(line.number, "invalid escape sequence");
      }
      const char escaped = trimmed[++index];
      switch (escaped) {
        case '\\':
        case '"':
          result.push_back(escaped);
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 't':
          result.push_back('\t');
          break;
        default:
          Fail(line.number, "unsupported escape sequence");
      }
      continue;
    }
    result.push_back(current);
  }
  return result;
}

std::vector<std::string> SplitListItems(const ParsedLine& line, const std::string& value) {
  const std::string trimmed = Trim(value);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    Fail(line.number, "expected a bracketed list");
  }

  const std::string inner = Trim(trimmed.substr(1, trimmed.size() - 2));
  if (inner.empty()) {
    return {};
  }

  std::vector<std::string> items;
  std::string current;
  bool in_quotes = false;
  bool escaping = false;
  for (const char character : inner) {
    if (escaping) {
      current.push_back(character);
      escaping = false;
      continue;
    }

    if (character == '\\') {
      current.push_back(character);
      escaping = true;
      continue;
    }

    if (character == '"') {
      in_quotes = !in_quotes;
      current.push_back(character);
      continue;
    }

    if (character == ',' && !in_quotes) {
      items.push_back(Trim(current));
      current.clear();
      continue;
    }

    current.push_back(character);
  }

  if (in_quotes) {
    Fail(line.number, "unterminated quoted string in list");
  }

  items.push_back(Trim(current));
  return items;
}

std::vector<std::string> ParseStringList(const ParsedLine& line, const std::string& value) {
  std::vector<std::string> parsed;
  for (const auto& item : SplitListItems(line, value)) {
    parsed.push_back(ParseStringValue(line, item));
  }
  return parsed;
}

int ParseIntegerValue(const ParsedLine& line, const std::string& value) {
  const std::string trimmed = Trim(value);
  if (trimmed.empty()) {
    Fail(line.number, "expected an integer value");
  }

  std::size_t consumed = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(trimmed, &consumed);
  } catch (const std::exception&) {
    Fail(line.number, "invalid integer value");
  }
  if (consumed != trimmed.size()) {
    Fail(line.number, "invalid integer value");
  }
  return parsed;
}

std::vector<int> ParseIntegerList(const ParsedLine& line, const std::string& value) {
  std::vector<int> parsed;
  for (const auto& item : SplitListItems(line, value)) {
    parsed.push_back(ParseIntegerValue(line, item));
  }
  return parsed;
}

std::vector<FeynmanPrescription> ParseLoopPrescriptionList(const ParsedLine& line,
                                                           const std::string& value) {
  std::vector<FeynmanPrescription> parsed;
  for (const int item : ParseIntegerList(line, value)) {
    const std::optional<FeynmanPrescription> prescription = ParseFeynmanPrescription(item);
    if (!prescription.has_value()) {
      Fail(line.number, "unsupported loop prescription value: " + std::to_string(item));
    }
    parsed.push_back(*prescription);
  }
  return parsed;
}

bool ParseBoolValue(const ParsedLine& line, const std::string& value) {
  const std::string trimmed = Trim(value);
  if (trimmed == "true") {
    return true;
  }
  if (trimmed == "false") {
    return false;
  }
  Fail(line.number, "expected true or false");
}

PropagatorKind ParsePropagatorKindValue(const ParsedLine& line, const std::string& value) {
  const std::string parsed = ParseStringValue(line, value);
  if (parsed == "standard") {
    return PropagatorKind::Standard;
  }
  if (parsed == "cut") {
    return PropagatorKind::Cut;
  }
  if (parsed == "linear") {
    return PropagatorKind::Linear;
  }
  if (parsed == "auxiliary") {
    return PropagatorKind::Auxiliary;
  }
  Fail(line.number, "unsupported propagator kind: " + parsed);
}

PropagatorVariant ParsePropagatorVariantValue(const ParsedLine& line, const std::string& value) {
  const std::string parsed = ParseStringValue(line, value);
  const std::optional<PropagatorVariant> variant = ParsePropagatorVariantKeyword(parsed);
  if (variant.has_value()) {
    return *variant;
  }
  Fail(line.number, "unsupported propagator variant: " + parsed);
}

void RecordSeenKey(std::set<std::string>& seen,
                   const ParsedLine& line,
                   const std::string& scope,
                   const std::string& key) {
  if (!seen.insert(key).second) {
    Fail(line.number, "duplicate " + scope + " field: " + key);
  }
}

void SkipIndentedBlock(const std::vector<ParsedLine>& lines,
                       std::size_t& index,
                       int effective_indent) {
  ++index;
  while (index < lines.size() && lines[index].indent > effective_indent) {
    ++index;
  }
}

bool ApplyPropagatorField(Propagator& propagator,
                          const ParsedLine& line,
                          const std::string& key,
                          const std::string& value) {
  if (key == "expression") {
    propagator.expression = ParseStringValue(line, value);
    return true;
  }
  if (key == "mass") {
    propagator.mass = ParseStringValue(line, value);
    return true;
  }
  if (key == "kind") {
    propagator.kind = ParsePropagatorKindValue(line, value);
    return true;
  }
  if (key == "variant") {
    propagator.variant = ParsePropagatorVariantValue(line, value);
    return true;
  }
  if (key == "prescription") {
    propagator.prescription = ParseIntegerValue(line, value);
    return true;
  }
  return false;
}

bool ApplyScalarProductRuleField(ScalarProductRule& rule,
                                 const ParsedLine& line,
                                 const std::string& key,
                                 const std::string& value) {
  if (key == "left") {
    rule.left = ParseStringValue(line, value);
    return true;
  }
  if (key == "right") {
    rule.right = ParseStringValue(line, value);
    return true;
  }
  return false;
}

bool ApplyTargetField(TargetIntegral& target,
                      const ParsedLine& line,
                      const std::string& key,
                      const std::string& value) {
  if (key == "family") {
    target.family = ParseStringValue(line, value);
    return true;
  }
  if (key == "indices") {
    target.indices = ParseIntegerList(line, value);
    return true;
  }
  return false;
}

std::vector<std::string> ParseStringBlockList(const std::vector<ParsedLine>& lines,
                                              std::size_t& index,
                                              int parent_indent) {
  std::vector<std::string> values;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const ParsedLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      Fail(line.number, "expected a list item");
    }
    values.push_back(ParseStringValue(line, Trim(line.text.substr(2))));
    ++index;
  }
  return values;
}

std::vector<Propagator> ParsePropagators(const std::vector<ParsedLine>& lines,
                                         std::size_t& index,
                                         int parent_indent) {
  std::vector<Propagator> propagators;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const ParsedLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      Fail(line.number, "expected a propagator list item");
    }

    Propagator propagator;
    std::set<std::string> seen_fields;
    const int item_field_indent = parent_indent + 4;
    const auto [first_key, first_value] = SplitKeyValue(line, Trim(line.text.substr(2)));
    RecordSeenKey(seen_fields, line, "propagator", first_key);
    if (ApplyPropagatorField(propagator, line, first_key, first_value)) {
      ++index;
    } else {
      SkipIndentedBlock(lines, index, item_field_indent);
    }

    while (index < lines.size() && lines[index].indent > parent_indent + 2) {
      const ParsedLine& nested = lines[index];
      if (nested.indent != parent_indent + 4) {
        Fail(nested.number, "unexpected indentation inside propagator");
      }
      const auto [key, value] = SplitKeyValue(nested, nested.text);
      RecordSeenKey(seen_fields, nested, "propagator", key);
      if (ApplyPropagatorField(propagator, nested, key, value)) {
        ++index;
      } else {
        SkipIndentedBlock(lines, index, nested.indent);
      }
    }

    if (propagator.expression.empty()) {
      Fail(line.number, "propagator expression must not be empty");
    }
    propagators.push_back(propagator);
  }
  return propagators;
}

std::vector<ScalarProductRule> ParseScalarProductRules(const std::vector<ParsedLine>& lines,
                                                       std::size_t& index,
                                                       int parent_indent) {
  std::vector<ScalarProductRule> rules;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const ParsedLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      Fail(line.number, "expected a scalar_product_rules list item");
    }

    ScalarProductRule rule;
    std::set<std::string> seen_fields;
    const int item_field_indent = parent_indent + 4;
    const auto [first_key, first_value] = SplitKeyValue(line, Trim(line.text.substr(2)));
    RecordSeenKey(seen_fields, line, "scalar product rule", first_key);
    if (ApplyScalarProductRuleField(rule, line, first_key, first_value)) {
      ++index;
    } else {
      SkipIndentedBlock(lines, index, item_field_indent);
    }

    while (index < lines.size() && lines[index].indent > parent_indent + 2) {
      const ParsedLine& nested = lines[index];
      if (nested.indent != parent_indent + 4) {
        Fail(nested.number, "unexpected indentation inside scalar_product_rules");
      }
      const auto [key, value] = SplitKeyValue(nested, nested.text);
      RecordSeenKey(seen_fields, nested, "scalar product rule", key);
      if (ApplyScalarProductRuleField(rule, nested, key, value)) {
        ++index;
      } else {
        SkipIndentedBlock(lines, index, nested.indent);
      }
    }

    rules.push_back(rule);
  }
  return rules;
}

std::map<std::string, std::string> ParseStringMap(const std::vector<ParsedLine>& lines,
                                                  std::size_t& index,
                                                  int parent_indent) {
  std::map<std::string, std::string> values;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const ParsedLine& line = lines[index];
    if (line.indent != parent_indent + 2) {
      Fail(line.number, "unexpected indentation inside mapping");
    }
    const auto [key, value] = SplitKeyValue(line, line.text);
    const bool inserted = values.emplace(key, ParseStringValue(line, value)).second;
    if (!inserted) {
      Fail(line.number, "duplicate mapping entry: " + key);
    }
    ++index;
  }
  return values;
}

std::vector<TargetIntegral> ParseTargets(const std::vector<ParsedLine>& lines,
                                         std::size_t& index,
                                         int parent_indent) {
  std::vector<TargetIntegral> targets;
  while (index < lines.size() && lines[index].indent > parent_indent) {
    const ParsedLine& line = lines[index];
    if (line.indent != parent_indent + 2 || !StartsWith(line.text, "- ")) {
      Fail(line.number, "expected a targets list item");
    }

    TargetIntegral target;
    std::set<std::string> seen_fields;
    const int item_field_indent = parent_indent + 4;
    const auto [first_key, first_value] = SplitKeyValue(line, Trim(line.text.substr(2)));
    RecordSeenKey(seen_fields, line, "target", first_key);
    if (ApplyTargetField(target, line, first_key, first_value)) {
      ++index;
    } else {
      SkipIndentedBlock(lines, index, item_field_indent);
    }

    while (index < lines.size() && lines[index].indent > parent_indent + 2) {
      const ParsedLine& nested = lines[index];
      if (nested.indent != parent_indent + 4) {
        Fail(nested.number, "unexpected indentation inside target");
      }
      const auto [key, value] = SplitKeyValue(nested, nested.text);
      RecordSeenKey(seen_fields, nested, "target", key);
      if (ApplyTargetField(target, nested, key, value)) {
        ++index;
      } else {
        SkipIndentedBlock(lines, index, nested.indent);
      }
    }

    targets.push_back(target);
  }
  return targets;
}

FamilyDefinition ParseFamily(const std::vector<ParsedLine>& lines, std::size_t& index) {
  FamilyDefinition family;
  std::set<std::string> seen_keys;
  while (index < lines.size() && lines[index].indent > 0) {
    const ParsedLine& line = lines[index];
    if (line.indent != 2) {
      Fail(line.number, "unexpected indentation in family section");
    }

    const auto [key, value] = SplitKeyValue(line, line.text);
    RecordSeenKey(seen_keys, line, "family", key);
    if (key == "name") {
      family.name = ParseStringValue(line, value);
      ++index;
      continue;
    }
    if (key == "loop_momenta") {
      family.loop_momenta = ParseStringList(line, value);
      ++index;
      continue;
    }
    if (key == "loop_prescriptions") {
      family.loop_prescriptions = ParseLoopPrescriptionList(line, value);
      ++index;
      continue;
    }
    if (key == "top_level_sectors") {
      family.top_level_sectors = ParseIntegerList(line, value);
      ++index;
      continue;
    }
    if (key == "preferred_masters") {
      if (!value.empty()) {
        Fail(line.number, "preferred_masters must use block list syntax");
      }
      ++index;
      family.preferred_masters = ParseStringBlockList(lines, index, line.indent);
      continue;
    }
    if (key == "propagators") {
      if (!value.empty()) {
        Fail(line.number, "propagators must use block list syntax");
      }
      ++index;
      family.propagators = ParsePropagators(lines, index, line.indent);
      continue;
    }
    SkipIndentedBlock(lines, index, line.indent);
  }
  return family;
}

Kinematics ParseKinematics(const std::vector<ParsedLine>& lines, std::size_t& index) {
  Kinematics kinematics;
  std::set<std::string> seen_keys;
  while (index < lines.size() && lines[index].indent > 0) {
    const ParsedLine& line = lines[index];
    if (line.indent != 2) {
      Fail(line.number, "unexpected indentation in kinematics section");
    }

    const auto [key, value] = SplitKeyValue(line, line.text);
    RecordSeenKey(seen_keys, line, "kinematics", key);
    if (key == "incoming_momenta") {
      kinematics.incoming_momenta = ParseStringList(line, value);
      ++index;
      continue;
    }
    if (key == "outgoing_momenta") {
      kinematics.outgoing_momenta = ParseStringList(line, value);
      ++index;
      continue;
    }
    if (key == "momentum_conservation") {
      kinematics.momentum_conservation = ParseStringValue(line, value);
      ++index;
      continue;
    }
    if (key == "invariants") {
      kinematics.invariants = ParseStringList(line, value);
      ++index;
      continue;
    }
    if (key == "scalar_product_rules") {
      if (!value.empty()) {
        Fail(line.number, "scalar_product_rules must use block list syntax");
      }
      ++index;
      kinematics.scalar_product_rules =
          ParseScalarProductRules(lines, index, line.indent);
      continue;
    }
    if (key == "numeric_substitutions") {
      if (!value.empty()) {
        Fail(line.number, "numeric_substitutions must use mapping syntax");
      }
      ++index;
      kinematics.numeric_substitutions = ParseStringMap(lines, index, line.indent);
      continue;
    }
    if (key == "complex_numeric_substitutions") {
      if (!value.empty()) {
        Fail(line.number, "complex_numeric_substitutions must use mapping syntax");
      }
      ++index;
      kinematics.complex_numeric_substitutions = ParseStringMap(lines, index, line.indent);
      continue;
    }
    SkipIndentedBlock(lines, index, line.indent);
  }
  return kinematics;
}

}  // namespace

ProblemSpec ParseProblemSpecYaml(const std::string& yaml) {
  const std::vector<ParsedLine> lines = Tokenize(yaml);
  ProblemSpec spec;
  std::set<std::string> seen_top_level_keys;

  std::size_t index = 0;
  while (index < lines.size()) {
    const ParsedLine& line = lines[index];
    if (line.indent != 0) {
      Fail(line.number, "unexpected indentation at top level");
    }

    const auto [key, value] = SplitKeyValue(line, line.text);
    RecordSeenKey(seen_top_level_keys, line, "top-level", key);

    if (key == "family") {
      if (!value.empty()) {
        Fail(line.number, "family must use mapping syntax");
      }
      ++index;
      spec.family = ParseFamily(lines, index);
      continue;
    }
    if (key == "kinematics") {
      if (!value.empty()) {
        Fail(line.number, "kinematics must use mapping syntax");
      }
      ++index;
      spec.kinematics = ParseKinematics(lines, index);
      continue;
    }
    if (key == "targets") {
      if (!value.empty()) {
        Fail(line.number, "targets must use block list syntax");
      }
      ++index;
      spec.targets = ParseTargets(lines, index, line.indent);
      continue;
    }

    if (key == "dimension") {
      spec.dimension = ParseStringValue(line, value);
      ++index;
      continue;
    }
    if (key == "complex_mode") {
      spec.complex_mode = ParseBoolValue(line, value);
      ++index;
      continue;
    }
    if (key == "notes") {
      spec.notes = ParseStringValue(line, value);
      ++index;
      continue;
    }

    SkipIndentedBlock(lines, index, line.indent);
  }

  return spec;
}

ProblemSpec LoadProblemSpecFile(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("failed to open problem spec: " + path.string());
  }

  const std::string content((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
  return ParseProblemSpecYaml(content);
}

std::vector<std::string> ValidateLoadedProblemSpec(const ProblemSpec& spec) {
  std::vector<std::string> messages = ValidateProblemSpec(spec);

  for (std::size_t index = 0; index < spec.targets.size(); ++index) {
    const auto& target = spec.targets[index];
    const std::string prefix = "targets[" + std::to_string(index) + "]";
    if (target.family.empty()) {
      messages.emplace_back(prefix + ".family must not be empty");
    }
    if (target.indices.empty()) {
      messages.emplace_back(prefix + ".indices must not be empty");
      continue;
    }
    if (!spec.family.propagators.empty() &&
        target.indices.size() != spec.family.propagators.size()) {
      messages.emplace_back(prefix + ".indices size must match family.propagators size");
    }
  }

  return messages;
}

}  // namespace amflow
