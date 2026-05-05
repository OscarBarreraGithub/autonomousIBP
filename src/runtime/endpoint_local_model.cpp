#include "amflow/runtime/endpoint_local_model.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "amflow/solver/series_solver.hpp"

namespace amflow {

namespace {

ExactRational ZeroRational() {
  return {"0", "1"};
}

ExactRational ExactArithmetic(const std::string& expression) {
  return EvaluateCoefficientExpression(expression, NumericEvaluationPoint{});
}

ExactRational SubtractRational(const ExactRational& lhs, const ExactRational& rhs) {
  return ExactArithmetic("(" + lhs.ToString() + ")-(" + rhs.ToString() + ")");
}

std::string JoinMessages(const std::vector<std::string>& messages) {
  std::ostringstream out;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << messages[index];
  }
  return out.str();
}

EtaEndpointLocalModelAnalysis MakeFailure(std::string failure_code, std::string summary) {
  EtaEndpointLocalModelAnalysis analysis;
  analysis.success = false;
  analysis.failure_code = std::move(failure_code);
  analysis.summary = std::move(summary);
  return analysis;
}

bool SameCanonicalRational(const ExactRational& lhs, const ExactRational& rhs) {
  try {
    return ExactArithmetic(lhs.ToString()) == ExactArithmetic(rhs.ToString());
  } catch (const std::exception&) {
    return false;
  }
}

bool IsZeroComplexEndpoint(const ExactComplexRational& value) {
  return SameCanonicalRational(value.real, ZeroRational()) &&
         SameCanonicalRational(value.imaginary, ZeroRational());
}

std::optional<long long> ParseExactInteger(const ExactRational& value) {
  const ExactRational normalized = ExactArithmetic(value.ToString());
  if (normalized.denominator != "1") {
    return std::nullopt;
  }
  try {
    return std::stoll(normalized.numerator);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

NumericEvaluationPoint BuildEndpointLocalModelExactPassiveBindings(const ProblemSpec& spec) {
  NumericEvaluationPoint passive_bindings;
  for (const auto& [name, value] : spec.kinematics.numeric_substitutions) {
    static_cast<void>(EvaluateCoefficientExpression(value, NumericEvaluationPoint{}));
    passive_bindings.emplace(name, value);
  }
  for (const auto& [name, value] : spec.kinematics.complex_numeric_substitutions) {
    try {
      static_cast<void>(EvaluateCoefficientExpression(value, NumericEvaluationPoint{}));
      passive_bindings.emplace(name, value);
    } catch (const std::exception&) {
      // Complex-valued substitutions remain unsupported for SRL-2 exact local
      // models unless the reviewed endpoint matrix does not depend on them.
    }
  }
  return passive_bindings;
}

ExactRationalMatrix MakeDiagonalResidueMatrix(
    const std::vector<ExactRational>& indicial_exponents) {
  ExactRationalMatrix residue_matrix(
      indicial_exponents.size(),
      std::vector<ExactRational>(indicial_exponents.size(), ZeroRational()));
  for (std::size_t index = 0; index < indicial_exponents.size(); ++index) {
    residue_matrix[index][index] = indicial_exponents[index];
  }
  return residue_matrix;
}

std::optional<std::string> FirstUnsupportedResonance(
    const std::vector<ExactRational>& indicial_exponents) {
  for (std::size_t row = 0; row < indicial_exponents.size(); ++row) {
    for (std::size_t column = 0; column < indicial_exponents.size(); ++column) {
      if (row == column) {
        continue;
      }
      const ExactRational separation =
          SubtractRational(indicial_exponents[row], indicial_exponents[column]);
      const std::optional<long long> integer_separation = ParseExactInteger(separation);
      if (integer_separation.has_value() && *integer_separation != 0) {
        return "exponent[" + std::to_string(row) + "] - exponent[" +
               std::to_string(column) + "] = " + separation.ToString();
      }
    }
  }
  return std::nullopt;
}

std::string ClassifyFrobeniusFailureCode(const std::string& message) {
  if (message.find("logarithmic resonance") != std::string::npos ||
      message.find("logarithmic Frobenius") != std::string::npos) {
    return "srl2_endpoint_logarithmic_model_unsupported";
  }
  if (message.find("requires a singular center") != std::string::npos &&
      message.find(" is regular") != std::string::npos) {
    return "srl2_endpoint_regular_model_unsupported";
  }
  if (message.find("requires the selected coefficient matrix to be square") !=
          std::string::npos ||
      message.find("diagonal simple-pole residue matrix") != std::string::npos ||
      message.find("strictly lower-triangular") != std::string::npos ||
      message.find("higher-order pole") != std::string::npos) {
    return "srl2_endpoint_matrix_form_unsupported";
  }
  if (message.find("unknown symbol") != std::string::npos ||
      message.find("imaginary unit") != std::string::npos) {
    return "srl2_endpoint_complex_coefficient_unsupported";
  }
  return "srl2_endpoint_local_model_unsupported";
}

}  // namespace

EtaEndpointLocalModelAnalysis AnalyzeEtaEndpointLocalModel(
    const DESystem& system,
    const ProblemSpec& spec,
    const EtaContinuationPlan& plan,
    const int extraction_order) {
  if (extraction_order < 0) {
    return MakeFailure("srl2_endpoint_invalid_order",
                       "SRL-2 endpoint local model requires a non-negative extraction order");
  }

  const std::vector<std::string> spec_messages = ValidateProblemSpec(spec);
  if (!spec_messages.empty()) {
    return MakeFailure("srl2_endpoint_problem_spec_invalid", JoinMessages(spec_messages));
  }
  const std::vector<std::string> system_messages = ValidateDESystem(system);
  if (!system_messages.empty()) {
    return MakeFailure("srl2_endpoint_de_system_invalid", JoinMessages(system_messages));
  }

  if (!spec.complex_mode) {
    return MakeFailure(
        "srl2_endpoint_complex_mode_required",
        "SRL-2 endpoint local-model analysis is branch-sensitive complex eta=0 evidence; "
        "direct-real Frobenius support is not accepted as singular-runtime-lane progress");
  }
  if (!plan.target_endpoint_singular.has_value()) {
    return MakeFailure(
        "srl2_endpoint_marker_missing",
        "SRL-2 endpoint local-model analysis requires an explicit target_endpoint_singular "
        "marker from the reviewed contour contract");
  }
  if (plan.contour_points.empty()) {
    return MakeFailure("srl2_endpoint_contour_missing",
                       "SRL-2 endpoint local-model analysis requires contour endpoint data");
  }

  const EtaContourSingularPoint& endpoint_singular = *plan.target_endpoint_singular;
  if (endpoint_singular.value != plan.contour_points.back()) {
    return MakeFailure(
        "srl2_endpoint_marker_mismatch",
        "SRL-2 endpoint local-model marker value must match the final contour point");
  }
  if (!endpoint_singular.value.IsReal()) {
    return MakeFailure(
        "srl2_endpoint_complex_center_unsupported",
        "SRL-2 endpoint local-model analysis currently supports only real exact endpoint "
        "centers");
  }
  if (!IsZeroComplexEndpoint(endpoint_singular.value)) {
    return MakeFailure(
        "srl2_endpoint_not_eta_zero",
        "SRL-2 endpoint local-model analysis is limited to the reviewed eta=0 singular "
        "endpoint");
  }

  const NumericEvaluationPoint passive_bindings =
      BuildEndpointLocalModelExactPassiveBindings(spec);
  const std::string endpoint_expression =
      plan.eta_symbol + "=" + endpoint_singular.value.real.ToString();

  UpperTriangularMatrixFrobeniusSeriesPatch patch;
  try {
    patch = GenerateUpperTriangularMatrixFrobeniusSeriesPatch(system,
                                                             plan.eta_symbol,
                                                             endpoint_expression,
                                                             extraction_order,
                                                             passive_bindings);
  } catch (const std::invalid_argument& error) {
    return MakeFailure(ClassifyFrobeniusFailureCode(error.what()), error.what());
  } catch (const std::runtime_error& error) {
    return MakeFailure(ClassifyFrobeniusFailureCode(error.what()), error.what());
  }

  std::vector<ExactRational> indicial_exponents;
  indicial_exponents.reserve(patch.indicial_exponents.size());
  for (std::size_t index = 0; index < patch.indicial_exponents.size(); ++index) {
    const ExactRational exponent = ExactArithmetic(patch.indicial_exponents[index]);
    if (!ParseExactInteger(exponent).has_value()) {
      return MakeFailure(
          "srl2_endpoint_fractional_exponent_unsupported",
          "SRL-2 endpoint local-model analysis found unsupported fractional Frobenius "
          "exponent[" +
              std::to_string(index) + "]=" + exponent.ToString());
    }
    indicial_exponents.push_back(exponent);
  }

  const std::optional<std::string> resonance =
      FirstUnsupportedResonance(indicial_exponents);
  if (resonance.has_value()) {
    return MakeFailure(
        "srl2_endpoint_resonance_unsupported",
        "SRL-2 endpoint local-model analysis found unsupported resonant Frobenius "
        "separation: " +
            *resonance);
  }

  EtaEndpointLocalModel model;
  model.eta_symbol = plan.eta_symbol;
  model.endpoint_expression = endpoint_singular.expression;
  model.endpoint_value = endpoint_singular.value;
  model.half_plane = plan.half_plane;
  model.contour_fingerprint = plan.contour_fingerprint;
  model.local_model_kind =
      "srl2-upper-triangular-frobenius-simple-pole-integer-nonresonant";
  model.extraction_order = extraction_order;
  model.branch_sensitive = true;
  model.live_endpoint_extraction_ready = false;
  model.basis_functions = patch.basis_functions;
  model.residue_matrix = MakeDiagonalResidueMatrix(indicial_exponents);
  model.indicial_exponents.reserve(indicial_exponents.size());
  for (const ExactRational& exponent : indicial_exponents) {
    model.indicial_exponents.push_back(exponent.ToString());
  }

  EtaEndpointLocalModelAnalysis analysis;
  analysis.success = true;
  analysis.summary =
      "SRL-2 endpoint local model constructed; branch ledger and live eta=0 endpoint "
      "extraction remain deferred";
  analysis.model = std::move(model);
  return analysis;
}

}  // namespace amflow
