#include "amflow/runtime/b61n_finite_start_coefficients.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <boost/math/special_functions/fpclassify.hpp>

#include "amflow/runtime/artifact_store.hpp"

namespace amflow {

namespace {

struct NodeLess {
  bool operator()(const B61nCoefficientNode& lhs,
                  const B61nCoefficientNode& rhs) const {
    return std::tie(lhs.master_index, lhs.eps_order) <
           std::tie(rhs.master_index, rhs.eps_order);
  }
};

struct StoredCoefficient {
  ComplexContourNumber value;
  B61nFiniteStartCoefficientSource source =
      B61nFiniteStartCoefficientSource::EtaInfinityCoefficientRecurrence;
  std::string provenance;
};

bool IsFiniteFloat(const ComplexContourFloat& value) {
  return boost::math::isfinite(value);
}

bool IsFiniteComplex(const ComplexContourNumber& value) {
  return IsFiniteFloat(value.real()) && IsFiniteFloat(value.imag());
}

std::string CompactFloat(const ComplexContourFloat& raw_value,
                         const int precision_digits = 50) {
  if (raw_value == ComplexContourFloat(0)) {
    return "0";
  }
  std::ostringstream stream;
  stream << std::setprecision(precision_digits) << raw_value;
  return stream.str();
}

std::string CompactComplex(const ComplexContourNumber& value,
                           const int precision_digits = 50) {
  return "(" + CompactFloat(value.real(), precision_digits) + "," +
         CompactFloat(value.imag(), precision_digits) + ")";
}

std::string FormatNodeList(const std::vector<B61nCoefficientNode>& nodes,
                           const std::vector<std::string>& master_labels) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << FormatB61nCoefficientNode(nodes[index], master_labels);
  }
  out << "]";
  return out.str();
}

std::string FormatAvailableNodeList(
    const std::vector<B61nFiniteStartAvailableCoefficient>& nodes,
    const std::vector<std::string>& master_labels) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << FormatB61nCoefficientNode(nodes[index].node, master_labels)
        << "@" << B61nFiniteStartCoefficientSourceLabel(nodes[index].source);
  }
  out << "]";
  return out.str();
}

std::vector<B61nCoefficientNode> SortedUniqueNodes(
    std::vector<B61nCoefficientNode> nodes) {
  std::sort(nodes.begin(), nodes.end(), NodeLess{});
  nodes.erase(std::unique(nodes.begin(),
                          nodes.end(),
                          [](const B61nCoefficientNode& lhs,
                             const B61nCoefficientNode& rhs) {
                            return lhs.master_index == rhs.master_index &&
                                   lhs.eps_order == rhs.eps_order;
                          }),
              nodes.end());
  return nodes;
}

void RequireValidNode(const B61nCoefficientNode& node,
                      const std::vector<std::string>& master_labels,
                      const std::string& context) {
  if (node.master_index >= master_labels.size()) {
    throw std::invalid_argument(
        "b61n finite-start coefficient data received out-of-range " + context);
  }
}

std::map<B61nCoefficientNode, StoredCoefficient, NodeLess>
CollectAvailableCoefficients(
    const std::vector<std::string>& master_labels,
    const std::set<B61nCoefficientNode, NodeLess>& required_nodes,
    const std::vector<B61nFiniteStartCoefficientValue>& eta_infinity_coefficients,
    const std::vector<B61nFiniteStartCoefficientValue>&
        reviewed_source_anchor_coefficients) {
  std::map<B61nCoefficientNode, StoredCoefficient, NodeLess> available;
  const auto add_values =
      [&](const std::vector<B61nFiniteStartCoefficientValue>& values,
          const B61nFiniteStartCoefficientSource source) {
        for (const B61nFiniteStartCoefficientValue& candidate : values) {
          RequireValidNode(candidate.node, master_labels, "coefficient node");
          if (!IsFiniteComplex(candidate.value)) {
            throw std::invalid_argument(
                "b61n finite-start coefficient data found nonfinite coefficient "
                "for " + FormatB61nCoefficientNode(candidate.node, master_labels));
          }
          if (required_nodes.find(candidate.node) == required_nodes.end()) {
            continue;
          }
          const auto [it, inserted] = available.emplace(
              candidate.node,
              StoredCoefficient{candidate.value, source, candidate.provenance});
          (void)it;
          if (!inserted) {
            throw std::invalid_argument(
                "b61n finite-start coefficient data received duplicate source "
                "for " + FormatB61nCoefficientNode(candidate.node, master_labels));
          }
        }
      };

  add_values(eta_infinity_coefficients,
             B61nFiniteStartCoefficientSource::EtaInfinityCoefficientRecurrence);
  add_values(reviewed_source_anchor_coefficients,
             B61nFiniteStartCoefficientSource::
                 ReviewedSourceAnchorCoefficientStateTransport);
  return available;
}

std::string SerializeAuditForFingerprint(
    const std::vector<std::string>& master_labels,
    const B61nFiniteStartCoefficientAudit& audit) {
  std::ostringstream out;
  out << "kind=b61n-coefficient-finite-start-audit\n";
  out << "finite_start_eta=" << audit.finite_start_eta << "\n";
  out << "min_state_eps_order=" << audit.min_state_eps_order << "\n";
  out << "max_state_eps_order=" << audit.max_state_eps_order << "\n";
  out << "finite_start_samples_present="
      << (audit.finite_start_samples_present ? "true" : "false") << "\n";
  out << "populated_from_finite_start_samples="
      << (audit.populated_from_finite_start_samples ? "true" : "false")
      << "\n";
  out << "solve_vandermonde_fit_used="
      << (audit.solve_vandermonde_fit_used ? "true" : "false") << "\n";
  for (const B61nCoefficientNode& node : audit.required_nodes) {
    out << "required=" << FormatB61nCoefficientNode(node, master_labels) << "\n";
  }
  for (const B61nFiniteStartAvailableCoefficient& node : audit.available_nodes) {
    out << "available=" << FormatB61nCoefficientNode(node.node, master_labels)
        << "; source=" << B61nFiniteStartCoefficientSourceLabel(node.source)
        << "; provenance=" << node.provenance << "\n";
  }
  for (const B61nCoefficientNode& node : audit.missing_nodes) {
    out << "missing=" << FormatB61nCoefficientNode(node, master_labels) << "\n";
  }
  return out.str();
}

std::string SerializeDataForFingerprint(
    const std::vector<std::string>& master_labels,
    const B61nFiniteStartCoefficientData& data) {
  std::ostringstream out;
  out << "kind=b61n-coefficient-finite-start-data\n";
  out << "min_state_eps_order=" << data.min_state_eps_order << "\n";
  out << "max_state_eps_order=" << data.max_state_eps_order << "\n";
  for (const B61nCoefficientNode& node : data.materialized_nodes) {
    out << "materialized=" << FormatB61nCoefficientNode(node, master_labels)
        << "\n";
  }
  for (std::size_t order_index = 0; order_index < data.coefficient_vectors.size();
       ++order_index) {
    const int eps_order = data.min_state_eps_order +
                          static_cast<int>(order_index);
    for (std::size_t master = 0;
         master < data.coefficient_vectors[order_index].size();
         ++master) {
      out << "coefficient[" << eps_order << "," << master << "]="
          << CompactComplex(data.coefficient_vectors[order_index][master], 70)
          << "\n";
    }
  }
  return out.str();
}

}  // namespace

std::string B61nFiniteStartCoefficientSourceLabel(
    const B61nFiniteStartCoefficientSource source) {
  switch (source) {
    case B61nFiniteStartCoefficientSource::EtaInfinityCoefficientRecurrence:
      return "eta_infinity_coefficient_recurrence";
    case B61nFiniteStartCoefficientSource::
        ReviewedSourceAnchorCoefficientStateTransport:
      return "reviewed_source_anchor_reverse_coefficient_state_transport";
  }
  return "unknown";
}

B61nFiniteStartCoefficientAudit AuditB61nFiniteStartCoefficientData(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const std::vector<B61nFiniteStartCoefficientValue>& eta_infinity_coefficients,
    const std::vector<B61nFiniteStartCoefficientValue>&
        reviewed_source_anchor_coefficients,
    const bool finite_start_samples_present,
    const std::string& finite_start_eta) {
  if (master_labels.empty()) {
    throw std::invalid_argument(
        "b61n finite-start coefficient data requires retained master labels");
  }
  if (target_graph.closed_nodes.empty()) {
    throw std::invalid_argument(
        "b61n finite-start coefficient data requires a nonempty target graph");
  }

  B61nFiniteStartCoefficientAudit audit;
  audit.finite_start_samples_present = finite_start_samples_present;
  audit.finite_start_eta =
      finite_start_eta.empty() ? "unspecified" : finite_start_eta;
  audit.required_nodes = SortedUniqueNodes(target_graph.closed_nodes);
  audit.required_node_count = audit.required_nodes.size();
  audit.min_state_eps_order = audit.required_nodes.front().eps_order;
  audit.max_state_eps_order = audit.required_nodes.front().eps_order;
  for (const B61nCoefficientNode& node : audit.required_nodes) {
    RequireValidNode(node, master_labels, "target graph node");
    audit.min_state_eps_order =
        std::min(audit.min_state_eps_order, node.eps_order);
    audit.max_state_eps_order =
        std::max(audit.max_state_eps_order, node.eps_order);
  }
  audit.source_anchor_reverse_coefficient_transport_required =
      !target_graph.reviewed_source_anchor_nodes.empty();

  const std::set<B61nCoefficientNode, NodeLess> required_set(
      audit.required_nodes.begin(), audit.required_nodes.end());
  const std::map<B61nCoefficientNode, StoredCoefficient, NodeLess> available =
      CollectAvailableCoefficients(master_labels,
                                   required_set,
                                   eta_infinity_coefficients,
                                   reviewed_source_anchor_coefficients);

  for (const B61nCoefficientNode& node : audit.required_nodes) {
    const auto available_it = available.find(node);
    if (available_it == available.end()) {
      audit.missing_nodes.push_back(node);
      continue;
    }
    B61nFiniteStartAvailableCoefficient available_node;
    available_node.node = node;
    available_node.source = available_it->second.source;
    available_node.provenance = available_it->second.provenance;
    audit.available_nodes.push_back(std::move(available_node));
    if (available_it->second.source ==
        B61nFiniteStartCoefficientSource::EtaInfinityCoefficientRecurrence) {
      ++audit.eta_infinity_recurrence_available_count;
    } else {
      ++audit.reviewed_source_anchor_transport_available_count;
    }
  }

  audit.available_node_count = audit.available_nodes.size();
  audit.missing_node_count = audit.missing_nodes.size();
  audit.finite_start_coefficients_available = audit.missing_nodes.empty();
  audit.source_anchor_reverse_coefficient_transport_available =
      audit.reviewed_source_anchor_transport_available_count > 0;
  audit.fingerprint =
      ComputeArtifactFingerprint(SerializeAuditForFingerprint(master_labels, audit));
  audit.summary =
      "b61n coefficient-level finite-start data dry-run; finite_start_eta=" +
      audit.finite_start_eta +
      "; required_coefficient_node_count=" +
      std::to_string(audit.required_node_count) +
      "; available_coefficient_node_count=" +
      std::to_string(audit.available_node_count) +
      "; missing_coefficient_node_count=" +
      std::to_string(audit.missing_node_count) +
      "; min_state_eps_order=" + std::to_string(audit.min_state_eps_order) +
      "; max_state_eps_order=" + std::to_string(audit.max_state_eps_order) +
      "; required_coefficient_nodes=" +
      FormatNodeList(audit.required_nodes, master_labels) +
      "; available_coefficient_nodes=" +
      FormatAvailableNodeList(audit.available_nodes, master_labels) +
      "; missing_coefficient_nodes=" +
      FormatNodeList(audit.missing_nodes, master_labels) +
      "; eta_infinity_recurrence_available_count=" +
      std::to_string(audit.eta_infinity_recurrence_available_count) +
      "; reviewed_source_anchor_reverse_coefficient_transport_available_count=" +
      std::to_string(audit.reviewed_source_anchor_transport_available_count) +
      "; source_anchor_reverse_coefficient_transport_required=" +
      std::string(audit.source_anchor_reverse_coefficient_transport_required
                      ? "true"
                      : "false") +
      "; source_anchor_reverse_coefficient_transport_available=" +
      std::string(audit.source_anchor_reverse_coefficient_transport_available
                      ? "true"
                      : "false") +
      "; finite_start_samples_present=" +
      std::string(audit.finite_start_samples_present ? "true" : "false") +
      "; populated_from_finite_start_samples=false; finite_start_samples_used=false; "
      "solve_vandermonde_fit_used=false; finite_start_coefficients_available=" +
      std::string(audit.finite_start_coefficients_available ? "true" : "false") +
      "; coefficient_data_materialized=false; coefficient_state_transport_applied=false; "
      "coefficient_state_endpoint_matcher_applied=false; "
      "target_coefficients_published_from_coefficient_state=false; "
      "target_coefficients_reconstructed_from_epsilon_samples=false; "
      "coefficient_publication=false; fingerprint=" + audit.fingerprint;
  return audit;
}

B61nFiniteStartCoefficientData BuildB61nFiniteStartCoefficientData(
    const std::vector<std::string>& master_labels,
    const B61nCoefficientTargetGraph& target_graph,
    const std::vector<B61nFiniteStartCoefficientValue>& eta_infinity_coefficients,
    const std::vector<B61nFiniteStartCoefficientValue>&
        reviewed_source_anchor_coefficients,
    const bool finite_start_samples_present,
    const std::string& finite_start_eta) {
  const B61nFiniteStartCoefficientAudit audit =
      AuditB61nFiniteStartCoefficientData(master_labels,
                                          target_graph,
                                          eta_infinity_coefficients,
                                          reviewed_source_anchor_coefficients,
                                          finite_start_samples_present,
                                          finite_start_eta);
  if (!audit.finite_start_coefficients_available) {
    throw std::runtime_error(
        "b61n finite-start coefficient data is missing required coefficient "
        "nodes; " +
        audit.summary);
  }

  const std::set<B61nCoefficientNode, NodeLess> required_set(
      audit.required_nodes.begin(), audit.required_nodes.end());
  const std::map<B61nCoefficientNode, StoredCoefficient, NodeLess> available =
      CollectAvailableCoefficients(master_labels,
                                   required_set,
                                   eta_infinity_coefficients,
                                   reviewed_source_anchor_coefficients);

  B61nFiniteStartCoefficientData data;
  data.min_state_eps_order = audit.min_state_eps_order;
  data.max_state_eps_order = audit.max_state_eps_order;
  const std::size_t order_count = static_cast<std::size_t>(
      static_cast<long long>(data.max_state_eps_order) -
      static_cast<long long>(data.min_state_eps_order) + 1LL);
  data.coefficient_vectors.assign(
      order_count,
      ComplexContourVector(master_labels.size(), ComplexContourNumber{}));

  for (const B61nCoefficientNode& node : audit.required_nodes) {
    const auto available_it = available.find(node);
    if (available_it == available.end()) {
      throw std::runtime_error(
          "b61n finite-start coefficient materialization lost required node " +
          FormatB61nCoefficientNode(node, master_labels));
    }
    const std::size_t order_index =
        static_cast<std::size_t>(node.eps_order - data.min_state_eps_order);
    data.coefficient_vectors[order_index][node.master_index] =
        available_it->second.value;
    data.materialized_nodes.push_back(node);
  }

  data.fingerprint =
      ComputeArtifactFingerprint(SerializeDataForFingerprint(master_labels, data));
  data.summary =
      "Materialized b61n coefficient-level finite-start vectors from explicit "
      "coefficient sources; materialized_node_count=" +
      std::to_string(data.materialized_nodes.size()) +
      "; min_state_eps_order=" + std::to_string(data.min_state_eps_order) +
      "; max_state_eps_order=" + std::to_string(data.max_state_eps_order) +
      "; master_dimension=" + std::to_string(master_labels.size()) +
      "; populated_from_finite_start_samples=false; finite_start_samples_used=false; "
      "solve_vandermonde_fit_used=false; coefficient_data_materialized=true; "
      "fingerprint=" + data.fingerprint;
  return data;
}

}  // namespace amflow
