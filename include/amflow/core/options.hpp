#pragma once

#include <optional>
#include <string>
#include <vector>

#include "amflow/kira/kira_insert_prefactors.hpp"

namespace amflow {

enum class ReductionMode {
  Kira,
  FireFly,
  Mixed,
  NoFactorScan,
  Masters
};

std::string ToString(ReductionMode mode);

struct AmfOptions {
  std::vector<std::string> amf_modes = {"Prescription", "Mass", "Propagator"};
  std::vector<std::string> ending_schemes = {"Tradition", "Cutkosky", "SingleMass", "Trivial"};
  std::string d0 = "4";
  std::optional<std::string> fixed_eps;
  int working_precision = 100;
  int chop_precision = 20;
  int x_order = 100;
  int extra_x_order = 20;
  int learn_x_order = -1;
  int test_x_order = 5;
  int rationalize_precision = 100;
  int run_length = 1000;
  bool use_cache = false;
  bool skip_reduction = false;
};

struct ReductionOptions {
  std::string ibp_reducer = "Kira";
  int black_box_rank = 3;
  int black_box_dot = 0;
  bool complex_mode = true;
  bool delete_black_box_directory = false;
  int integral_order = 5;
  ReductionMode reduction_mode = ReductionMode::Kira;
  bool kira_insert_prefactors = false;
  std::optional<KiraInsertPrefactorsSurface> kira_insert_prefactors_surface;
  std::optional<int> permutation_option;
  std::optional<int> master_rank;
  std::optional<int> master_dot;
};

std::string SerializeAmfOptionsYaml(const AmfOptions& options);
std::string SerializeReductionOptionsYaml(const ReductionOptions& options);

}  // namespace amflow
