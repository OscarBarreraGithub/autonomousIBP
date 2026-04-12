#include "amflow/core/options.hpp"

#include <sstream>

namespace amflow {

namespace {

std::string Quote(const std::string& value) {
  return "\"" + value + "\"";
}

std::string Join(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    out << Quote(values[index]);
  }
  return out.str();
}

}  // namespace

std::string ToString(ReductionMode mode) {
  switch (mode) {
    case ReductionMode::Kira:
      return "Kira";
    case ReductionMode::FireFly:
      return "FireFly";
    case ReductionMode::Mixed:
      return "Mixed";
    case ReductionMode::NoFactorScan:
      return "NoFactorScan";
    case ReductionMode::Masters:
      return "Masters";
  }
  return "Kira";
}

std::string SerializeAmfOptionsYaml(const AmfOptions& options) {
  std::ostringstream out;
  out << "amf_options:\n";
  out << "  AMFMode: [" << Join(options.amf_modes) << "]\n";
  out << "  EndingScheme: [" << Join(options.ending_schemes) << "]\n";
  out << "  D0: " << Quote(options.d0) << "\n";
  out << "  WorkingPre: " << options.working_precision << "\n";
  out << "  ChopPre: " << options.chop_precision << "\n";
  out << "  XOrder: " << options.x_order << "\n";
  out << "  ExtraXOrder: " << options.extra_x_order << "\n";
  out << "  LearnXOrder: " << options.learn_x_order << "\n";
  out << "  TestXOrder: " << options.test_x_order << "\n";
  out << "  RationalizePre: " << options.rationalize_precision << "\n";
  out << "  RunLength: " << options.run_length << "\n";
  out << "  UseCache: " << (options.use_cache ? "true" : "false") << "\n";
  out << "  SkipReduction: " << (options.skip_reduction ? "true" : "false") << "\n";
  return out.str();
}

std::string SerializeReductionOptionsYaml(const ReductionOptions& options) {
  std::ostringstream out;
  out << "reduction_options:\n";
  out << "  IBPReducer: " << Quote(options.ibp_reducer) << "\n";
  out << "  BlackBoxRank: " << options.black_box_rank << "\n";
  out << "  BlackBoxDot: " << options.black_box_dot << "\n";
  out << "  ComplexMode: " << (options.complex_mode ? "true" : "false") << "\n";
  out << "  DeleteBlackBoxDirectory: "
      << (options.delete_black_box_directory ? "true" : "false") << "\n";
  out << "  IntegralOrder: " << options.integral_order << "\n";
  out << "  ReductionMode: " << Quote(ToString(options.reduction_mode)) << "\n";
  if (options.permutation_option.has_value()) {
    out << "  PermutationOption: " << *options.permutation_option << "\n";
  }
  if (options.master_rank.has_value()) {
    out << "  MasterRank: " << *options.master_rank << "\n";
  }
  if (options.master_dot.has_value()) {
    out << "  MasterDot: " << *options.master_dot << "\n";
  }
  return out.str();
}

}  // namespace amflow
