#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace amflow {

struct B61nCoefficientNode {
  std::size_t master_index = 0;
  int eps_order = 0;
};

struct B61nMatrixEpsilonSupport {
  std::size_t row = 0;
  std::size_t column = 0;
  int eps_order = 0;
};

struct B61nCoefficientOrderRange {
  int min_eps_order = 0;
  int max_eps_order = 0;
};

struct B61nCoefficientGraphEdge {
  B61nCoefficientNode target;
  B61nCoefficientNode source;
  int matrix_eps_order = 0;
};

struct B61nBlockedCoefficientGraphEdge {
  B61nCoefficientNode target;
  std::size_t source_master_index = 0;
  int source_eps_order = 0;
  int matrix_eps_order = 0;
  std::string reason;
};

struct B61nCoefficientTargetGraph {
  std::vector<B61nCoefficientNode> public_targets;
  std::vector<B61nCoefficientNode> reviewed_source_anchor_nodes;
  std::vector<B61nCoefficientNode> closed_nodes;
  std::vector<B61nMatrixEpsilonSupport> matrix_support;
  std::vector<B61nCoefficientGraphEdge> dependency_edges;
  std::vector<B61nBlockedCoefficientGraphEdge> blocked_edges;
  int min_matrix_eps_order = 0;
  int max_matrix_eps_order = 0;
  std::string summary;
};

const std::vector<std::string>& B61nRow56RetainedMasterLabels();
const std::vector<std::string>& B61nRow56ReviewedSourceAnchorLabels();

std::vector<B61nCoefficientNode> B61nRow56PublicTargetNodes(
    const std::vector<std::string>& master_labels);

std::vector<B61nCoefficientOrderRange> B61nDefaultRow56CoefficientOrderRanges(
    const std::vector<std::string>& master_labels);

std::vector<B61nMatrixEpsilonSupport> ExtractB61nMatrixEpsilonSupport(
    const std::vector<std::vector<std::string>>& matrix);

B61nCoefficientTargetGraph BuildB61nCoefficientTargetGraph(
    const std::vector<std::string>& master_labels,
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support,
    const std::vector<B61nCoefficientOrderRange>& coefficient_order_ranges,
    const std::vector<B61nCoefficientNode>& public_targets,
    const std::vector<B61nCoefficientNode>& reviewed_source_anchor_nodes = {});

B61nCoefficientTargetGraph BuildB61nRow56CoefficientTargetGraph(
    const std::vector<std::string>& master_labels,
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support);

std::string FormatB61nCoefficientNode(
    const B61nCoefficientNode& node,
    const std::vector<std::string>& master_labels);

}  // namespace amflow
