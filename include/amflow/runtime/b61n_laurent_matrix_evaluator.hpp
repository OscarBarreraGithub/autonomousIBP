#pragma once

#include <map>
#include <string>
#include <vector>

#include "amflow/runtime/b61n_coefficient_target_graph.hpp"
#include "amflow/runtime/complex_contour_propagator.hpp"

namespace amflow {

using B61nLaurentNumericSubstitutions =
    std::map<std::string, ComplexContourNumber>;

struct B61nLaurentMatrixEvaluation {
  std::vector<ComplexContourMatrix> matrix_coefficients;
  std::vector<B61nMatrixEpsilonSupport> nonzero_support;
  int min_matrix_eps_order = 0;
  int max_matrix_eps_order = 0;
  std::size_t matrix_coefficient_count = 0;
};

struct B61nLaurentMatrixEvaluatorAudit {
  int min_matrix_eps_order = 0;
  int max_matrix_eps_order = 0;
  std::size_t matrix_coefficient_count = 0;
  std::vector<B61nMatrixEpsilonSupport> nonzero_support;
  std::string fingerprint;
  std::string summary;
};

struct B61nLaurentMatrixEvaluator {
  ComplexContourLaurentMatrixEvaluator evaluator;
  B61nLaurentMatrixEvaluatorAudit audit;
};

B61nLaurentMatrixEvaluation EvaluateB61nLaurentMatrixCoefficientsAtEta(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name,
    const B61nLaurentNumericSubstitutions& numeric_substitutions,
    const ComplexContourNumber& eta);

B61nLaurentMatrixEvaluator BuildB61nRealLaurentMatrixEvaluator(
    const std::vector<std::vector<std::string>>& matrix,
    const std::string& variable_name,
    const B61nLaurentNumericSubstitutions& numeric_substitutions,
    const B61nCoefficientTargetGraph& target_graph);

}  // namespace amflow
