cmake_minimum_required(VERSION 3.20)

project(amflow_cpp VERSION 0.1.0 LANGUAGES CXX)

include(cmake/Dependencies.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(amflow
  src/core/problem_spec.cpp
  src/core/options.cpp
  src/core/de_system.cpp
  src/core/boundary_data.cpp
  src/de/eta_derivative_generation.cpp
  src/de/invariant_derivative_generation.cpp
  src/de/lightlike_linear_derivative_generation.cpp
  src/de/lightlike_linear_derivative_reduction_execution.cpp
  src/de/lightlike_linear_derivative_reduction_preparation.cpp
  src/de/eta_reduction_execution.cpp
  src/de/eta_reduction_preparation.cpp
  src/de/invariant_reduction_execution.cpp
  src/de/invariant_reduction_preparation.cpp
  src/de/lightlike_linear_reduction_execution.cpp
  src/de/lightlike_linear_reduction_preparation.cpp
  src/de/reduction_assembly.cpp
  src/io/problem_spec_io.cpp
  src/io/sample_data.cpp
  src/kira/kira_backend.cpp
  src/kira/kira_insert_prefactors.cpp
  src/kira/target_reduction.cpp
  src/runtime/artifact_store.cpp
  src/runtime/auxiliary_family.cpp
  src/runtime/b61n_coefficient_state_transport.cpp
  src/runtime/b61n_coefficient_target_graph.cpp
  src/runtime/b61n_finite_start_coefficients.cpp
  src/runtime/b61n_laurent_matrix_evaluator.cpp
  src/runtime/boundary_generation.cpp
  src/runtime/complex_contour_propagator.cpp
  src/runtime/continuation_path.cpp
  src/runtime/cutkosky_transport.cpp
  src/runtime/endpoint_branch_ledger.cpp
  src/runtime/endpoint_local_model.cpp
  src/runtime/eta_mode.cpp
  src/runtime/ending_scheme.cpp
  src/runtime/lightlike_propagator.cpp
  src/runtime/physical_kinematics_guardrails.cpp
  src/solver/boundary_provider.cpp
  src/solver/boundary_request.cpp
  src/solver/coefficient_evaluator.cpp
  src/solver/precision_policy.cpp
  src/solver/series_solver.cpp
  src/solver/singular_point_analysis.cpp
)

target_include_directories(amflow
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(amflow PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(amflow PUBLIC stdc++fs)
endif()
amflow_apply_default_warnings(amflow)
amflow_apply_optional_dependencies(amflow)

add_executable(amflow-cli src/cli/main.cpp)
target_link_libraries(amflow-cli PRIVATE amflow)
target_compile_features(amflow-cli PRIVATE cxx_std_17)
amflow_apply_default_warnings(amflow-cli)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_executable(amflow-tests tests/amflow_tests.cpp)
  target_link_libraries(amflow-tests PRIVATE amflow)
  target_compile_features(amflow-tests PRIVATE cxx_std_17)
  target_compile_definitions(
    amflow-tests PRIVATE
      AMFLOW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
      AMFLOW_PYTHON_EXECUTABLE="${Python3_EXECUTABLE}"
  )
  amflow_apply_default_warnings(amflow-tests)
  add_test(NAME amflow-tests COMMAND amflow-tests)
  set_tests_properties(amflow-tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 2400)

  add_test(
    NAME b64ag-golden-recapture-readiness-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b64ag_golden_recapture_readiness.py
      --self-check
  )
  set_tests_properties(b64ag-golden-recapture-readiness-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b64ag-first-block-gap-diagnostic-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/diagnose_b64ag_first_block_gap.py
      --self-check
  )
  set_tests_properties(b64ag-first-block-gap-diagnostic-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b63n-d246-evidence-verifier-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b63n_d246_evidence.py
      --self-check
  )
  set_tests_properties(b63n-d246-evidence-verifier-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b63n-selected4-permutation-audit-verifier
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b63n_selected4_permutation_audit.py
  )
  set_tests_properties(b63n-selected4-permutation-audit-verifier PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b63n-selected4-permutation-audit-verifier-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b63n_selected4_permutation_audit.py
      --self-check
  )
  set_tests_properties(b63n-selected4-permutation-audit-verifier-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-row56-specific-target-diagnostic-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/diagnose_b61n_row56_specific_targets.py
      --self-check
  )
  set_tests_properties(b61n-row56-specific-target-diagnostic-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-qualifier-determinism
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --determinism-check
  )
  set_tests_properties(b61n-publication-qualifier-determinism PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-precision-evidence-binding
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --precision-evidence-binding-check
  )
  set_tests_properties(b61n-publication-precision-evidence-binding PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-precision-inventory-consistency
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --precision-inventory-consistency-check
  )
  set_tests_properties(b61n-publication-precision-inventory-consistency PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-diagnostic-fields
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --diagnostic-fields-check
  )
  set_tests_properties(b61n-publication-diagnostic-fields PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-reference-floor-parity-gate
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_reference_floor_parity.py
  )
  set_tests_properties(b61n-reference-floor-parity-gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-precision-uplift-monotonicity
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_precision_uplift_monotonicity.py
  )
  set_tests_properties(b61n-precision-uplift-monotonicity PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-precision-uplift-monotonicity-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_precision_uplift_monotonicity.py
      --self-check
  )
  set_tests_properties(b61n-precision-uplift-monotonicity-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-exact-rational-extension-stability
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_exact_rational_extension_stability.py
  )
  set_tests_properties(b61n-exact-rational-extension-stability PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-exact-rational-extension-stability-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_exact_rational_extension_stability.py
      --self-check
  )
  set_tests_properties(b61n-exact-rational-extension-stability-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-signoff-readiness-accepted-inputs
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_signoff_ready.py
  )
  set_tests_properties(m7-release-signoff-readiness-accepted-inputs PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-signoff-readiness-failure-modes
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_signoff_failures.py
  )
  set_tests_properties(m7-release-signoff-readiness-failure-modes PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-sidecar-schema-validation
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_m7_release_sidecar_schemas.py
  )
  set_tests_properties(m7-release-sidecar-schema-validation PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-evidence-bundle-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/package_m7_release_evidence.py
      --self-check
  )
  set_tests_properties(m7-release-evidence-bundle-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-markdown-docs-validation
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_release_markdown.py
  )
  set_tests_properties(m7-release-markdown-docs-validation PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-sidecar-inventory-audit
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m7_sidecar_inventory.py
      --verify
      --verify-schema-reconciliation
      --summary-only
  )
  set_tests_properties(m7-release-sidecar-inventory-audit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(singular-runtime-lane-tests tests/singular_runtime_lane_tests.cpp)
  target_link_libraries(singular-runtime-lane-tests PRIVATE amflow)
  target_compile_features(singular-runtime-lane-tests PRIVATE cxx_std_17)
  amflow_apply_default_warnings(singular-runtime-lane-tests)
  add_test(NAME singular-runtime-lane-tests COMMAND singular-runtime-lane-tests)
  set_tests_properties(singular-runtime-lane-tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(cutkosky-weighted-residue-tests
    tests/cutkosky_weighted_residue_tests.cpp)
  target_link_libraries(cutkosky-weighted-residue-tests PRIVATE amflow)
  target_compile_features(cutkosky-weighted-residue-tests PRIVATE cxx_std_17)
  amflow_apply_default_warnings(cutkosky-weighted-residue-tests)
  add_test(
    NAME cutkosky-weighted-residue-tests
    COMMAND cutkosky-weighted-residue-tests)
  set_tests_properties(cutkosky-weighted-residue-tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME b63n-weighted-residue-fingerprint-regenerator-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/regenerate_b63n_weighted_residue_fingerprints.py
      --self-check
      --test-binary
      $<TARGET_FILE:cutkosky-weighted-residue-tests>)
  set_tests_properties(b63n-weighted-residue-fingerprint-regenerator-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME b63n-scoped-gate-audit-trail-query-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b63n_scoped_gate_audit_trail.py
      --self-check
      --test-binary
      $<TARGET_FILE:cutkosky-weighted-residue-tests>)
  set_tests_properties(b63n-scoped-gate-audit-trail-query-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_dependencies(
    amflow-tests
    amflow-cli
    singular-runtime-lane-tests
    cutkosky-weighted-residue-tests)
endif()
