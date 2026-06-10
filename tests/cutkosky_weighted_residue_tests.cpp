#include <algorithm>
#include <cstddef>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include "amflow/core/problem_spec.hpp"
#include "amflow/runtime/artifact_store.hpp"
#include "amflow/runtime/cutkosky_transport.hpp"

namespace {

using TestBigFloat = boost::multiprecision::cpp_dec_float_100;

void Expect(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void ExpectContains(const std::string& value,
                    const std::string& needle,
                    const std::string& message) {
  Expect(value.find(needle) != std::string::npos, message);
}

void ExpectEqual(const std::string& actual,
                 const std::string& expected,
                 const std::string& message) {
  Expect(actual == expected,
         message + ": expected " + expected + ", got " + actual);
}

void ExpectStringVectorEqual(const std::vector<std::string>& actual,
                             const std::vector<std::string>& expected,
                             const std::string& message) {
  Expect(actual == expected,
         message + ": expected size " + std::to_string(expected.size()) +
             ", got size " + std::to_string(actual.size()));
}

void ExpectDecimalNear(const std::string& actual,
                       const std::string& expected,
                       const std::string& message) {
  const TestBigFloat actual_value(actual);
  const TestBigFloat expected_value(expected);
  using boost::multiprecision::abs;
  Expect(abs(actual_value - expected_value) <= TestBigFloat("1e-70"),
         message + ": expected " + expected + ", got " + actual);
}

int DecimalSign(const std::string& value) {
  const TestBigFloat parsed(value.empty() ? "0" : value);
  if (parsed > 0) {
    return 1;
  }
  if (parsed < 0) {
    return -1;
  }
  return 0;
}

constexpr const char* kD246ReferenceEvidenceSidecar =
    "tools/reference-harness/specs/m6/lane146/"
    "b63n-d246-weighted-residue-reference-evidence.json";

constexpr const char* kLane146Selected4Compare30 =
    "tools/reference-harness/specs/m6/lane146/"
    "automatic_phasespace.selected4-cutkosky.compare30.json";

constexpr const char* kLane146Selected4CppResult =
    "tools/reference-harness/specs/m6/lane146/"
    "automatic_phasespace.selected4-cutkosky.cpp-result.json";

constexpr const char* kLane146Selected4Golden =
    "tools/reference-harness/specs/m6/lane146/"
    "automatic_phasespace.selected4-cutkosky.amflow-golden.txt";

constexpr int kLane146Selected4ToleranceDigits = 30;
constexpr int kLane146Selected4ExpectedDigitAgreement = 999;

std::filesystem::path LocateRepositoryRoot() {
  std::filesystem::path current = std::filesystem::current_path();
  for (int depth = 0; depth < 5; ++depth) {
    if (std::filesystem::exists(current / "tools/reference-harness") &&
        std::filesystem::exists(
            current / "tests/cutkosky_weighted_residue_tests.cpp")) {
      return current;
    }
    if (!current.has_parent_path() || current == current.parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return std::filesystem::current_path();
}

std::string ReadTextFileIfPresent(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read b63n D2/D4/D6 evidence sidecar: " +
                             path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string ReadRequiredTextFile(const std::filesystem::path& path,
                                 const std::string& label) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("missing " + label + ": " + path.string());
  }
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read " + label + ": " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool TextContains(const std::string& value, const std::string& needle) {
  return value.find(needle) != std::string::npos;
}

bool HasD246PublishedReferenceSidecarEvidence() {
  const std::filesystem::path sidecar =
      LocateRepositoryRoot() / kD246ReferenceEvidenceSidecar;
  const std::string evidence = ReadTextFileIfPresent(sidecar);
  return TextContains(evidence, "b63n") &&
         TextContains(evidence, "automatic_phasespace") &&
         TextContains(evidence, "phase[1,2,1,1,1,1,1]") &&
         TextContains(evidence, "D2") &&
         TextContains(evidence, "D4") &&
         TextContains(evidence, "D6") &&
         (TextContains(evidence, "amflow") ||
          TextContains(evidence, "AMFlow")) &&
         (TextContains(evidence, "reference") ||
          TextContains(evidence, "Reference")) &&
         TextContains(evidence, "coefficient") &&
         TextContains(evidence, "\"passed\": true") &&
         TextContains(evidence, "\"coefficient_published\": true") &&
         !TextContains(evidence, "\"skeleton\": true") &&
         !TextContains(evidence, "\"coefficients\": []") &&
         TextContains(evidence, "final_solution_samples_used_as_input") &&
         TextContains(evidence, "false");
}

bool IsSolvedScopedWeight(
    const amflow::CutkoskyScopedWeightedResidueEvaluation& evaluation) {
  return evaluation.publication_gate_passed ||
         evaluation.live_coefficients_available ||
         evaluation.reference_validation_passed ||
         evaluation.failure_code.empty();
}

struct AuditFingerprintPin {
  std::string label;
  std::string category;
  std::string audit;
  std::string expected_fingerprint;
};

void ExpectPublishedAuditCategoryFingerprintPins(
    const std::vector<AuditFingerprintPin>& pins,
    const std::vector<std::string>& published_categories) {
  Expect(pins.size() >= published_categories.size(),
         "b63n audit fingerprint coverage requires at least one pin per "
         "published category");
  std::vector<std::string> actual_fingerprints;
  actual_fingerprints.reserve(pins.size());
  for (std::size_t index = 0; index < pins.size(); ++index) {
    const AuditFingerprintPin& pin = pins[index];
    Expect(!pin.label.empty(),
           "b63n audit fingerprint pin label must not be empty");
    Expect(!pin.category.empty(),
           pin.label + " b63n audit fingerprint category must not be empty");
    Expect(std::find(published_categories.begin(),
                     published_categories.end(),
                     pin.category) != published_categories.end(),
           pin.label + " b63n audit fingerprint category is not in the "
                       "published weighted-residue category list: " +
               pin.category);
    ExpectContains(pin.audit,
                   "kind=" + pin.category + "\n",
                   pin.label + " b63n audit fingerprint pin should match the "
                               "serialized audit category");
    actual_fingerprints.push_back(amflow::ComputeArtifactFingerprint(pin.audit));
    ExpectEqual(actual_fingerprints[index],
                pin.expected_fingerprint,
                pin.label +
                    " b63n weighted-residue audit fingerprint should stay pinned");
  }
  for (const std::string& category : published_categories) {
    bool covered = false;
    for (const AuditFingerprintPin& pin : pins) {
      if (pin.category == category) {
        covered = true;
        break;
      }
    }
    Expect(covered,
           "published b63n weighted-residue audit category lacks a pinned "
           "fingerprint: " +
               category);
  }
  for (std::size_t index = 0; index < pins.size(); ++index) {
    const AuditFingerprintPin& pin = pins[index];
    for (std::size_t other = index + 1; other < pins.size(); ++other) {
      const AuditFingerprintPin& other_pin = pins[other];
      Expect(pin.expected_fingerprint != other_pin.expected_fingerprint,
             "distinct b63n audit fingerprint pins must stay unique: " +
                 pin.label + " and " + other_pin.label + " both pin " +
                 pin.expected_fingerprint);
      Expect(actual_fingerprints[index] != actual_fingerprints[other],
             "distinct b63n weighted-residue audits must not collide: " +
                 pin.label + " and " + other_pin.label + " both produced " +
                 actual_fingerprints[index]);
    }
  }
}

std::vector<std::string> PublishedB63nWeightedResidueAuditCategories() {
  return {"b63n-cutkosky-weighted-residue-evaluation-plan",
          "b63n-automatic-phasespace-weighted-residue-moment-seed",
          "b63n-automatic-phasespace-weighted-residue-moment-seed-packet",
          "b63n-weighted-residue-moment-cross-validation-gate",
          "b63n-scoped-weighted-residue-evaluation"};
}

template <typename Callable>
void ExpectExceptionContains(Callable&& callable,
                             const std::string& expected_substring,
                             const std::string& message) {
  try {
    callable();
  } catch (const std::exception& error) {
    Expect(std::string(error.what()).find(expected_substring) != std::string::npos,
           message);
    return;
  }
  throw std::runtime_error(message);
}

amflow::ProblemSpec MakeB63nAutomaticPhaseSpaceSpec() {
  amflow::ProblemSpec spec;
  spec.family.name = "phase";
  spec.family.loop_momenta = {"l1", "l2"};
  spec.family.loop_prescriptions = {amflow::FeynmanPrescription::None,
                                    amflow::FeynmanPrescription::None};
  spec.family.propagators = {
      amflow::Propagator("l1^2-msq"),
      amflow::Propagator("(l1+p1)^2"),
      amflow::Propagator("l2^2"),
      amflow::Propagator("(l1+l2+p1)^2"),
      amflow::Propagator("(l1+l2+p1+p2)^2"),
      amflow::Propagator("(l1+l2+p2)^2"),
      amflow::Propagator("(l1+p2)^2"),
  };
  for (const std::size_t index : {std::size_t{0}, std::size_t{2}, std::size_t{4}}) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Cut;
    spec.family.propagators[index].prescription =
        static_cast<int>(amflow::FeynmanPrescription::None);
  }
  spec.kinematics.invariants = {"s", "msq"};
  spec.kinematics.numeric_substitutions = {{"s", "100"}, {"msq", "1"}};
  spec.targets = {
      amflow::TargetIntegral{"phase", {1, 2, 1, 1, 1, 1, 1}}};
  return spec;
}

amflow::ProblemSpec MakeB63nFeynmanPrescriptionSpec(
    const amflow::FeynmanPrescription l1_prescription,
    const amflow::FeynmanPrescription l2_prescription) {
  amflow::ProblemSpec spec;
  spec.family.name = "loopxloop";
  spec.family.loop_momenta = {"l1", "l2", "q"};
  spec.family.loop_prescriptions = {l1_prescription,
                                    l2_prescription,
                                    amflow::FeynmanPrescription::None};
  spec.family.propagators = {
      amflow::Propagator("l1^2"),
      amflow::Propagator("(l1+p1)^2"),
      amflow::Propagator("(l1+p1+p2)^2"),
      amflow::Propagator("(l1+q)^2"),
      amflow::Propagator("l2^2"),
      amflow::Propagator("(l2-p2)^2"),
      amflow::Propagator("(l2-p2-p1)^2"),
      amflow::Propagator("(l2-q)^2"),
      amflow::Propagator("q^2-msq"),
      amflow::Propagator("(p1+p2-q)^2-m2sq"),
      amflow::Propagator("(l1-l2)^2"),
      amflow::Propagator("(p1-q)^2"),
  };
  for (const std::size_t index : {std::size_t{8}, std::size_t{9}}) {
    spec.family.propagators[index].kind = amflow::PropagatorKind::Cut;
    spec.family.propagators[index].prescription =
        static_cast<int>(amflow::FeynmanPrescription::None);
  }
  spec.kinematics.invariants = {"s", "msq", "m2sq"};
  spec.kinematics.numeric_substitutions = {
      {"s", "10"},
      {"msq", "1"},
      {"m2sq", "2/5"},
  };
  spec.targets = {
      amflow::TargetIntegral{"loopxloop",
                             {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0}}};
  return spec;
}

void ApplyB63nFeynmanExactKinematicRescaling(amflow::ProblemSpec& spec) {
  spec.kinematics.numeric_substitutions = {
      {"s", "(5*4)/2"},
      {"msq", "(9-8)"},
      {"m2sq", "(6+2)/20"},
  };
}

void FlipB63nFeynmanConjugatePrescriptions(amflow::ProblemSpec& spec) {
  Expect(spec.family.loop_prescriptions.size() >= 2,
         "b63n feynman conjugate flip requires l1/l2 prescriptions");
  const amflow::FeynmanPrescription l1 = spec.family.loop_prescriptions[0];
  spec.family.loop_prescriptions[0] = spec.family.loop_prescriptions[1];
  spec.family.loop_prescriptions[1] = l1;
}

std::string ReplaceAll(std::string value,
                       const std::string& from,
                       const std::string& to) {
  if (from.empty()) {
    return value;
  }
  std::size_t position = 0;
  while ((position = value.find(from, position)) != std::string::npos) {
    value.replace(position, from.size(), to);
    position += to.size();
  }
  return value;
}

std::string NormalizeB63nConjugatePlanAudit(std::string value) {
  value = ReplaceAll(value, "plus-minus", "conjugate-pair");
  value = ReplaceAll(value, "minus-plus", "conjugate-pair");
  value = ReplaceAll(value, "plus_i0", "conjugate_i0");
  value = ReplaceAll(value, "minus_i0", "conjugate_i0");
  return value;
}

const amflow::CutkoskyResidueSeriesTerm& ResidueTermAt(
    const amflow::CutkoskyResidueSeries& series,
    const int eps_order) {
  for (const amflow::CutkoskyResidueSeriesTerm& term : series.terms) {
    if (term.eps_order == eps_order && term.eta_power == 0 &&
        term.log_power == 0 && term.region_key == "integer") {
      return term;
    }
  }
  throw std::runtime_error("missing scoped weighted residue eps order " +
                           std::to_string(eps_order));
}

std::string BoolText(const bool value) {
  return value ? "true" : "false";
}

std::string SerializeScopedWeightedResiduePublicationPacketForDeterminism(
    const amflow::CutkoskyScopedWeightedResidueEvaluation& evaluation) {
  std::ostringstream out;
  out << amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(
      evaluation);
  out << "publication_packet_version=b63n-scoped-weighted-residue-v1\n";
  out << "series_label=" << evaluation.candidate_series.series_label << "\n";
  out << "series_expansion_variable="
      << evaluation.candidate_series.expansion_variable << "\n";
  out << "series_eta_variable=" << evaluation.candidate_series.eta_variable
      << "\n";
  out << "series_min_eps_order=" << evaluation.candidate_series.min_eps_order
      << "\n";
  out << "series_max_eps_order=" << evaluation.candidate_series.max_eps_order
      << "\n";
  out << "series_requested_precision_digits="
      << evaluation.candidate_series.requested_precision_digits << "\n";
  out << "series_working_precision_digits="
      << evaluation.candidate_series.working_precision_digits << "\n";
  out << "series_precision_diagnostics="
      << evaluation.candidate_series.precision_diagnostics << "\n";
  for (std::size_t index = 0; index < evaluation.candidate_series.terms.size();
       ++index) {
    const amflow::CutkoskyResidueSeriesTerm& term =
        evaluation.candidate_series.terms[index];
    out << "term[" << index << "].eps_order=" << term.eps_order << "\n";
    out << "term[" << index << "].eta_power=" << term.eta_power << "\n";
    out << "term[" << index << "].log_power=" << term.log_power << "\n";
    out << "term[" << index << "].region_key=" << term.region_key << "\n";
    out << "term[" << index
        << "].coefficient_label=" << term.coefficient_label << "\n";
    out << "term[" << index << "].coefficient.real="
        << term.coefficient.real << "\n";
    out << "term[" << index << "].coefficient.imaginary="
        << term.coefficient.imaginary << "\n";
    out << "term[" << index << "].precision.requested="
        << term.precision.requested_precision_digits << "\n";
    out << "term[" << index << "].precision.working="
        << term.precision.working_precision_digits << "\n";
    out << "term[" << index << "].precision.backend="
        << term.precision.arithmetic_backend << "\n";
    out << "term[" << index << "].precision.summary="
        << term.precision.summary << "\n";
    out << "term[" << index << "].provenance.source="
        << term.provenance.source << "\n";
    out << "term[" << index << "].provenance.derivation="
        << term.provenance.derivation << "\n";
    out << "term[" << index << "].provenance.fixture_id="
        << term.provenance.fixture_id << "\n";
    out << "term[" << index << "].provenance.synthetic_fixture="
        << BoolText(term.provenance.synthetic_fixture) << "\n";
    out << "term[" << index
        << "].provenance.retained_solution_samples_used="
        << BoolText(term.provenance.retained_solution_samples_used) << "\n";
    out << "term[" << index << "].provenance.coefficient_published="
        << BoolText(term.provenance.coefficient_published) << "\n";
  }
  return out.str();
}

std::string JsonKeyToken(const std::string& key) {
  return "\"" + key + "\"";
}

std::string JsonEscape(const std::string& value) {
  std::ostringstream escaped;
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (ch < 0x20) {
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(ch) << std::dec << std::setfill(' ');
        } else {
          escaped << static_cast<char>(ch);
        }
        break;
    }
  }
  return escaped.str();
}

std::size_t FindMatchingJsonDelimiter(const std::string& text,
                                      const std::size_t open_pos,
                                      const char open,
                                      const char close,
                                      const std::string& label) {
  if (open_pos >= text.size() || text[open_pos] != open) {
    throw std::runtime_error("malformed " + label + ": missing opening delimiter");
  }

  int depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t pos = open_pos; pos < text.size(); ++pos) {
    const char ch = text[pos];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        in_string = false;
      }
      continue;
    }
    if (ch == '"') {
      in_string = true;
      continue;
    }
    if (ch == open) {
      ++depth;
      continue;
    }
    if (ch == close) {
      --depth;
      if (depth == 0) {
        return pos;
      }
    }
  }
  throw std::runtime_error("malformed " + label + ": unterminated JSON delimiter");
}

std::size_t FindJsonValueStart(const std::string& object,
                               const std::string& key,
                               const std::string& label) {
  const std::size_t key_pos = object.find(JsonKeyToken(key));
  if (key_pos == std::string::npos) {
    throw std::runtime_error("missing JSON key " + key + " in " + label);
  }
  const std::size_t colon = object.find(':', key_pos + key.size() + 2);
  if (colon == std::string::npos) {
    throw std::runtime_error("missing JSON value separator for " + key +
                             " in " + label);
  }
  std::size_t pos = colon + 1;
  while (pos < object.size() &&
         std::isspace(static_cast<unsigned char>(object[pos])) != 0) {
    ++pos;
  }
  if (pos == object.size()) {
    throw std::runtime_error("empty JSON value for " + key + " in " + label);
  }
  return pos;
}

std::string ExtractJsonStringField(const std::string& object,
                                   const std::string& key,
                                   const std::string& label) {
  const std::size_t start = FindJsonValueStart(object, key, label);
  if (object[start] != '"') {
    throw std::runtime_error("JSON key " + key + " in " + label +
                             " is not a string");
  }
  std::string value;
  bool escaped = false;
  for (std::size_t pos = start + 1; pos < object.size(); ++pos) {
    const char ch = object[pos];
    if (escaped) {
      value.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }
  throw std::runtime_error("unterminated JSON string for " + key + " in " +
                           label);
}

int ExtractJsonIntField(const std::string& object,
                        const std::string& key,
                        const std::string& label) {
  std::size_t pos = FindJsonValueStart(object, key, label);
  const std::size_t begin = pos;
  if (object[pos] == '-') {
    ++pos;
  }
  while (pos < object.size() &&
         std::isdigit(static_cast<unsigned char>(object[pos])) != 0) {
    ++pos;
  }
  if (pos == begin || (pos == begin + 1 && object[begin] == '-')) {
    throw std::runtime_error("JSON key " + key + " in " + label +
                             " is not an integer");
  }
  return std::stoi(object.substr(begin, pos - begin));
}

bool ExtractJsonBoolField(const std::string& object,
                          const std::string& key,
                          const std::string& label) {
  const std::size_t pos = FindJsonValueStart(object, key, label);
  if (object.compare(pos, 4, "true") == 0) {
    return true;
  }
  if (object.compare(pos, 5, "false") == 0) {
    return false;
  }
  throw std::runtime_error("JSON key " + key + " in " + label +
                           " is not a bool");
}

std::string ExtractJsonArrayField(const std::string& object,
                                  const std::string& key,
                                  const std::string& label) {
  const std::size_t start = FindJsonValueStart(object, key, label);
  if (object[start] != '[') {
    throw std::runtime_error("JSON key " + key + " in " + label +
                             " is not an array");
  }
  const std::size_t end =
      FindMatchingJsonDelimiter(object, start, '[', ']', label + "." + key);
  return object.substr(start, end - start + 1);
}

std::vector<std::string> ExtractJsonObjectsFromArray(
    const std::string& array_text,
    const std::string& label) {
  std::vector<std::string> objects;
  for (std::size_t pos = 0; pos < array_text.size(); ++pos) {
    if (array_text[pos] != '{') {
      continue;
    }
    const std::size_t end =
        FindMatchingJsonDelimiter(array_text, pos, '{', '}', label);
    objects.push_back(array_text.substr(pos, end - pos + 1));
    pos = end;
  }
  if (objects.empty()) {
    throw std::runtime_error("JSON array " + label + " contains no objects");
  }
  return objects;
}

std::string FindJsonObjectByStringField(const std::string& array_text,
                                        const std::string& field,
                                        const std::string& expected,
                                        const std::string& label) {
  for (const std::string& object : ExtractJsonObjectsFromArray(array_text, label)) {
    if (ExtractJsonStringField(object, field, label) == expected) {
      return object;
    }
  }
  throw std::runtime_error("unable to find " + label + " object with " + field +
                           "=" + expected);
}

std::string FindJsonObjectByIntField(const std::string& array_text,
                                     const std::string& field,
                                     const int expected,
                                     const std::string& label) {
  for (const std::string& object : ExtractJsonObjectsFromArray(array_text, label)) {
    if (ExtractJsonIntField(object, field, label) == expected) {
      return object;
    }
  }
  throw std::runtime_error("unable to find " + label + " object with " + field +
                           "=" + std::to_string(expected));
}

void ScopedAutomaticPhaseSpaceWeightedResidueStopsAtPublicationGateTest() {
  const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          1,
          2,
          70);

  Expect(evaluation.reviewed_surface,
         "scoped weighted residue evaluator should bind the reviewed surface");
  Expect(evaluation.evaluation_attempted,
         "scoped weighted residue evaluator should mark the attempted evaluation");
  Expect(evaluation.publication_gate_checked,
         "scoped weighted residue evaluator should run the publication gate");
  Expect(!evaluation.publication_gate_passed,
         "synthetic weighted residue candidate must not pass publication");
  Expect(!evaluation.live_coefficients_available,
         "scoped weighted residue evaluator must not claim live coefficients");
  Expect(!evaluation.retained_solution_samples_used,
         "scoped weighted residue evaluator must not use retained final samples");
  Expect(!evaluation.full_eta_zero_contour_applied,
         "scoped weighted residue evaluator must not promote the full contour");
  Expect(evaluation.failure_code == "boundary_unsolved",
         "publication-gated synthetic weighted residue should fail as boundary_unsolved");
  Expect(evaluation.surface_label == "phase[1,2,1,1,1,1,1]",
         "scoped weighted residue evaluator should retain the weighted target");
  Expect(evaluation.selected_weight_denominator == "D2" &&
             evaluation.selected_weight_denominator_index == 1 &&
             evaluation.selected_weight_power == 2,
         "scoped weighted residue evaluator should bind the D2 squared weight");
  Expect(evaluation.moment_cross_validation_gate.passed,
         "scoped weighted residue evaluator should reuse the D2,D4,D6,D7 gate");
  Expect(evaluation.eta_zero_selection.success,
         "scoped weighted residue evaluator should reach the eta-zero selector");
  Expect(evaluation.eta_zero_selection.selected_coefficient_label ==
             "automatic_phasespace_D2_weighted_moment_seed",
         "scoped weighted residue evaluator should preserve the selected D2 label");
  Expect(evaluation.candidate_series.terms.size() == 3,
         "scoped weighted residue evaluator should carry the D2 K_2 candidate through eps^2");
  ExpectContains(ResidueTermAt(evaluation.candidate_series, 0).coefficient.real,
                 "-8.020298636472136866525611927436479798",
                 "scoped weighted residue evaluator should carry the reviewed K_2 seed");
  ExpectContains(evaluation.publication_gate_status,
                 "blocked-by-publication-gate",
                 "scoped weighted residue evaluator should preserve publication blocking");
  ExpectContains(evaluation.publication_gate_status,
                 "synthetic",
                 "scoped weighted residue evaluator should expose the synthetic blocker");

  ExpectExceptionContains(
      [&evaluation]() {
        amflow::ValidateCutkoskyResiduePublicationGate(
            evaluation.candidate_series);
      },
      "synthetic",
      "candidate series should be blocked by the existing publication validator");

  const std::string audit =
      amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(evaluation);
  ExpectContains(audit,
                 "kind=b63n-scoped-weighted-residue-evaluation",
                 "scoped weighted residue audit should identify the new evaluation");
  ExpectContains(audit,
                 "publication_gate_checked=true",
                 "scoped weighted residue audit should report publication validation");
  ExpectContains(audit,
                 "publication_gate_passed=false",
                 "scoped weighted residue audit should report the blocked gate");
  ExpectContains(audit,
                 "moment_cross_validation=passed",
                 "scoped weighted residue audit should report the existing moment gate");
  ExpectContains(audit,
                 "selected_weight_role=D2=(l1+p1)^2 angular weight with power 2",
                 "scoped weighted residue audit should expose the D2 role");
  ExpectContains(audit,
                 "selected_weight_form=inverse_denominator_weight[D2(q2,cos_theta_a)]",
                 "scoped weighted residue audit should expose the D2 structural form");
  ExpectContains(audit,
                 "candidate_provenance=D2;series=automatic_phasespace::weighted-moment-seed::D2::prefactor;eta_zero_label=automatic_phasespace_D2_weighted_moment_seed;coefficient_label=automatic_phasespace_D2_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D2-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
                 "scoped weighted residue audit should expose synthetic D2 provenance");
  ExpectContains(audit,
                 "live_coefficients_available=false",
                 "scoped weighted residue audit must not claim live coefficients");
  ExpectContains(audit,
                 "retained_solution_samples_used=false",
                 "scoped weighted residue audit must reject retained final samples");
  ExpectContains(audit,
                 "full_eta_zero_contour_applied=false",
                 "scoped weighted residue audit must not promote M6");
}

void ScopedAutomaticPhaseSpaceWeightedResidueCanSelectD7Test() {
  const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          6,
          3,
          70);

  Expect(evaluation.selected_weight_denominator == "D7" &&
             evaluation.selected_weight_denominator_index == 6 &&
             evaluation.selected_weight_power == 1,
         "scoped weighted residue evaluator should select a requested reviewed weight");
  Expect(evaluation.eta_zero_selection.selected_coefficient_label ==
             "automatic_phasespace_D7_weighted_residue_eps0",
         "scoped weighted residue evaluator should preserve the published D7 label");
  Expect(evaluation.publication_gate_checked &&
             evaluation.publication_gate_passed &&
             evaluation.failure_code.empty(),
         "D7 scoped candidate should pass the publication gate");
  Expect(evaluation.live_coefficients_available,
         "D7 scoped candidate should publish the first reviewed weighted residue");
  Expect(!evaluation.retained_solution_samples_used,
         "D7 scoped candidate must not use retained final samples as runtime input");
  Expect(!evaluation.full_eta_zero_contour_applied,
         "D7 scoped candidate must not promote the full eta-zero contour");
  Expect(evaluation.reference_validation_passed &&
             evaluation.reference_min_digit_agreement == 999,
         "D7 scoped candidate should record the AMFlow reference validation");
  Expect(evaluation.candidate_series.terms.size() == 4,
         "D7 scoped candidate should publish the reviewed eps^0..eps^3 terms");
  const amflow::CutkoskyResidueSeriesTerm& eps0 =
      ResidueTermAt(evaluation.candidate_series, 0);
  ExpectContains(eps0.coefficient.real,
                 "0.00003072064900647741498508445978252334878466335562820067",
                 "D7 scoped candidate should carry the reviewed AMFlow coefficient");
  const amflow::CutkoskyResidueSeriesTerm& eps1 =
      ResidueTermAt(evaluation.candidate_series, 1);
  Expect(eps1.coefficient_label ==
             "automatic_phasespace_D7_weighted_residue_eps1",
         "D7 scoped candidate should label the reviewed eps^1 coefficient");
  ExpectContains(eps1.coefficient.real,
                 "0.00007356405785821532462745545720829135530511062062212243",
                 "D7 scoped candidate should carry the reviewed eps^1 AMFlow coefficient");
  const amflow::CutkoskyResidueSeriesTerm& eps2 =
      ResidueTermAt(evaluation.candidate_series, 2);
  Expect(eps2.coefficient_label ==
             "automatic_phasespace_D7_weighted_residue_eps2",
         "D7 scoped candidate should label the reviewed eps^2 coefficient");
  ExpectContains(eps2.coefficient.real,
                 "0.00010902267810384027262638236274794011613970048228043",
                 "D7 scoped candidate should carry the reviewed eps^2 AMFlow coefficient");
  const amflow::CutkoskyResidueSeriesTerm& eps3 =
      ResidueTermAt(evaluation.candidate_series, 3);
  Expect(eps3.coefficient_label ==
             "automatic_phasespace_D7_weighted_residue_eps3",
         "D7 scoped candidate should label the reviewed eps^3 coefficient");
  ExpectContains(eps3.coefficient.real,
                 "0.000173930728974086295250725394127631108897561981647609",
                 "D7 scoped candidate should carry the reviewed eps^3 AMFlow coefficient");
  Expect(eps0.provenance.coefficient_published &&
             !eps0.provenance.synthetic_fixture &&
             !eps0.provenance.retained_solution_samples_used &&
             eps1.provenance.coefficient_published &&
             !eps1.provenance.synthetic_fixture &&
             !eps1.provenance.retained_solution_samples_used &&
             eps2.provenance.coefficient_published &&
             !eps2.provenance.synthetic_fixture &&
             !eps2.provenance.retained_solution_samples_used &&
             eps3.provenance.coefficient_published &&
             !eps3.provenance.synthetic_fixture &&
             !eps3.provenance.retained_solution_samples_used,
         "D7 scoped candidate should carry publishable non-synthetic provenance");
  amflow::ValidateCutkoskyResiduePublicationGate(evaluation.candidate_series);

  const std::string audit =
      amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(evaluation);
  ExpectContains(audit,
                 "publication_gate_passed=true",
                 "D7 scoped audit should report publication gate success");
  ExpectContains(audit,
                 "reference_validation_passed=true",
                 "D7 scoped audit should report reference validation");
  ExpectContains(audit,
                 "reference_min_digit_agreement=999",
                 "D7 scoped audit should report the stored AMFlow agreement");
  ExpectContains(audit,
                 "selected_weight_role=D7=(l1+p2)^2 angular weight",
                 "D7 scoped audit should expose the selected denominator role");
  ExpectContains(audit,
                 "candidate_provenance=D7;series=automatic_phasespace::reviewed-weighted-residue::D7::eps0-eps3;eta_zero_label=automatic_phasespace_D7_weighted_residue_eps0;coefficient_label=automatic_phasespace_D7_weighted_residue_eps0;source=tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.compare30.json;fixture=lane146-reviewed-automatic-phasespace-D7-weighted-residue-eps0;synthetic=false;retained_solution_samples_used=false;coefficient_published=true",
                 "D7 scoped audit should expose reviewed AMFlow provenance");
  ExpectContains(audit,
                 "full_eta_zero_contour_applied=false",
                 "D7 scoped audit must not promote M6");
}

std::vector<AuditFingerprintPin> BuildB63nWeightedResidueAuditFingerprintPins() {
  const amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  const amflow::CutkoskyWeightedResidueEvaluationPlan plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(spec);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> seed_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          spec,
          2,
          70);
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(plan,
                                                              seed_packet);
  const amflow::CutkoskyScopedWeightedResidueEvaluation blocked_d2 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          spec,
          1,
          2,
          70);
  const amflow::CutkoskyScopedWeightedResidueEvaluation published_d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          spec,
          6,
          3,
          70);

  Expect(plan.reviewed_surface && plan.coefficient_free,
         "b63n audit fingerprint regeneration requires the reviewed weighted "
         "residue plan");
  Expect(seed_packet.size() == 4,
         "b63n audit fingerprint regeneration requires the D2,D4,D6,D7 seed "
         "packet");
  Expect(gate.passed,
         "b63n audit fingerprint regeneration requires the cross-validation gate");
  Expect(!blocked_d2.publication_gate_passed &&
             blocked_d2.failure_code == "boundary_unsolved",
         "b63n D2 audit fingerprint regeneration requires the blocked D2 input");
  Expect(published_d7.publication_gate_passed &&
             published_d7.reference_validation_passed,
         "b63n D7 audit fingerprint regeneration requires the published D7 input");

  const auto d7_seed =
      std::find_if(seed_packet.begin(),
                   seed_packet.end(),
                   [](const amflow::CutkoskyWeightedResidueMomentSeed& seed) {
                     return seed.selected_weight_denominator == "D7";
                   });
  Expect(d7_seed != seed_packet.end(),
         "b63n audit fingerprint regeneration requires a D7 moment seed");

  return {{"evaluation-plan",
           "b63n-cutkosky-weighted-residue-evaluation-plan",
           amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(plan),
           "fnv1a64:a9c31cddfef4646b"},
          {"D7-moment-seed",
           "b63n-automatic-phasespace-weighted-residue-moment-seed",
           amflow::SerializeCutkoskyWeightedResidueMomentSeedAudit(*d7_seed),
           "fnv1a64:f29d369c5bd1cfd5"},
          {"moment-seed-packet",
           "b63n-automatic-phasespace-weighted-residue-moment-seed-packet",
           amflow::SerializeCutkoskyWeightedResidueMomentSeedPacketAudit(
               seed_packet),
           "fnv1a64:d5c19619e8703771"},
          {"moment-cross-validation-gate",
           "b63n-weighted-residue-moment-cross-validation-gate",
           amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
               gate),
           "fnv1a64:aa480d3ceb075144"},
          {"blocked-D2-scoped-weighted-residue",
           "b63n-scoped-weighted-residue-evaluation",
           amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(blocked_d2),
           "fnv1a64:8c19abf9c1a1f0d1"},
          {"published-D7-scoped-weighted-residue",
           "b63n-scoped-weighted-residue-evaluation",
           amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(
               published_d7),
           "fnv1a64:1c099d4fbf9dd649"}};
}

void EmitB63nWeightedResidueAuditFingerprintsJson(std::ostream& out) {
  const std::vector<AuditFingerprintPin> pins =
      BuildB63nWeightedResidueAuditFingerprintPins();
  const std::vector<std::string> published_categories =
      PublishedB63nWeightedResidueAuditCategories();

  out << "{\n";
  out << "  \"kind\": \"b63n-weighted-residue-audit-fingerprint-regeneration\",\n";
  out << "  \"entry_count\": " << pins.size() << ",\n";
  out << "  \"published_categories\": [";
  for (std::size_t index = 0; index < published_categories.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << "\"" << JsonEscape(published_categories[index]) << "\"";
  }
  out << "],\n";
  out << "  \"entries\": [\n";
  for (std::size_t index = 0; index < pins.size(); ++index) {
    const AuditFingerprintPin& pin = pins[index];
    const std::string fresh_fingerprint =
        amflow::ComputeArtifactFingerprint(pin.audit);
    out << "    {\n";
    out << "      \"label\": \"" << JsonEscape(pin.label) << "\",\n";
    out << "      \"category\": \"" << JsonEscape(pin.category) << "\",\n";
    out << "      \"fresh_fingerprint\": \"" << fresh_fingerprint << "\",\n";
    out << "      \"pinned_fingerprint\": \"" << pin.expected_fingerprint
        << "\",\n";
    out << "      \"matches_pin\": "
        << (fresh_fingerprint == pin.expected_fingerprint ? "true" : "false")
        << "\n";
    out << "    }" << (index + 1 == pins.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
}

void ScopedAutomaticPhaseSpaceWeightedResidueAuditFingerprintDriftTest() {
  const amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  const amflow::CutkoskyScopedWeightedResidueEvaluation blocked_d2 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          spec,
          1,
          2,
          70);
  Expect(!blocked_d2.publication_gate_passed &&
             blocked_d2.failure_code == "boundary_unsolved",
         "b63n D2 audit fingerprint regression requires the blocked D2 input");
  const std::string blocked_d2_fingerprint =
      amflow::ComputeCutkoskyScopedWeightedResidueEvaluationAuditFingerprint(
          blocked_d2);
  ExpectPublishedAuditCategoryFingerprintPins(
      BuildB63nWeightedResidueAuditFingerprintPins(),
      PublishedB63nWeightedResidueAuditCategories());

  amflow::CutkoskyScopedWeightedResidueEvaluation tampered_d2 = blocked_d2;
  tampered_d2.publication_gate_status += "; drift";
  Expect(amflow::ComputeCutkoskyScopedWeightedResidueEvaluationAuditFingerprint(
             tampered_d2) != blocked_d2_fingerprint,
         "b63n weighted-residue audit fingerprint should change when the "
         "canonical audit output drifts");
}

void ScopedAutomaticPhaseSpacePublishedD7OutputIsByteDeterministicTest() {
  const amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  const amflow::CutkoskyScopedWeightedResidueEvaluation first =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          spec,
          6,
          3,
          70);
  const amflow::CutkoskyScopedWeightedResidueEvaluation second =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          spec,
          6,
          3,
          70);
  Expect(first.publication_gate_passed && second.publication_gate_passed,
         "b63n D7 scoped publication determinism test requires a published input");

  const std::string first_packet =
      SerializeScopedWeightedResiduePublicationPacketForDeterminism(first);
  const std::string second_packet =
      SerializeScopedWeightedResiduePublicationPacketForDeterminism(second);
  Expect(first_packet == second_packet,
         "same b63n D7 scoped publication input must produce byte-identical "
         "publication output across runs");
  ExpectContains(first_packet,
                 "publication_packet_version=b63n-scoped-weighted-residue-v1",
                 "b63n scoped publication output should include the packet version");
  ExpectContains(first_packet,
                 "candidate_series_terms=4",
                 "b63n scoped publication output should include all reviewed D7 "
                 "eps^0..eps^3 terms");
  ExpectContains(first_packet,
                 "term[3].coefficient_label="
                 "automatic_phasespace_D7_weighted_residue_eps3",
                 "b63n scoped publication output should preserve deterministic "
                 "eps-order serialization");
  ExpectContains(first_packet,
                 "term[0].provenance.coefficient_published=true",
                 "b63n scoped publication output should preserve publishable "
                 "provenance");
}

void ScopedAutomaticPhaseSpaceWeightedResidueMomentSeedPermutationInvariantTest() {
  const amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  const amflow::CutkoskyWeightedResidueEvaluationPlan plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(spec);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> seed_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          spec,
          1,
          70);
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate baseline_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(plan, seed_packet);
  Expect(baseline_gate.passed,
         "baseline b63n weighted residue moment seed packet should pass the "
         "structural gate");
  const std::string baseline_audit =
      amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
          baseline_gate);
  ExpectContains(baseline_audit,
                 "seed_denominator_identity[0]=D2;index=1;power=2;role=D2=(l1+p1)^2 angular weight with power 2;form=inverse_denominator_weight[D2(q2,cos_theta_a)]",
                 "b63n weighted residue gate audit should expose D2 identity");
  ExpectContains(baseline_audit,
                 "seed_provenance[0]=D2;series=automatic_phasespace::weighted-moment-seed::D2::prefactor;eta_zero_label=automatic_phasespace_D2_weighted_moment_seed;coefficient_label=automatic_phasespace_D2_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D2-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
                 "b63n weighted residue gate audit should expose D2 provenance");
  ExpectContains(baseline_audit,
                 "seed_denominator_identity[3]=D7;index=6;power=1;role=D7=(l1+p2)^2 angular weight;form=inverse_denominator_weight[D7(q2,cos_theta_a)]",
                 "b63n weighted residue gate audit should expose D7 identity");
  const std::vector<std::string> expected_weight_order = {"D2", "D4", "D6", "D7"};

  std::vector<std::size_t> order = {0, 1, 2, 3};
  int permutation_count = 0;
  do {
    std::vector<amflow::CutkoskyWeightedResidueMomentSeed> permuted_packet;
    permuted_packet.reserve(order.size());
    for (const std::size_t seed_index : order) {
      permuted_packet.push_back(seed_packet[seed_index]);
    }

    const amflow::CutkoskyWeightedResidueMomentCrossValidationGate gate =
        amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(
            plan, permuted_packet);
    Expect(gate.passed,
           "b63n weighted residue moment gate should pass seed permutation " +
               std::to_string(permutation_count));
    Expect(gate.validated_weight_denominators == expected_weight_order,
           "b63n weighted residue moment gate should report canonical D2,D4,D6,D7 "
           "order for seed permutation " +
               std::to_string(permutation_count));
    Expect(!gate.live_coefficients_available &&
               !gate.retained_solution_samples_used &&
               !gate.full_eta_zero_contour_applied,
           "b63n weighted residue moment gate must keep non-publishing flags false "
           "for seed permutation " +
               std::to_string(permutation_count));
    Expect(amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
               gate) == baseline_audit,
           "b63n weighted residue moment gate audit should be permutation-invariant "
           "for seed permutation " +
               std::to_string(permutation_count));
    ++permutation_count;
  } while (std::next_permutation(order.begin(), order.end()));
  Expect(permutation_count == 24,
         "b63n weighted residue moment permutation test should cover all 24 "
         "D2,D4,D6,D7 seed orders");

  std::vector<amflow::CutkoskyWeightedResidueMomentSeed> relabeled_packet =
      seed_packet;
  relabeled_packet[1].selected_weight_denominator = "D6";
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate relabeled_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(
          plan, relabeled_packet);
  Expect(!relabeled_gate.passed,
         "b63n weighted residue moment gate should reject a relabeled duplicate "
         "rather than treating it as a permutation");
  const std::string relabeled_audit =
      amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
          relabeled_gate);
  ExpectContains(relabeled_audit,
                 "D6 seed appears more than once",
                 "b63n weighted residue moment gate should report duplicate "
                 "weights in a relabeled packet");
  ExpectContains(relabeled_audit,
                 "missing reviewed D4 seed",
                 "b63n weighted residue moment gate should report the missing "
                 "reviewed weight in a relabeled packet");
}

void ScopedAutomaticPhaseSpaceWeightedResidueKinematicRescalingInvariantTest() {
  const amflow::ProblemSpec canonical_spec = MakeB63nAutomaticPhaseSpaceSpec();
  amflow::ProblemSpec rescaled_spec = canonical_spec;
  rescaled_spec.kinematics.numeric_substitutions = {
      {"s", "(250*8)/20"},
      {"msq", "(9+5)/(2*7)"},
  };

  const amflow::CutkoskyWeightedResidueEvaluationPlan canonical_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(canonical_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan rescaled_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(rescaled_spec);
  Expect(amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
             rescaled_plan) ==
             amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
                 canonical_plan),
         "b63n weighted residue plan should be invariant under exact "
         "kinematic substitution rescaling");

  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> canonical_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          canonical_spec,
          2,
          70);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> rescaled_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          rescaled_spec,
          2,
          70);
  Expect(amflow::SerializeCutkoskyWeightedResidueMomentSeedPacketAudit(
             rescaled_packet) ==
             amflow::SerializeCutkoskyWeightedResidueMomentSeedPacketAudit(
                 canonical_packet),
         "b63n weighted residue seed packet should be invariant under exact "
         "kinematic substitution rescaling");

  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate canonical_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(canonical_plan,
                                                             canonical_packet);
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate rescaled_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(rescaled_plan,
                                                             rescaled_packet);
  Expect(canonical_gate.passed && rescaled_gate.passed,
         "b63n weighted residue gate should pass both canonical and rescaled "
         "kinematic substitutions");
  Expect(amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
             rescaled_gate) ==
             amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
                 canonical_gate),
         "b63n weighted residue gate audit should be invariant under exact "
         "kinematic substitution rescaling");

  const amflow::CutkoskyScopedWeightedResidueEvaluation canonical_d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(canonical_spec,
                                                               6,
                                                               3,
                                                               70);
  const amflow::CutkoskyScopedWeightedResidueEvaluation rescaled_d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(rescaled_spec,
                                                               6,
                                                               3,
                                                               70);
  Expect(canonical_d7.publication_gate_passed &&
             rescaled_d7.publication_gate_passed,
         "rescaled b63n D7 scoped residue should retain the reviewed publication "
         "state");
  for (const int eps_order : {0, 1, 2, 3}) {
    const amflow::CutkoskyResidueSeriesTerm& canonical_term =
        ResidueTermAt(canonical_d7.candidate_series, eps_order);
    const amflow::CutkoskyResidueSeriesTerm& rescaled_term =
        ResidueTermAt(rescaled_d7.candidate_series, eps_order);
    ExpectEqual(rescaled_term.coefficient.real,
                canonical_term.coefficient.real,
                "rescaled b63n D7 eps^" + std::to_string(eps_order) +
                    " real coefficient should match canonical substitution");
    ExpectEqual(rescaled_term.coefficient.imaginary,
                canonical_term.coefficient.imaginary,
                "rescaled b63n D7 eps^" + std::to_string(eps_order) +
                    " imaginary coefficient should match canonical substitution");
  }
}

void ScopedFeynmanPrescriptionWeightedResiduePlanRescalingConjugateCompositionTest() {
  const amflow::ProblemSpec plus_minus_spec =
      MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::PlusI0,
                                      amflow::FeynmanPrescription::MinusI0);
  amflow::ProblemSpec rescaled_plus_minus_spec = plus_minus_spec;
  ApplyB63nFeynmanExactKinematicRescaling(rescaled_plus_minus_spec);

  amflow::ProblemSpec flipped_then_rescaled_spec = plus_minus_spec;
  FlipB63nFeynmanConjugatePrescriptions(flipped_then_rescaled_spec);
  ApplyB63nFeynmanExactKinematicRescaling(flipped_then_rescaled_spec);

  amflow::ProblemSpec rescaled_then_flipped_spec = plus_minus_spec;
  ApplyB63nFeynmanExactKinematicRescaling(rescaled_then_flipped_spec);
  FlipB63nFeynmanConjugatePrescriptions(rescaled_then_flipped_spec);

  const amflow::CutkoskyWeightedResidueEvaluationPlan plus_minus_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(plus_minus_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan rescaled_plus_minus_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(
          rescaled_plus_minus_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan flipped_then_rescaled_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(
          flipped_then_rescaled_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan rescaled_then_flipped_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(
          rescaled_then_flipped_spec);

  const std::string plus_minus_audit =
      amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
          plus_minus_plan);
  const std::string rescaled_plus_minus_audit =
      amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
          rescaled_plus_minus_plan);
  const std::string flipped_then_rescaled_audit =
      amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
          flipped_then_rescaled_plan);
  const std::string rescaled_then_flipped_audit =
      amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
          rescaled_then_flipped_plan);

  Expect(rescaled_plus_minus_audit == plus_minus_audit,
         "b63n feynman_prescription weighted residue plan should be invariant "
         "under exact kinematic rescaling");
  Expect(flipped_then_rescaled_audit == rescaled_then_flipped_audit,
         "b63n feynman_prescription weighted residue plan should commute exact "
         "kinematic rescaling with the conjugate-prescription flip");
  Expect(NormalizeB63nConjugatePlanAudit(flipped_then_rescaled_audit) ==
             NormalizeB63nConjugatePlanAudit(plus_minus_audit),
         "b63n feynman_prescription weighted residue plan should change only "
         "conjugate sign-bearing audit tokens after rescaling and flipping");

  Expect(plus_minus_plan.requires_feynman_conjugate_validation &&
             flipped_then_rescaled_plan.requires_feynman_conjugate_validation,
         "b63n feynman_prescription weighted residue plan should retain the "
         "conjugate validation requirement");
  Expect(plus_minus_plan.coefficient_free &&
             flipped_then_rescaled_plan.coefficient_free &&
             !plus_minus_plan.live_coefficients_available &&
             !flipped_then_rescaled_plan.live_coefficients_available &&
             !plus_minus_plan.retained_solution_samples_used &&
             !flipped_then_rescaled_plan.retained_solution_samples_used &&
             !plus_minus_plan.full_eta_zero_contour_applied &&
             !flipped_then_rescaled_plan.full_eta_zero_contour_applied,
         "b63n feynman_prescription weighted residue composition must remain "
         "coefficient-free and non-publishing");
  Expect(plus_minus_plan.residue_model_kind ==
             "feynman_prescription::two-body-residue::plus-minus" &&
             flipped_then_rescaled_plan.residue_model_kind ==
                 "feynman_prescription::two-body-residue::minus-plus",
         "b63n feynman_prescription weighted residue composition should swap "
         "the conjugate residue model");
  Expect(plus_minus_plan.conjugate_residue_model_kind ==
             flipped_then_rescaled_plan.residue_model_kind &&
             flipped_then_rescaled_plan.conjugate_residue_model_kind ==
                 plus_minus_plan.residue_model_kind,
         "b63n feynman_prescription weighted residue composition should keep "
         "the conjugate partner pointers symmetric");
  Expect(plus_minus_plan.cut_denominator_indices ==
             flipped_then_rescaled_plan.cut_denominator_indices &&
             plus_minus_plan.uncut_denominator_indices ==
                 flipped_then_rescaled_plan.uncut_denominator_indices &&
             plus_minus_plan.residue_variables ==
                 flipped_then_rescaled_plan.residue_variables,
         "b63n feynman_prescription weighted residue composition should preserve "
         "cut support, moment weights, and residue variables");
  ExpectContains(flipped_then_rescaled_audit,
                 "conjugate_partner=feynman_prescription::two-body-residue::plus-minus",
                 "b63n feynman_prescription flipped/rescaled audit should name "
                 "the plus/minus conjugate partner");
  ExpectContains(flipped_then_rescaled_audit,
                 "required_validation=feynman-conjugate",
                 "b63n feynman_prescription flipped/rescaled audit should retain "
                 "the conjugate validation marker");
}

void ScopedAutomaticD7SeedSignComposesWithFeynmanConjugateFlipTest() {
  const amflow::ProblemSpec automatic_spec = MakeB63nAutomaticPhaseSpaceSpec();
  const amflow::CutkoskyWeightedResidueEvaluationPlan automatic_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(automatic_spec);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> seed_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          automatic_spec,
          3,
          70);
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate automatic_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(automatic_plan,
                                                             seed_packet);
  Expect(automatic_gate.passed,
         "b63n D7/conjugate composition requires the reviewed automatic seed gate");

  const auto d7_seed =
      std::find_if(seed_packet.begin(),
                   seed_packet.end(),
                   [](const amflow::CutkoskyWeightedResidueMomentSeed& seed) {
                     return seed.selected_weight_denominator == "D7";
                   });
  Expect(d7_seed != seed_packet.end() &&
             d7_seed->selected_weight_denominator_index == 6 &&
             d7_seed->selected_weight_power == 1,
         "b63n D7/conjugate composition should bind the reviewed D7 seed");

  const amflow::CutkoskyScopedWeightedResidueEvaluation published_d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          automatic_spec,
          6,
          3,
          70);
  Expect(published_d7.publication_gate_passed &&
             published_d7.reference_validation_passed &&
             published_d7.live_coefficients_available &&
             !published_d7.full_eta_zero_contour_applied,
         "b63n D7/conjugate composition requires the scoped D7 publication only");

  for (const int eps_order : {0, 1, 2, 3}) {
    const amflow::CutkoskyResidueSeriesTerm& seed_term =
        ResidueTermAt(d7_seed->residue_series, eps_order);
    const amflow::CutkoskyResidueSeriesTerm& published_term =
        ResidueTermAt(published_d7.candidate_series, eps_order);
    Expect(DecimalSign(seed_term.coefficient.real) < 0 &&
               seed_term.coefficient.imaginary == "0",
           "b63n D7 seed eps^" + std::to_string(eps_order) +
               " should keep the negative K_2 seed sign before composition");
    Expect(DecimalSign(published_term.coefficient.real) > 0 &&
               published_term.coefficient.imaginary == "0",
           "b63n D7 published eps^" + std::to_string(eps_order) +
               " should keep the reviewed positive sign before composition");
    Expect(seed_term.provenance.synthetic_fixture &&
               !seed_term.provenance.coefficient_published &&
               !seed_term.provenance.retained_solution_samples_used &&
               published_term.provenance.coefficient_published &&
               !published_term.provenance.synthetic_fixture &&
               !published_term.provenance.retained_solution_samples_used,
           "b63n D7 seed and publication provenance should remain separated "
           "before composition");
  }

  const std::string seed_packet_audit =
      amflow::SerializeCutkoskyWeightedResidueMomentSeedPacketAudit(seed_packet);
  const std::string published_d7_audit =
      amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(published_d7);
  Expect(NormalizeB63nConjugatePlanAudit(seed_packet_audit) ==
             seed_packet_audit,
         "b63n feynman conjugate normalizer must leave automatic D7 seed signs "
         "unchanged");
  Expect(NormalizeB63nConjugatePlanAudit(published_d7_audit) ==
             published_d7_audit,
         "b63n feynman conjugate normalizer must leave scoped D7 publication "
         "signs unchanged");
  ExpectContains(
      seed_packet_audit,
      "D7;series=automatic_phasespace::weighted-moment-seed::D7::prefactor",
      "b63n D7/conjugate composition should keep the D7 seed provenance visible");
  ExpectContains(published_d7_audit,
                 "selected_weight=D7",
                 "b63n D7/conjugate composition should keep the D7 scoped "
                 "publication visible");

  const amflow::ProblemSpec plus_minus_spec =
      MakeB63nFeynmanPrescriptionSpec(amflow::FeynmanPrescription::PlusI0,
                                      amflow::FeynmanPrescription::MinusI0);
  amflow::ProblemSpec minus_plus_spec = plus_minus_spec;
  FlipB63nFeynmanConjugatePrescriptions(minus_plus_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan plus_minus_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(plus_minus_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan minus_plus_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(minus_plus_spec);
  Expect(plus_minus_plan.requires_feynman_conjugate_validation &&
             minus_plus_plan.requires_feynman_conjugate_validation &&
             plus_minus_plan.coefficient_free &&
             minus_plus_plan.coefficient_free &&
             !plus_minus_plan.live_coefficients_available &&
             !minus_plus_plan.live_coefficients_available &&
             !plus_minus_plan.full_eta_zero_contour_applied &&
             !minus_plus_plan.full_eta_zero_contour_applied,
         "b63n feynman conjugate composition should remain a non-publishing "
         "plan guard");
  Expect(plus_minus_plan.conjugate_residue_model_kind ==
             minus_plus_plan.residue_model_kind &&
             minus_plus_plan.conjugate_residue_model_kind ==
                 plus_minus_plan.residue_model_kind,
         "b63n feynman conjugate composition should keep symmetric conjugate "
         "partners");

  const std::string plus_composite_audit =
      seed_packet_audit + published_d7_audit +
      amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
          plus_minus_plan);
  const std::string minus_composite_audit =
      seed_packet_audit + published_d7_audit +
      amflow::SerializeCutkoskyWeightedResidueEvaluationPlanAudit(
          minus_plus_plan);
  Expect(plus_composite_audit != minus_composite_audit,
         "b63n D7/conjugate composition should expose the sign-bearing feynman "
         "ledger before normalization");
  Expect(NormalizeB63nConjugatePlanAudit(plus_composite_audit) ==
             NormalizeB63nConjugatePlanAudit(minus_composite_audit),
         "b63n D7 seed sign audit should compose with the feynman conjugate flip "
         "after only conjugate sign tokens are normalized");
}

void ScopedAutomaticPhaseSpaceWeightedResidueProvenanceDiagnosticsTransformInvariantTest() {
  const amflow::ProblemSpec canonical_spec = MakeB63nAutomaticPhaseSpaceSpec();
  amflow::ProblemSpec transformed_spec = canonical_spec;
  transformed_spec.kinematics.numeric_substitutions = {
      {"s", "(25*16)/4"},
      {"msq", "(3*11-5)/(7*4)"},
  };

  const amflow::CutkoskyWeightedResidueEvaluationPlan canonical_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(canonical_spec);
  const amflow::CutkoskyWeightedResidueEvaluationPlan transformed_plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(transformed_spec);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> canonical_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          canonical_spec,
          2,
          70);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> transformed_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          transformed_spec,
          2,
          70);

  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate canonical_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(canonical_plan,
                                                             canonical_packet);
  std::vector<amflow::CutkoskyWeightedResidueMomentSeed> permuted_transformed_packet;
  for (const std::size_t index : {std::size_t{3}, std::size_t{0}, std::size_t{2},
                                  std::size_t{1}}) {
    permuted_transformed_packet.push_back(transformed_packet[index]);
  }
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate transformed_gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(
          transformed_plan,
          permuted_transformed_packet);

  Expect(canonical_gate.passed && transformed_gate.passed,
         "combined b63n permutation/rescaling provenance diagnostic transform "
         "should pass both gates");
  ExpectStringVectorEqual(transformed_gate.validated_weight_denominators,
                          canonical_gate.validated_weight_denominators,
                          "b63n validated weight order should stay canonical under "
                          "combined permutation/rescaling transforms");
  ExpectStringVectorEqual(
      transformed_gate.validated_seed_denominator_identities,
      canonical_gate.validated_seed_denominator_identities,
      "b63n seed denominator identities should stay canonical under combined "
      "permutation/rescaling transforms");
  ExpectStringVectorEqual(transformed_gate.validated_seed_provenance,
                          canonical_gate.validated_seed_provenance,
                          "b63n seed provenance diagnostics should stay canonical "
                          "under combined permutation/rescaling transforms");
  Expect(canonical_gate.validated_seed_provenance.size() == 4,
         "b63n provenance diagnostic regression should cover D2,D4,D6,D7");
  ExpectContains(
      canonical_gate.validated_seed_provenance[1],
      "D4;series=automatic_phasespace::weighted-moment-seed::D4::prefactor;"
      "eta_zero_label=automatic_phasespace_D4_weighted_moment_seed;"
      "coefficient_label=automatic_phasespace_D4_weighted_moment_seed;"
      "source=reviewed automatic_phasespace symbolic integrand; not AMFlow final "
      "solution samples;fixture=lane3-next2-automatic-phasespace-D4-weighted-"
      "moment-seed;synthetic=true;retained_solution_samples_used=false;"
      "coefficient_published=false",
      "b63n provenance diagnostics should retain the D4 synthetic fixture boundary");
  Expect(amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
             transformed_gate) ==
             amflow::SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit(
                 canonical_gate),
         "b63n cross-validation audit should stay stable when provenance "
         "diagnostics are canonicalized after permutation and exact rescaling");
}

void ScopedAutomaticPhaseSpaceSeedIdentitySignBoundaryTest() {
  struct ExpectedSeedBoundary {
    std::size_t denominator_index;
    std::string denominator_id;
    int power;
    std::string identity_fragment;
  };
  const std::vector<ExpectedSeedBoundary> expected_boundaries = {
      {1,
       "D2",
       2,
       "D2;index=1;power=2;role=D2=(l1+p1)^2 angular weight with power 2;"
       "form=inverse_denominator_weight[D2(q2,cos_theta_a)]"},
      {3,
       "D4",
       1,
       "D4;index=3;power=1;role=D4=(l1+l2+p1)^2 angular weight;"
       "form=inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)]"},
      {5,
       "D6",
       1,
       "D6;index=5;power=1;role=D6=(l1+l2+p2)^2 angular weight;"
       "form=inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)]"},
      {6,
       "D7",
       1,
       "D7;index=6;power=1;role=D7=(l1+p2)^2 angular weight;"
       "form=inverse_denominator_weight[D7(q2,cos_theta_a)]"},
  };

  const amflow::ProblemSpec spec = MakeB63nAutomaticPhaseSpaceSpec();
  const amflow::CutkoskyWeightedResidueEvaluationPlan plan =
      amflow::BuildCutkoskyWeightedResidueEvaluationPlan(spec);
  const std::vector<amflow::CutkoskyWeightedResidueMomentSeed> seed_packet =
      amflow::BuildAutomaticPhaseSpaceWeightedResidueMomentSeeds(
          spec,
          3,
          70);
  const amflow::CutkoskyWeightedResidueMomentCrossValidationGate gate =
      amflow::CrossValidateCutkoskyWeightedResidueMomentSeeds(plan, seed_packet);
  Expect(gate.passed,
         "b63n seed identity/sign boundary test requires a valid seed packet");
  Expect(gate.validated_seed_denominator_identities.size() ==
             expected_boundaries.size(),
         "b63n seed identity/sign boundary test should cover D2,D4,D6,D7");

  const amflow::CutkoskyScopedWeightedResidueEvaluation published_d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          spec,
          6,
          3,
          70);
  Expect(published_d7.publication_gate_passed &&
             published_d7.live_coefficients_available,
         "b63n seed identity/sign boundary test requires the reviewed D7 "
         "publication surface");
  std::vector<int> published_d7_signs;
  for (const int eps_order : {0, 1, 2, 3}) {
    const amflow::CutkoskyResidueSeriesTerm& d7_term =
        ResidueTermAt(published_d7.candidate_series, eps_order);
    const int sign = DecimalSign(d7_term.coefficient.real);
    Expect(sign > 0,
           "reviewed published D7 eps^" + std::to_string(eps_order) +
               " coefficient should stay positive");
    Expect(d7_term.provenance.coefficient_published &&
               !d7_term.provenance.synthetic_fixture,
           "reviewed published D7 eps^" + std::to_string(eps_order) +
               " coefficient should retain publishable provenance");
    published_d7_signs.push_back(sign);
  }

  for (const ExpectedSeedBoundary& expected : expected_boundaries) {
    const amflow::CutkoskyWeightedResidueMomentSeed* seed = nullptr;
    for (const amflow::CutkoskyWeightedResidueMomentSeed& candidate :
         seed_packet) {
      if (candidate.selected_weight_denominator == expected.denominator_id) {
        seed = &candidate;
        break;
      }
    }
    Expect(seed != nullptr,
           "b63n seed packet should include " + expected.denominator_id);
    Expect(seed->selected_weight_denominator_index ==
               expected.denominator_index &&
               seed->selected_weight_power == expected.power,
           "b63n seed identity should bind the reviewed " +
               expected.denominator_id + " denominator index and power");
    const std::string seed_audit =
        amflow::SerializeCutkoskyWeightedResidueMomentSeedAudit(*seed);
    ExpectContains(seed_audit,
                   "selected_weight_identity=" + expected.identity_fragment,
                   "b63n seed audit should pin the " + expected.denominator_id +
                       " denominator identity");
    ExpectContains(seed_audit,
                   "publication_gate=blocked-by-publication-gate",
                   "b63n " + expected.denominator_id +
                       " seed should stay blocked before publication");

    for (const int eps_order : {0, 1, 2, 3}) {
      const amflow::CutkoskyResidueSeriesTerm& seed_term =
          ResidueTermAt(seed->residue_series, eps_order);
      const int seed_sign = DecimalSign(seed_term.coefficient.real);
      Expect(seed_sign < 0,
             expected.denominator_id + " synthetic seed eps^" +
                 std::to_string(eps_order) +
                 " sign should follow the negative K_2 normalization");
      Expect(seed_sign != published_d7_signs[static_cast<std::size_t>(eps_order)],
             expected.denominator_id + " synthetic seed eps^" +
                 std::to_string(eps_order) +
                 " must not borrow the reviewed D7 published sign");
      Expect(seed_term.provenance.synthetic_fixture &&
                 !seed_term.provenance.coefficient_published &&
                 !seed_term.provenance.retained_solution_samples_used,
             expected.denominator_id + " synthetic seed eps^" +
                 std::to_string(eps_order) +
                 " should remain unpublished seed provenance");
    }
  }
}

void ScopedAutomaticPhaseSpacePublishedD7MatchesLane146AMFlowCompare30Test() {
  const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          6,
          3,
          70);
  Expect(evaluation.publication_gate_passed &&
             evaluation.live_coefficients_available,
         "D7 scoped runtime surface must be published before AMFlow parity");
  ExpectEqual(evaluation.reference_validation_source,
              kLane146Selected4Compare30,
              "D7 scoped runtime surface should name the retained AMFlow compare30");
  Expect(evaluation.reference_min_digit_agreement >= 30,
         "D7 scoped runtime surface should retain compare30 digit agreement");

  const std::string compare_json = ReadRequiredTextFile(
      LocateRepositoryRoot() / kLane146Selected4Compare30,
      "lane146 b63n selected4 compare30");
  const std::string integrals =
      ExtractJsonArrayField(compare_json, "integrals", "lane146 compare30");
  const std::string d7_integral = FindJsonObjectByStringField(
      integrals,
      "integral",
      "phase[1,1,1,0,1,0,1]",
      "lane146 compare30 integrals");
  const std::string coefficients = ExtractJsonArrayField(
      d7_integral,
      "coefficients",
      "lane146 compare30 D7 integral");

  for (const int eps_order : {0, 1, 2, 3}) {
    const std::string coefficient = FindJsonObjectByIntField(
        coefficients,
        "order",
        eps_order,
        "lane146 compare30 D7 coefficients");
    Expect(ExtractJsonBoolField(coefficient,
                                "amflow_present",
                                "lane146 compare30 D7 coefficient"),
           "D7 eps^" + std::to_string(eps_order) +
               " must retain AMFlow presence");
    Expect(ExtractJsonBoolField(coefficient,
                                "cpp_present",
                                "lane146 compare30 D7 coefficient"),
           "D7 eps^" + std::to_string(eps_order) +
               " must retain C++ presence");
    Expect(ExtractJsonBoolField(coefficient,
                                "passed",
                                "lane146 compare30 D7 coefficient"),
           "D7 eps^" + std::to_string(eps_order) +
               " must pass the retained AMFlow comparison");
    Expect(ExtractJsonIntField(coefficient,
                               "real_agreement_digits",
                               "lane146 compare30 D7 coefficient") >= 30,
           "D7 eps^" + std::to_string(eps_order) +
               " must retain at least 30 real agreement digits");
    Expect(ExtractJsonIntField(coefficient,
                               "imag_agreement_digits",
                               "lane146 compare30 D7 coefficient") >= 30,
           "D7 eps^" + std::to_string(eps_order) +
               " must retain at least 30 imaginary agreement digits");

    const amflow::CutkoskyResidueSeriesTerm& term =
        ResidueTermAt(evaluation.candidate_series, eps_order);
    ExpectEqual(term.coefficient.real,
                ExtractJsonStringField(coefficient,
                                       "amflow_real",
                                       "lane146 compare30 D7 coefficient"),
                "published D7 eps^" + std::to_string(eps_order) +
                    " real coefficient must match AMFlow compare30");
    ExpectEqual(term.coefficient.imaginary,
                ExtractJsonStringField(coefficient,
                                       "amflow_imag",
                                       "lane146 compare30 D7 coefficient"),
                "published D7 eps^" + std::to_string(eps_order) +
                    " imaginary coefficient must match AMFlow compare30");
    ExpectEqual(term.provenance.source,
                kLane146Selected4Compare30,
                "published D7 eps^" + std::to_string(eps_order) +
                    " provenance must bind the compare30 fixture");
  }
}

void ScopedAutomaticPhaseSpaceSelected4PinsAllLane146ComparedCoefficientsTest() {
  struct ExpectedIntegral {
    std::string integral;
    std::vector<int> eps_orders;
  };
  const std::vector<ExpectedIntegral> expected_integrals = {
      {"phase[1,-1,1,0,1,0,0]", {0, 1, 2, 3}},
      {"phase[1,0,1,0,1,0,0]", {0, 1, 2, 3}},
      {"phase[1,1,1,0,1,0,1]", {0, 1, 2, 3}},
      {"phase[1,1,1,1,1,1,1]", {-3, -2, -1, 0, 1, 2, 3}},
  };

  const std::string compare_json = ReadRequiredTextFile(
      LocateRepositoryRoot() / kLane146Selected4Compare30,
      "lane146 b63n selected4 compare30");
  ExpectEqual(ExtractJsonStringField(compare_json,
                                     "benchmark_id",
                                     "lane146 compare30"),
              "automatic_phasespace",
              "lane146 compare30 should remain scoped to automatic_phasespace");
  ExpectEqual(ExtractJsonStringField(compare_json,
                                     "comparison",
                                     "lane146 compare30"),
              "cpp-vs-amflow",
              "lane146 compare30 should remain a C++ vs AMFlow comparison");
  ExpectEqual(ExtractJsonStringField(compare_json,
                                     "amflow_golden",
                                     "lane146 compare30"),
              kLane146Selected4Golden,
              "lane146 compare30 should bind the selected4 AMFlow golden");
  ExpectEqual(ExtractJsonStringField(compare_json,
                                     "cpp_result",
                                     "lane146 compare30"),
              kLane146Selected4CppResult,
              "lane146 compare30 should bind the retained C++ runtime result");
  Expect(ExtractJsonIntField(compare_json,
                             "compared_coefficient_count",
                             "lane146 compare30") == 19,
         "lane146 compare30 should retain exactly 19 compared coefficients");
  Expect(ExtractJsonIntField(compare_json,
                             "passed_coefficient_count",
                             "lane146 compare30") == 19,
         "lane146 compare30 should retain 19 passing coefficients");
  Expect(ExtractJsonBoolField(compare_json, "passed", "lane146 compare30"),
         "lane146 compare30 should retain top-level pass status");
  Expect(ExtractJsonIntField(compare_json,
                             "matched_integral_count",
                             "lane146 compare30") ==
             static_cast<int>(expected_integrals.size()),
         "lane146 compare30 should retain all selected4 matched integrals");
  Expect(ExtractJsonIntField(compare_json,
                             "tolerance_digits",
                             "lane146 compare30") ==
             kLane146Selected4ToleranceDigits,
         "lane146 compare30 should retain the 30-digit tolerance contract");
  Expect(ExtractJsonIntField(compare_json,
                             "minimum_digit_agreement",
                             "lane146 compare30") ==
             kLane146Selected4ExpectedDigitAgreement,
         "lane146 compare30 should retain the exact selected4 digit agreement");
  const std::string failures =
      ExtractJsonArrayField(compare_json, "failures", "lane146 compare30");
  Expect(failures.find('{') == std::string::npos,
         "lane146 compare30 should not retain comparison failures");

  const std::string cpp_result_json = ReadRequiredTextFile(
      LocateRepositoryRoot() / kLane146Selected4CppResult,
      "lane146 b63n selected4 C++ runtime result");
  const std::string compare_integrals =
      ExtractJsonArrayField(compare_json, "integrals", "lane146 compare30");
  const std::string cpp_results =
      ExtractJsonArrayField(cpp_result_json, "results", "lane146 C++ result");

  std::size_t compared_count = 0;
  std::size_t real_digit_agreement_checks = 0;
  std::size_t imaginary_digit_agreement_checks = 0;
  bool observed_digit_agreement = false;
  int observed_min_digit_agreement = 0;
  for (const ExpectedIntegral& expected : expected_integrals) {
    const std::string compare_integral = FindJsonObjectByStringField(
        compare_integrals,
        "integral",
        expected.integral,
        "lane146 compare30 integrals");
    const std::string cpp_integral = FindJsonObjectByStringField(
        cpp_results,
        "integral",
        expected.integral,
        "lane146 C++ result integrals");
    ExpectEqual(ExtractJsonStringField(compare_integral,
                                       "status",
                                       "lane146 compare30 integral"),
                "compared",
                expected.integral + " should retain compared status");

    const std::string compare_coefficients = ExtractJsonArrayField(
        compare_integral,
        "coefficients",
        "lane146 compare30 " + expected.integral);
    const std::string cpp_orders = ExtractJsonArrayField(
        cpp_integral,
        "epsilon_orders",
        "lane146 C++ result " + expected.integral);
    Expect(ExtractJsonObjectsFromArray(compare_coefficients,
                                       "lane146 compare30 coefficients")
               .size() == expected.eps_orders.size(),
           expected.integral +
               " should retain exactly the expected compared coefficient scope");

    for (const int eps_order : expected.eps_orders) {
      const std::string compare_coefficient = FindJsonObjectByIntField(
          compare_coefficients,
          "order",
          eps_order,
          "lane146 compare30 coefficients for " + expected.integral);
      const std::string cpp_order = FindJsonObjectByIntField(
          cpp_orders,
          "order",
          eps_order,
          "lane146 C++ result coefficients for " + expected.integral);
      Expect(ExtractJsonBoolField(compare_coefficient,
                                  "amflow_present",
                                  "lane146 compare30 coefficient"),
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should retain an AMFlow value");
      Expect(ExtractJsonBoolField(compare_coefficient,
                                  "cpp_present",
                                  "lane146 compare30 coefficient"),
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should retain a C++ runtime value");
      Expect(ExtractJsonBoolField(compare_coefficient,
                                  "passed",
                                  "lane146 compare30 coefficient"),
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should pass AMFlow parity");
      const int real_agreement_digits =
          ExtractJsonIntField(compare_coefficient,
                              "real_agreement_digits",
                              "lane146 compare30 coefficient");
      const int imaginary_agreement_digits =
          ExtractJsonIntField(compare_coefficient,
                              "imag_agreement_digits",
                              "lane146 compare30 coefficient");
      Expect(real_agreement_digits >= kLane146Selected4ToleranceDigits,
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should retain 30 real agreement digits");
      Expect(imaginary_agreement_digits >= kLane146Selected4ToleranceDigits,
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should retain 30 imaginary agreement digits");
      Expect(real_agreement_digits == kLane146Selected4ExpectedDigitAgreement,
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should retain the exact compare30 real digit agreement");
      Expect(imaginary_agreement_digits ==
                 kLane146Selected4ExpectedDigitAgreement,
             expected.integral + " eps^" + std::to_string(eps_order) +
                 " should retain the exact compare30 imaginary digit agreement");
      if (!observed_digit_agreement ||
          real_agreement_digits < observed_min_digit_agreement) {
        observed_min_digit_agreement = real_agreement_digits;
        observed_digit_agreement = true;
      }
      if (imaginary_agreement_digits < observed_min_digit_agreement) {
        observed_min_digit_agreement = imaginary_agreement_digits;
      }
      ++real_digit_agreement_checks;
      ++imaginary_digit_agreement_checks;

      const std::string amflow_real = ExtractJsonStringField(
          compare_coefficient, "amflow_real", "lane146 compare30 coefficient");
      const std::string amflow_imag = ExtractJsonStringField(
          compare_coefficient, "amflow_imag", "lane146 compare30 coefficient");
      const std::string cpp_real = ExtractJsonStringField(
          compare_coefficient, "cpp_real", "lane146 compare30 coefficient");
      const std::string cpp_imag = ExtractJsonStringField(
          compare_coefficient, "cpp_imag", "lane146 compare30 coefficient");
      ExpectDecimalNear(cpp_real,
                        amflow_real,
                        expected.integral + " eps^" +
                            std::to_string(eps_order) +
                            " retained C++ real value should match AMFlow");
      ExpectDecimalNear(cpp_imag,
                        amflow_imag,
                        expected.integral + " eps^" +
                            std::to_string(eps_order) +
                            " retained C++ imaginary value should match AMFlow");
      ExpectDecimalNear(ExtractJsonStringField(cpp_order,
                                               "real_digits",
                                               "lane146 C++ result coefficient"),
                        cpp_real,
                        expected.integral + " eps^" +
                            std::to_string(eps_order) +
                            " C++ result real value should match compare30");
      ExpectDecimalNear(ExtractJsonStringField(cpp_order,
                                               "imag_digits",
                                               "lane146 C++ result coefficient"),
                        cpp_imag,
                        expected.integral + " eps^" +
                            std::to_string(eps_order) +
                            " C++ result imaginary value should match compare30");
      ++compared_count;
    }
  }
  Expect(compared_count == 19,
         "lane146 selected4 parity test should cover all 19 compared coefficients");
  Expect(real_digit_agreement_checks == 19 &&
             imaginary_digit_agreement_checks == 19,
         "lane146 selected4 parity test should check all 38 digit-agreement fields");
  Expect(observed_min_digit_agreement ==
             ExtractJsonIntField(compare_json,
                                 "minimum_digit_agreement",
                                 "lane146 compare30"),
         "lane146 selected4 coefficient digit agreements should match the "
         "top-level compare30 minimum");
}

void ScopedAutomaticPhaseSpaceD246SolvedStateRequiresSidecarEvidenceTest() {
  struct ExpectedWeight {
    std::size_t denominator_index;
    std::string denominator_id;
  };
  const std::vector<ExpectedWeight> watched_weights = {
      {1, "D2"},
      {3, "D4"},
      {5, "D6"},
  };
  const bool has_sidecar_evidence =
      HasD246PublishedReferenceSidecarEvidence();

  for (const ExpectedWeight& expected : watched_weights) {
    const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
        amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
            MakeB63nAutomaticPhaseSpaceSpec(),
            expected.denominator_index,
            1,
            70);

    Expect(!IsSolvedScopedWeight(evaluation) || has_sidecar_evidence,
           expected.denominator_id +
               " scoped b63n weight was promoted to solved without the "
               "required AMFlow reference sidecar at " +
               kD246ReferenceEvidenceSidecar);
  }
}

void ScopedAutomaticPhaseSpaceWeightedResiduePinsRemainingWeightGapTest() {
  struct ExpectedWeightGap {
    std::size_t denominator_index;
    std::string denominator_id;
    int power;
  };
  const std::vector<ExpectedWeightGap> blocked_weights = {
      {1, "D2", 2},
      {3, "D4", 1},
      {5, "D6", 1},
  };

  for (const ExpectedWeightGap& expected : blocked_weights) {
    const amflow::CutkoskyScopedWeightedResidueEvaluation evaluation =
        amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
            MakeB63nAutomaticPhaseSpaceSpec(),
            expected.denominator_index,
            1,
            70);

    Expect(evaluation.reviewed_surface,
           "blocked automatic weight should still bind the reviewed weighted surface");
    Expect(evaluation.selected_weight_denominator == expected.denominator_id &&
               evaluation.selected_weight_denominator_index ==
                   expected.denominator_index &&
               evaluation.selected_weight_power == expected.power,
           "scoped weighted residue evaluator should preserve the requested " +
               expected.denominator_id + " weight");
    Expect(evaluation.moment_cross_validation_gate.passed,
           expected.denominator_id +
               " scoped evaluation should pass the D2,D4,D6,D7 structural gate");
    Expect(evaluation.publication_gate_checked &&
               !evaluation.publication_gate_passed,
           expected.denominator_id +
               " scoped evaluation should stop at the publication gate");
    Expect(evaluation.failure_code == "boundary_unsolved",
           expected.denominator_id +
               " scoped evaluation should remain a boundary_unsolved gap");
    Expect(!evaluation.live_coefficients_available,
           expected.denominator_id +
               " scoped evaluation must not publish an unreviewed coefficient");
    Expect(!evaluation.retained_solution_samples_used,
           expected.denominator_id +
               " scoped evaluation must not read retained final samples");
    Expect(!evaluation.full_eta_zero_contour_applied,
           expected.denominator_id +
               " scoped evaluation must not promote full eta-zero coverage");
    ExpectContains(evaluation.publication_gate_status,
                   "blocked-by-publication-gate",
                   expected.denominator_id +
                       " scoped evaluation should expose publication blocking");
    ExpectContains(evaluation.publication_gate_status,
                   "synthetic",
                   expected.denominator_id +
                       " scoped evaluation should identify the synthetic blocker");
    Expect(evaluation.eta_zero_selection.success,
           expected.denominator_id +
               " scoped evaluation should still reach the eta-zero selector");
    Expect(evaluation.eta_zero_selection.selected_coefficient_label ==
               "automatic_phasespace_" + expected.denominator_id +
                   "_weighted_moment_seed",
           expected.denominator_id +
               " scoped evaluation should preserve the seed label");

    const std::string audit =
        amflow::SerializeCutkoskyScopedWeightedResidueEvaluationAudit(evaluation);
    ExpectContains(audit,
                   "selected_weight=" + expected.denominator_id,
                   expected.denominator_id +
                       " scoped audit should identify the selected weight");
    ExpectContains(audit,
                   "publication_gate_passed=false",
                   expected.denominator_id +
                       " scoped audit should report the blocked publication gate");
    ExpectContains(audit,
                   "failure_code=boundary_unsolved",
                   expected.denominator_id +
                       " scoped audit should report the remaining boundary gap");
    ExpectContains(audit,
                   "live_coefficients_available=false",
                   expected.denominator_id +
                       " scoped audit must not claim live coefficients");
  }

  const amflow::CutkoskyScopedWeightedResidueEvaluation d7 =
      amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
          MakeB63nAutomaticPhaseSpaceSpec(),
          6,
          1,
          70);
  Expect(d7.publication_gate_passed && d7.live_coefficients_available,
         "D7 eps^0..eps^3 should remain the only reviewed published scoped weight");
  Expect(d7.reference_validation_passed,
         "D7 eps^0..eps^3 should remain tied to the stored AMFlow comparison");
  Expect(!d7.full_eta_zero_contour_applied,
         "the scoped D7 coefficient must not close the full weighted residue lane");
  ExpectContains(d7.summary,
                 "D2/D4/D6",
                 "D7 scoped summary should name the remaining automatic weight gap");
  ExpectContains(d7.summary,
                 "feynman_prescription",
                 "D7 scoped summary should preserve the companion b63n row gap");
}

void ScopedAutomaticPhaseSpaceWeightedResidueRejectsCutDenominatorTest() {
  ExpectExceptionContains(
      []() {
        static_cast<void>(
            amflow::EvaluateAutomaticPhaseSpaceScopedWeightedResidue(
                MakeB63nAutomaticPhaseSpaceSpec(),
                0,
                1,
                70));
      },
      "reviewed uncut weight",
      "scoped weighted residue evaluator should reject cut denominator selection");
}

}  // namespace

int main(const int argc, char** argv) {
  try {
    if (argc == 2 &&
        std::string(argv[1]) == "--emit-b63n-weighted-residue-fingerprints") {
      EmitB63nWeightedResidueAuditFingerprintsJson(std::cout);
      return 0;
    }
    if (argc != 1) {
      std::cerr << "usage: cutkosky-weighted-residue-tests "
                   "[--emit-b63n-weighted-residue-fingerprints]\n";
      return 2;
    }

    ScopedAutomaticPhaseSpaceWeightedResidueStopsAtPublicationGateTest();
    ScopedAutomaticPhaseSpaceWeightedResidueCanSelectD7Test();
    ScopedAutomaticPhaseSpaceWeightedResidueAuditFingerprintDriftTest();
    ScopedAutomaticPhaseSpacePublishedD7OutputIsByteDeterministicTest();
    ScopedAutomaticPhaseSpaceWeightedResidueMomentSeedPermutationInvariantTest();
    ScopedAutomaticPhaseSpaceWeightedResidueKinematicRescalingInvariantTest();
    ScopedFeynmanPrescriptionWeightedResiduePlanRescalingConjugateCompositionTest();
    ScopedAutomaticD7SeedSignComposesWithFeynmanConjugateFlipTest();
    ScopedAutomaticPhaseSpaceWeightedResidueProvenanceDiagnosticsTransformInvariantTest();
    ScopedAutomaticPhaseSpaceSeedIdentitySignBoundaryTest();
    ScopedAutomaticPhaseSpacePublishedD7MatchesLane146AMFlowCompare30Test();
    ScopedAutomaticPhaseSpaceSelected4PinsAllLane146ComparedCoefficientsTest();
    ScopedAutomaticPhaseSpaceD246SolvedStateRequiresSidecarEvidenceTest();
    ScopedAutomaticPhaseSpaceWeightedResiduePinsRemainingWeightGapTest();
    ScopedAutomaticPhaseSpaceWeightedResidueRejectsCutDenominatorTest();
  } catch (const std::exception& error) {
    std::cerr << "cutkosky-weighted-residue-tests failed: " << error.what()
              << '\n';
    return 1;
  }
  return 0;
}
