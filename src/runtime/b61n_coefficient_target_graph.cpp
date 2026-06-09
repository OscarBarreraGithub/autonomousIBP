#include "amflow/runtime/b61n_coefficient_target_graph.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace amflow {

namespace {

struct NodeLess {
  bool operator()(const B61nCoefficientNode& lhs,
                  const B61nCoefficientNode& rhs) const {
    if (lhs.master_index != rhs.master_index) {
      return lhs.master_index < rhs.master_index;
    }
    return lhs.eps_order < rhs.eps_order;
  }
};

struct MatrixSupportLess {
  bool operator()(const B61nMatrixEpsilonSupport& lhs,
                  const B61nMatrixEpsilonSupport& rhs) const {
    return std::tie(lhs.eps_order, lhs.row, lhs.column) <
           std::tie(rhs.eps_order, rhs.row, rhs.column);
  }
};

std::string RemoveAsciiSpaces(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](const unsigned char character) {
                               return std::isspace(character) != 0;
                             }),
              value.end());
  return value;
}

std::string EpsOrderLabel(const int eps_order) {
  if (eps_order == 0) {
    return "eps^0";
  }
  return "eps^" + std::to_string(eps_order);
}

std::size_t FindRequiredLabel(const std::vector<std::string>& master_labels,
                              const std::string& label) {
  const auto it =
      std::find(master_labels.begin(), master_labels.end(), label);
  if (it == master_labels.end()) {
    throw std::runtime_error(
        "b61n coefficient target graph missing retained master " + label);
  }
  return static_cast<std::size_t>(it - master_labels.begin());
}

bool IsZeroLiteral(const std::string& compact) {
  if (compact.empty()) {
    return true;
  }
  if (compact == "0") {
    return true;
  }
  bool saw_digit = false;
  bool saw_nonzero = false;
  bool saw_decimal = false;
  for (const char character : compact) {
    if (character == '.') {
      if (saw_decimal) {
        return false;
      }
      saw_decimal = true;
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(character)) == 0) {
      return false;
    }
    saw_digit = true;
    if (character != '0') {
      saw_nonzero = true;
    }
  }
  return saw_digit && !saw_nonzero;
}

using EpsilonSupport = std::set<int>;

EpsilonSupport AddSupport(EpsilonSupport lhs, const EpsilonSupport& rhs) {
  lhs.insert(rhs.begin(), rhs.end());
  return lhs;
}

EpsilonSupport MultiplySupport(const EpsilonSupport& lhs,
                               const EpsilonSupport& rhs) {
  EpsilonSupport result;
  if (lhs.empty() || rhs.empty()) {
    return result;
  }
  for (const int lhs_order : lhs) {
    for (const int rhs_order : rhs) {
      result.insert(lhs_order + rhs_order);
    }
  }
  return result;
}

EpsilonSupport ShiftSupport(const EpsilonSupport& support, const int shift) {
  EpsilonSupport result;
  for (const int order : support) {
    result.insert(order + shift);
  }
  return result;
}

EpsilonSupport PowerSupport(const EpsilonSupport& base, const int exponent) {
  if (exponent == 0) {
    return {0};
  }
  if (base.empty()) {
    return {};
  }
  if (exponent < 0) {
    if (base.size() != 1) {
      throw std::invalid_argument(
          "b61n matrix epsilon support requires negative powers to have "
          "monomial epsilon support");
    }
    return {*base.begin() * exponent};
  }
  if (exponent > 64) {
    throw std::invalid_argument(
        "b61n matrix epsilon support refuses an exponent above 64");
  }
  EpsilonSupport result = {0};
  for (int count = 0; count < exponent; ++count) {
    result = MultiplySupport(result, base);
  }
  return result;
}

class EpsilonSupportParser {
 public:
  explicit EpsilonSupportParser(std::string expression)
      : expression_(std::move(expression)) {}

  EpsilonSupport Parse() {
    const EpsilonSupport support = ParseExpression();
    SkipSpaces();
    if (position_ != expression_.size()) {
      throw Error("unexpected trailing token");
    }
    return support;
  }

 private:
  EpsilonSupport ParseExpression() {
    EpsilonSupport value = ParseTerm();
    while (true) {
      SkipSpaces();
      if (Consume('+')) {
        value = AddSupport(std::move(value), ParseTerm());
      } else if (Consume('-')) {
        value = AddSupport(std::move(value), ParseTerm());
      } else {
        return value;
      }
    }
  }

  EpsilonSupport ParseTerm() {
    EpsilonSupport value = ParsePower();
    while (true) {
      SkipSpaces();
      if (Consume('*')) {
        value = MultiplySupport(value, ParsePower());
      } else if (Consume('/')) {
        const EpsilonSupport denominator = ParsePower();
        if (denominator.empty()) {
          throw Error("zero denominator epsilon support");
        }
        if (denominator.size() != 1) {
          throw Error("non-monomial denominator epsilon support");
        }
        value = ShiftSupport(value, -*denominator.begin());
      } else {
        return value;
      }
    }
  }

  EpsilonSupport ParsePower() {
    EpsilonSupport value = ParseUnary();
    SkipSpaces();
    if (Consume('^')) {
      value = PowerSupport(value, ParseIntegerExponent());
    }
    return value;
  }

  EpsilonSupport ParseUnary() {
    SkipSpaces();
    if (Consume('+') || Consume('-')) {
      return ParseUnary();
    }
    return ParsePrimary();
  }

  EpsilonSupport ParsePrimary() {
    SkipSpaces();
    if (Consume('(')) {
      EpsilonSupport value = ParseExpression();
      if (!Consume(')')) {
        throw Error("missing right parenthesis");
      }
      return value;
    }
    if (position_ >= expression_.size()) {
      throw Error("unexpected end of expression");
    }
    const char character = expression_[position_];
    if (std::isdigit(static_cast<unsigned char>(character)) != 0 ||
        character == '.') {
      ParseNumber();
      return {0};
    }
    if (std::isalpha(static_cast<unsigned char>(character)) != 0 ||
        character == '_') {
      const std::string identifier = ParseIdentifier();
      return identifier == "eps" ? EpsilonSupport{1} : EpsilonSupport{0};
    }
    throw Error("unexpected token");
  }

  int ParseIntegerExponent() {
    SkipSpaces();
    int sign = 1;
    if (Consume('+')) {
      sign = 1;
    } else if (Consume('-')) {
      sign = -1;
    }
    if (position_ >= expression_.size() ||
        std::isdigit(static_cast<unsigned char>(expression_[position_])) == 0) {
      throw Error("epsilon support exponent must be an integer literal");
    }
    int value = 0;
    while (position_ < expression_.size() &&
           std::isdigit(static_cast<unsigned char>(expression_[position_])) !=
               0) {
      value = value * 10 + (expression_[position_] - '0');
      ++position_;
    }
    return sign * value;
  }

  void ParseNumber() {
    bool saw_digit = false;
    bool saw_decimal = false;
    while (position_ < expression_.size()) {
      const char character = expression_[position_];
      if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
        saw_digit = true;
        ++position_;
        continue;
      }
      if (character == '.') {
        if (saw_decimal) {
          break;
        }
        saw_decimal = true;
        ++position_;
        continue;
      }
      break;
    }
    if (!saw_digit) {
      throw Error("malformed numeric literal");
    }
  }

  std::string ParseIdentifier() {
    const std::size_t start = position_;
    while (position_ < expression_.size()) {
      const char character = expression_[position_];
      if (std::isalnum(static_cast<unsigned char>(character)) == 0 &&
          character != '_') {
        break;
      }
      ++position_;
    }
    return expression_.substr(start, position_ - start);
  }

  void SkipSpaces() {
    while (position_ < expression_.size() &&
           std::isspace(static_cast<unsigned char>(expression_[position_])) !=
               0) {
      ++position_;
    }
  }

  bool Consume(const char character) {
    SkipSpaces();
    if (position_ < expression_.size() && expression_[position_] == character) {
      ++position_;
      return true;
    }
    return false;
  }

  std::invalid_argument Error(const std::string& message) const {
    return std::invalid_argument("b61n matrix epsilon support parse failed: " +
                                 message + " near offset " +
                                 std::to_string(position_) + " in " +
                                 expression_);
  }

  std::string expression_;
  std::size_t position_ = 0;
};

EpsilonSupport ParseCellEpsilonSupport(const std::string& cell) {
  const std::string compact = RemoveAsciiSpaces(cell);
  if (IsZeroLiteral(compact)) {
    return {};
  }
  return EpsilonSupportParser(cell).Parse();
}

bool InRange(const B61nCoefficientOrderRange& range, const int order) {
  return order >= range.min_eps_order && order <= range.max_eps_order;
}

std::vector<B61nCoefficientNode> SortedNodes(
    const std::set<B61nCoefficientNode, NodeLess>& nodes) {
  return {nodes.begin(), nodes.end()};
}

std::vector<B61nMatrixEpsilonSupport> SortedMatrixSupport(
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support) {
  std::set<B61nMatrixEpsilonSupport, MatrixSupportLess> unique_support(
      matrix_support.begin(), matrix_support.end());
  return {unique_support.begin(), unique_support.end()};
}

std::string JoinNodeList(const std::vector<B61nCoefficientNode>& nodes,
                         const std::vector<std::string>& master_labels) {
  std::ostringstream out;
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << FormatB61nCoefficientNode(nodes[index], master_labels);
  }
  return out.str();
}

std::string JoinMatrixSupportList(
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support,
    const std::vector<std::string>& master_labels) {
  std::ostringstream out;
  for (std::size_t index = 0; index < matrix_support.size(); ++index) {
    const B61nMatrixEpsilonSupport& support = matrix_support[index];
    if (index > 0) {
      out << ", ";
    }
    out << EpsOrderLabel(support.eps_order) << ":"
        << master_labels[support.row] << "<-" << master_labels[support.column];
  }
  return out.str();
}

std::vector<B61nCoefficientNode> BuildReviewedSourceAnchorNodes(
    const std::vector<std::string>& master_labels,
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support,
    const std::vector<B61nCoefficientOrderRange>& ranges,
    const std::vector<B61nCoefficientNode>& public_targets) {
  int min_matrix_order = matrix_support.front().eps_order;
  int max_matrix_order = matrix_support.front().eps_order;
  for (const B61nMatrixEpsilonSupport& support : matrix_support) {
    min_matrix_order = std::min(min_matrix_order, support.eps_order);
    max_matrix_order = std::max(max_matrix_order, support.eps_order);
  }
  int min_target_order = public_targets.front().eps_order;
  int max_target_order = public_targets.front().eps_order;
  for (const B61nCoefficientNode& target : public_targets) {
    min_target_order = std::min(min_target_order, target.eps_order);
    max_target_order = std::max(max_target_order, target.eps_order);
  }

  const int anchor_min_order = min_target_order - max_matrix_order;
  const int anchor_max_order = max_target_order - min_matrix_order;
  std::set<B61nCoefficientNode, NodeLess> nodes;
  for (const std::string& label : B61nRow56ReviewedSourceAnchorLabels()) {
    const std::size_t index = FindRequiredLabel(master_labels, label);
    for (int order = std::max(anchor_min_order, ranges[index].min_eps_order);
         order <= std::min(anchor_max_order, ranges[index].max_eps_order);
         ++order) {
      nodes.insert({index, order});
    }
  }
  return SortedNodes(nodes);
}

}  // namespace

const std::vector<std::string>& B61nRow56RetainedMasterLabels() {
  static const std::vector<std::string> labels = {
      "box[0,0,0,1]",
      "box[1,0,1,0]",
      "box[1,0,0,1]",
      "box[0,1,0,1]",
      "box[0,0,1,1]",
      "box[1,0,1,1]",
      "box[1,1,1,1]",
  };
  return labels;
}

const std::vector<std::string>& B61nRow56ReviewedSourceAnchorLabels() {
  static const std::vector<std::string> labels = {
      "box[0,0,0,1]",
      "box[1,0,1,0]",
      "box[1,0,0,1]",
      "box[0,1,0,1]",
      "box[0,0,1,1]",
  };
  return labels;
}

std::vector<B61nCoefficientNode> B61nRow56PublicTargetNodes(
    const std::vector<std::string>& master_labels) {
  return {
      {FindRequiredLabel(master_labels, "box[1,0,1,1]"), 0},
      {FindRequiredLabel(master_labels, "box[1,1,1,1]"), -2},
      {FindRequiredLabel(master_labels, "box[1,1,1,1]"), -1},
      {FindRequiredLabel(master_labels, "box[1,1,1,1]"), 0},
  };
}

std::vector<B61nCoefficientOrderRange> B61nDefaultRow56CoefficientOrderRanges(
    const std::vector<std::string>& master_labels) {
  std::vector<B61nCoefficientOrderRange> ranges(master_labels.size(), {0, 0});
  for (const std::string& label : B61nRow56ReviewedSourceAnchorLabels()) {
    ranges[FindRequiredLabel(master_labels, label)] = {-1, 2};
  }
  ranges[FindRequiredLabel(master_labels, "box[1,0,1,1]")] = {0, 0};
  ranges[FindRequiredLabel(master_labels, "box[1,1,1,1]")] = {-2, 0};
  return ranges;
}

std::vector<B61nMatrixEpsilonSupport> ExtractB61nMatrixEpsilonSupport(
    const std::vector<std::vector<std::string>>& matrix) {
  if (matrix.empty()) {
    throw std::invalid_argument(
        "b61n coefficient target graph requires a nonempty eta matrix");
  }
  const std::size_t dimension = matrix.size();
  std::vector<B61nMatrixEpsilonSupport> support;
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    if (matrix[row].size() != dimension) {
      throw std::invalid_argument(
          "b61n coefficient target graph encountered a malformed matrix row");
    }
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      const EpsilonSupport cell_support =
          ParseCellEpsilonSupport(matrix[row][column]);
      for (const int eps_order : cell_support) {
        support.push_back({row, column, eps_order});
      }
    }
  }
  support = SortedMatrixSupport(support);
  if (support.empty()) {
    throw std::invalid_argument(
        "b61n coefficient target graph found empty matrix epsilon support");
  }
  return support;
}

B61nCoefficientTargetGraph BuildB61nCoefficientTargetGraph(
    const std::vector<std::string>& master_labels,
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support,
    const std::vector<B61nCoefficientOrderRange>& coefficient_order_ranges,
    const std::vector<B61nCoefficientNode>& public_targets,
    const std::vector<B61nCoefficientNode>& reviewed_source_anchor_nodes) {
  if (master_labels.empty()) {
    throw std::invalid_argument(
        "b61n coefficient target graph requires retained master labels");
  }
  if (matrix_support.empty()) {
    throw std::invalid_argument(
        "b61n coefficient target graph found empty matrix epsilon support");
  }
  if (coefficient_order_ranges.size() != master_labels.size()) {
    throw std::invalid_argument(
        "b61n coefficient target graph coefficient ranges do not match masters");
  }
  if (public_targets.empty()) {
    throw std::invalid_argument(
        "b61n coefficient target graph requires public target nodes");
  }

  std::set<std::string> unique_labels;
  for (const std::string& label : master_labels) {
    if (label.empty() || !unique_labels.insert(label).second) {
      throw std::invalid_argument(
          "b61n coefficient target graph requires unique nonempty master labels");
    }
  }
  for (const B61nCoefficientOrderRange& range : coefficient_order_ranges) {
    if (range.min_eps_order > range.max_eps_order) {
      throw std::invalid_argument(
          "b61n coefficient target graph received an invalid coefficient range");
    }
  }

  B61nCoefficientTargetGraph graph;
  graph.public_targets = public_targets;
  graph.reviewed_source_anchor_nodes = reviewed_source_anchor_nodes;
  graph.matrix_support = SortedMatrixSupport(matrix_support);
  graph.min_matrix_eps_order = graph.matrix_support.front().eps_order;
  graph.max_matrix_eps_order = graph.matrix_support.front().eps_order;
  for (const B61nMatrixEpsilonSupport& support : graph.matrix_support) {
    if (support.row >= master_labels.size() ||
        support.column >= master_labels.size()) {
      throw std::invalid_argument(
          "b61n coefficient target graph matrix support references an unknown "
          "master index");
    }
    graph.min_matrix_eps_order =
        std::min(graph.min_matrix_eps_order, support.eps_order);
    graph.max_matrix_eps_order =
        std::max(graph.max_matrix_eps_order, support.eps_order);
  }

  std::set<B61nCoefficientNode, NodeLess> closed_nodes;
  std::vector<B61nCoefficientNode> queue;
  const auto seed_node = [&](const B61nCoefficientNode& node,
                             const std::string& context) {
    if (node.master_index >= master_labels.size()) {
      throw std::invalid_argument(
          "b61n coefficient target graph " + context +
          " references an unknown master index");
    }
    if (!InRange(coefficient_order_ranges[node.master_index], node.eps_order)) {
      throw std::invalid_argument(
          "b61n coefficient target graph " + context +
          " is outside declared coefficient support");
    }
    if (closed_nodes.insert(node).second) {
      queue.push_back(node);
    }
  };
  for (const B61nCoefficientNode& target : public_targets) {
    seed_node(target, "public target");
  }
  for (const B61nCoefficientNode& anchor : reviewed_source_anchor_nodes) {
    seed_node(anchor, "reviewed source anchor");
  }

  std::set<std::tuple<std::size_t, int, std::size_t, int, int>> edge_keys;
  std::set<std::tuple<std::size_t, int, std::size_t, int, int>> blocked_keys;
  for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
    const B61nCoefficientNode target = queue[cursor];
    for (const B61nMatrixEpsilonSupport& support : graph.matrix_support) {
      if (support.row != target.master_index) {
        continue;
      }
      const int source_eps_order = target.eps_order - support.eps_order;
      if (!InRange(coefficient_order_ranges[support.column],
                   source_eps_order)) {
        const auto key = std::make_tuple(target.master_index,
                                         target.eps_order,
                                         support.column,
                                         source_eps_order,
                                         support.eps_order);
        if (blocked_keys.insert(key).second) {
          graph.blocked_edges.push_back(
              {target,
               support.column,
               source_eps_order,
               support.eps_order,
               "outside_declared_coefficient_support"});
        }
        continue;
      }
      const B61nCoefficientNode source{support.column, source_eps_order};
      const auto key = std::make_tuple(target.master_index,
                                       target.eps_order,
                                       source.master_index,
                                       source.eps_order,
                                       support.eps_order);
      if (edge_keys.insert(key).second) {
        graph.dependency_edges.push_back({target, source, support.eps_order});
      }
      if (closed_nodes.insert(source).second) {
        queue.push_back(source);
      }
    }
  }
  graph.closed_nodes = SortedNodes(closed_nodes);

  std::ostringstream summary;
  summary << "b61n coefficient target graph closed "
          << graph.closed_nodes.size() << " coefficient node(s) from "
          << graph.public_targets.size()
          << " public row56 target node(s) and "
          << graph.reviewed_source_anchor_nodes.size()
          << " reviewed source-anchor node(s); matrix_eps_order_min="
          << graph.min_matrix_eps_order << "; matrix_eps_order_max="
          << graph.max_matrix_eps_order << "; matrix_support_count="
          << graph.matrix_support.size() << "; dependency_edge_count="
          << graph.dependency_edges.size()
          << "; blocked_out_of_range_edge_count="
          << graph.blocked_edges.size() << "; public_targets=["
          << JoinNodeList(graph.public_targets, master_labels)
          << "]; reviewed_source_anchor_nodes=["
          << JoinNodeList(graph.reviewed_source_anchor_nodes, master_labels)
          << "]; closed_target_nodes=["
          << JoinNodeList(graph.closed_nodes, master_labels)
          << "]; matrix_epsilon_support=["
          << JoinMatrixSupportList(graph.matrix_support, master_labels) << "]";
  graph.summary = summary.str();
  return graph;
}

B61nCoefficientTargetGraph BuildB61nRow56CoefficientTargetGraph(
    const std::vector<std::string>& master_labels,
    const std::vector<B61nMatrixEpsilonSupport>& matrix_support) {
  for (const std::string& expected : B61nRow56RetainedMasterLabels()) {
    static_cast<void>(FindRequiredLabel(master_labels, expected));
  }
  const std::vector<B61nCoefficientNode> public_targets =
      B61nRow56PublicTargetNodes(master_labels);
  const std::vector<B61nCoefficientOrderRange> ranges =
      B61nDefaultRow56CoefficientOrderRanges(master_labels);
  const std::vector<B61nCoefficientNode> reviewed_source_anchor_nodes =
      BuildReviewedSourceAnchorNodes(master_labels,
                                     SortedMatrixSupport(matrix_support),
                                     ranges,
                                     public_targets);
  return BuildB61nCoefficientTargetGraph(master_labels,
                                         matrix_support,
                                         ranges,
                                         public_targets,
                                         reviewed_source_anchor_nodes);
}

std::string FormatB61nCoefficientNode(
    const B61nCoefficientNode& node,
    const std::vector<std::string>& master_labels) {
  std::ostringstream out;
  if (node.master_index < master_labels.size()) {
    out << master_labels[node.master_index];
  } else {
    out << "master[" << node.master_index << "]";
  }
  out << ":" << EpsOrderLabel(node.eps_order);
  return out.str();
}

}  // namespace amflow
