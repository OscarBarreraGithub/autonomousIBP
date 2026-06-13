cmake_minimum_required(VERSION 3.20)

project(amflow_cpp VERSION 0.1.0 LANGUAGES CXX)

include(cmake/Dependencies.cmake)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(AMFLOW_ENABLE_COVERAGE_BASELINE_GATE "Enable the coverage baseline CTest gate." ON)
option(AMFLOW_ENABLE_CTEST_TIME_BASELINE_GATE "Enable the CTest execution-time baseline gate." ON)

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

set(AMFLOW_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/amflow"
  CACHE STRING "CMake package configuration install directory")

install(
  TARGETS amflow amflow-cli
  EXPORT amflowTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")

install(
  EXPORT amflowTargets
  NAMESPACE amflow::
  DESTINATION ${AMFLOW_INSTALL_CMAKEDIR})

configure_package_config_file(
  cmake/amflowConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/amflowConfig.cmake
  INSTALL_DESTINATION ${AMFLOW_INSTALL_CMAKEDIR})

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/amflowConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/amflowConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/amflowConfigVersion.cmake
  DESTINATION ${AMFLOW_INSTALL_CMAKEDIR})

configure_file(
  cmake/cmake_uninstall.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
  @ONLY)

add_custom_target(
  uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

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
    NAME cmake-install-uninstall-roundtrip
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_cmake_install_roundtrip.py
      --cmake
      ${CMAKE_COMMAND}
      --build-dir
      ${CMAKE_CURRENT_BINARY_DIR}
      --source-dir
      ${CMAKE_CURRENT_SOURCE_DIR}
      --install-root
      ${CMAKE_CURRENT_BINARY_DIR}/install-roundtrip-gate
      --bindir
      ${CMAKE_INSTALL_BINDIR}
      --libdir
      ${CMAKE_INSTALL_LIBDIR}
      --includedir
      ${CMAKE_INSTALL_INCLUDEDIR}
      --library-filename
      $<TARGET_FILE_NAME:amflow>
      --cli-filename
      $<TARGET_FILE_NAME:amflow-cli>
      --config
      $<CONFIG>)
  set_tests_properties(cmake-install-uninstall-roundtrip PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 300)

  add_test(
    NAME cmake-build-testing-off-configure
    COMMAND
      ${CMAKE_COMMAND}
      -S
      ${CMAKE_CURRENT_SOURCE_DIR}
      -B
      ${CMAKE_CURRENT_BINARY_DIR}/cmake-build-testing-off-configure
      -DBUILD_TESTING=OFF
      -DAMFLOW_WITH_GINAC=OFF
      -DAMFLOW_WITH_MPFR=OFF
      -DAMFLOW_WITH_YAML_CPP=OFF
  )
  set_tests_properties(cmake-build-testing-off-configure PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME amflow-upstream-version-pin
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_amflow_upstream_version_pin.py
      --verify
  )
  set_tests_properties(amflow-upstream-version-pin PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME amflow-upstream-version-pin-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_amflow_upstream_version_pin.py
      --self-check
  )
  set_tests_properties(amflow-upstream-version-pin-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME amflow-public-symbol-surface
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_public_symbol_surface.py
      --target
      $<TARGET_FILE:amflow>
      --manifest
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/abi/amflow-public-symbols.txt
  )
  set_tests_properties(amflow-public-symbol-surface PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME amflow-upstream-example-inventory
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_upstream_example_inventory.py
      --verify
  )
  set_tests_properties(amflow-upstream-example-inventory PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME amflow-public-symbol-surface-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_public_symbol_surface.py
      --self-check
  )
  set_tests_properties(amflow-public-symbol-surface-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME source-license-header-gate
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_source_license_headers.py
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/license/source-license-header-baseline.txt
  )
  set_tests_properties(source-license-header-gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME source-license-header-gate-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_source_license_headers.py
      --self-check
  )
  set_tests_properties(source-license-header-gate-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME env-var-registry
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_env_var_registry.py
      --verify
      --registry
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/env-var-registry.json
  )
  set_tests_properties(env-var-registry PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME env-var-registry-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_env_var_registry.py
      --self-check
  )
  set_tests_properties(env-var-registry-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME include-order-baseline
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_include_order_baseline.py
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/diagnostics/include-order-baseline.json
  )
  set_tests_properties(include-order-baseline PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME include-order-baseline-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_include_order_baseline.py
      --self-check
  )
  set_tests_properties(include-order-baseline-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME shell-cmake-lint-baseline
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_shell_cmake_lint_baseline.py
      --build-dir
      ${CMAKE_CURRENT_BINARY_DIR}/shell-cmake-lint-baseline
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/diagnostics/shell-cmake-lint-baseline.json
  )
  set_tests_properties(shell-cmake-lint-baseline PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 300)

  add_test(
    NAME shell-cmake-lint-baseline-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_shell_cmake_lint_baseline.py
      --self-check
  )
  set_tests_properties(shell-cmake-lint-baseline-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME python-type-baseline
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_python_type_baseline.py
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/diagnostics/python-type-baseline.json
  )
  set_tests_properties(python-type-baseline PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 600)

  add_test(
    NAME python-type-baseline-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_python_type_baseline.py
      --self-check
  )
  set_tests_properties(python-type-baseline-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME cpp-api-doc-coverage-baseline
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_api_doc_coverage_baseline.py
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/diagnostics/api-doc-coverage-baseline.json
  )
  set_tests_properties(cpp-api-doc-coverage-baseline PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME cpp-api-doc-coverage-baseline-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_api_doc_coverage_baseline.py
      --self-check
  )
  set_tests_properties(cpp-api-doc-coverage-baseline-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME amflow-upstream-example-inventory-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_upstream_example_inventory.py
      --self-check
  )
  set_tests_properties(amflow-upstream-example-inventory-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME amflow-compile-warning-baseline
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_compile_warning_baseline.py
      --build-dir
      ${CMAKE_CURRENT_BINARY_DIR}/compile-warning-baseline
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/diagnostics/compile-warning-src-baseline.json
  )
  set_tests_properties(amflow-compile-warning-baseline PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 900)

  if(AMFLOW_ENABLE_COVERAGE_BASELINE_GATE)
    add_test(
      NAME amflow-coverage-baseline
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_coverage_baseline.py
        --build-dir
        ${CMAKE_CURRENT_BINARY_DIR}/coverage-baseline
        --baseline
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/coverage/amflow-coverage-baseline.json
    )
    set_tests_properties(amflow-coverage-baseline PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 5400)

    add_test(
      NAME amflow-coverage-baseline-self-check
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_coverage_baseline.py
        --self-check
    )
    set_tests_properties(amflow-coverage-baseline-self-check PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()

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
    NAME b64ag-phase0-packet-field-drift-gate
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m6_readiness_sidecar_closure.py
      --self-check
  )
  set_tests_properties(b64ag-phase0-packet-field-drift-gate PROPERTIES
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
    NAME b63n-d246-evidence-verifier
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b63n_d246_evidence.py
  )
  set_tests_properties(b63n-d246-evidence-verifier PROPERTIES
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
    NAME b61n-publication-diagnostic-field-removal-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --diagnostic-field-removal-self-check
  )
  set_tests_properties(b61n-publication-diagnostic-field-removal-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-diagnostic-field-addition-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --diagnostic-field-addition-self-check
  )
  set_tests_properties(b61n-publication-diagnostic-field-addition-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-audit-field-query-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_publication_audit_field.py
      --self-check
  )
  set_tests_properties(b61n-publication-audit-field-query-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME b61n-publication-qualifier-failure-modes-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_b61n_publication_qualifier.py
      --self-check
  )
  set_tests_properties(b61n-publication-qualifier-failure-modes-self-check PROPERTIES
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
    NAME b61n-reference-floor-parity-gate-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_reference_floor_parity.py
      --self-check
  )
  set_tests_properties(b61n-reference-floor-parity-gate-self-check PROPERTIES
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
    NAME m6-sidecar-shape-validation
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_m6_sidecar_shapes.py
      --verify
  )
  set_tests_properties(m6-sidecar-shape-validation PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m6-sidecar-shape-validation-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_m6_sidecar_shapes.py
      --self-check
  )
  set_tests_properties(m6-sidecar-shape-validation-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m6-sidecar-drift-audit
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m6_sidecar_drift.py
      --verify
      --verify-readiness-closure
      --summary-only
  )
  set_tests_properties(m6-sidecar-drift-audit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m6-sidecar-drift-audit-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m6_sidecar_drift.py
      --self-check
  )
  set_tests_properties(m6-sidecar-drift-audit-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m6-unaccepted-sidecar-promotion-guard
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m6_sidecar_drift.py
      --verify
      --verify-unaccepted-not-promoted
      --summary-only
  )
  set_tests_properties(m6-unaccepted-sidecar-promotion-guard PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME boundary-sidecar-provenance-audit
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_boundary_sidecar_provenance.py
  )
  set_tests_properties(boundary-sidecar-provenance-audit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME boundary-sidecar-provenance-audit-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_boundary_sidecar_provenance.py
      --self-check
  )
  set_tests_properties(boundary-sidecar-provenance-audit-self-check PROPERTIES
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
    NAME m7-release-signoff-readiness-performance-baseline
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_signoff_performance.py
  )
  set_tests_properties(m7-release-signoff-readiness-performance-baseline PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 45)

  add_test(
    NAME m7-signoff-ctest-coverage-audit
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m7_signoff_ctest_coverage.py
      --verify
  )
  set_tests_properties(m7-signoff-ctest-coverage-audit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-signoff-ctest-coverage-audit-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m7_signoff_ctest_coverage.py
      --self-check
  )
  set_tests_properties(m7-signoff-ctest-coverage-audit-self-check PROPERTIES
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
    NAME m7-release-evidence-manifest-digest-fixture
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_evidence_manifest_digest.py
  )
  set_tests_properties(m7-release-evidence-manifest-digest-fixture PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-evidence-tarball-size
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m7_release_tarball_size.py
      --baseline
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/release/m7-tarball-size-baseline.json
  )
  set_tests_properties(m7-release-evidence-tarball-size PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 180)

  add_test(
    NAME m7-release-evidence-tarball-size-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m7_release_tarball_size.py
      --self-check
  )
  set_tests_properties(m7-release-evidence-tarball-size-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m5-golden-fingerprint-pins
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m5_golden_fingerprint_pins.py
      --verify
  )
  set_tests_properties(m5-golden-fingerprint-pins PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m5-golden-fingerprint-pins-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m5_golden_fingerprint_pins.py
      --self-check
  )
  set_tests_properties(m5-golden-fingerprint-pins-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m5-packet-provenance-audit
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m5_packet_provenance.py
      --verify
  )
  set_tests_properties(m5-packet-provenance-audit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m5-packet-provenance-audit-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m5_packet_provenance.py
      --self-check
  )
  set_tests_properties(m5-packet-provenance-audit-self-check PROPERTIES
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
    NAME m7-release-markdown-docs-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_release_markdown.py
      --self-check
  )
  set_tests_properties(m7-release-markdown-docs-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-doc-metadata-fields
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_release_doc_metadata_fields.py
      --verify
  )
  set_tests_properties(m7-release-doc-metadata-fields PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-doc-metadata-fields-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_release_doc_metadata_fields.py
      --self-check
  )
  set_tests_properties(m7-release-doc-metadata-fields-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-doc-file-counts
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_release_doc_file_counts.py
      --verify
  )
  set_tests_properties(m7-release-doc-file-counts PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-doc-file-counts-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_release_doc_file_counts.py
      --self-check
  )
  set_tests_properties(m7-release-doc-file-counts-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-tools-catalog-validation
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_release_tools_catalog.py
      --verify
  )
  set_tests_properties(m7-release-tools-catalog-validation PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-tools-catalog-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_release_tools_catalog.py
      --self-check
  )
  set_tests_properties(m7-release-tools-catalog-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-automatic-phasespace-blocker-docs
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_automatic_phasespace_blocker_docs.py
      --verify
  )
  set_tests_properties(m7-automatic-phasespace-blocker-docs PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-automatic-phasespace-blocker-docs-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_automatic_phasespace_blocker_docs.py
      --self-check
  )
  set_tests_properties(m7-automatic-phasespace-blocker-docs-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-amflow-live-rerun-doc-inventory
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_live_rerun_docs.py
      --verify
  )
  set_tests_properties(m7-amflow-live-rerun-doc-inventory PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-amflow-live-rerun-doc-inventory-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_live_rerun_docs.py
      --self-check
  )
  set_tests_properties(m7-amflow-live-rerun-doc-inventory-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-amflow-live-rerun-doc-structure
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_live_rerun_doc_structure.py
      --verify
  )
  set_tests_properties(m7-amflow-live-rerun-doc-structure PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-amflow-live-rerun-doc-structure-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_live_rerun_doc_structure.py
      --self-check
  )
  set_tests_properties(m7-amflow-live-rerun-doc-structure-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-amflow-live-rerun-freshness
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_live_rerun_freshness.py
      --verify
  )
  set_tests_properties(m7-amflow-live-rerun-freshness PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-amflow-live-rerun-freshness-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/validate_amflow_live_rerun_freshness.py
      --self-check
  )
  set_tests_properties(m7-amflow-live-rerun-freshness-self-check PROPERTIES
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

  add_test(
    NAME m7-release-sidecar-inventory-root-guard
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_m7_sidecar_inventory.py
      --self-check
  )
  set_tests_properties(m7-release-sidecar-inventory-root-guard PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME reference-sidecar-schema-version-gate
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_reference_sidecar_schema_versions.py
  )
  set_tests_properties(reference-sidecar-schema-version-gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-unaccepted-sidecar-review-queue
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/list_m7_unaccepted_sidecars.py
      --verify
      --summary-only
  )
  set_tests_properties(m7-release-unaccepted-sidecar-review-queue PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-unaccepted-sidecar-review-queue-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/list_m7_unaccepted_sidecars.py
      --self-check
  )
  set_tests_properties(m7-release-unaccepted-sidecar-review-queue-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-readiness-sidecar-references
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m7_release_readiness_sidecar_references.py
      --self-check
  )
  set_tests_properties(m7-release-readiness-sidecar-references PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m5-m6-m7-cross-link-closure
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_m5_m6_m7_cross_links.py
      --self-check
  )
  set_tests_properties(m5-m6-m7-cross-link-closure PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/release_health_summary.py
      --verify
  )
  set_tests_properties(m7-release-health-summary PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-source-sidecar-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/release_health_summary.py
      --self-check
  )
  set_tests_properties(m7-release-health-source-sidecar-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary-json
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/release_health_summary.py
      --verify
      --format
      json
  )
  set_tests_properties(m7-release-health-summary-json PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary-json-schema
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_health_json_schema.py
  )
  set_tests_properties(m7-release-health-summary-json-schema PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary-json-schema-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_health_json_schema.py
      --self-check
  )
  set_tests_properties(m7-release-health-summary-json-schema-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary-json-fixture
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py
  )
  set_tests_properties(m7-release-health-summary-json-fixture PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary-json-fixture-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py
      --self-check
  )
  set_tests_properties(m7-release-health-summary-json-fixture-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-summary-text-fixture
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_health_text_fixture.py
  )
  set_tests_properties(m7-release-health-summary-text-fixture PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-status-badge
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/release_status_badge.py
      --verify
  )
  set_tests_properties(m7-release-status-badge PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-status-badge-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/release_status_badge.py
      --self-check
  )
  set_tests_properties(m7-release-status-badge-self-check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-status-badge-fixture
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_status_badge_fixture.py
  )
  set_tests_properties(m7-release-status-badge-fixture PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_test(
    NAME m7-release-health-output-consistency
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_m7_release_health_outputs_consistent.py
  )
  set_tests_properties(m7-release-health-output-consistency PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(singular-runtime-lane-tests tests/singular_runtime_lane_tests.cpp)
  target_link_libraries(singular-runtime-lane-tests PRIVATE amflow)
  target_compile_features(singular-runtime-lane-tests PRIVATE cxx_std_17)
  amflow_apply_default_warnings(singular-runtime-lane-tests)
  add_test(NAME singular-runtime-lane-tests COMMAND singular-runtime-lane-tests)
  set_tests_properties(singular-runtime-lane-tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 900)
  add_test(
    NAME b61n-publication-audit-trail-query-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_b61n_publication_audit_trail.py
      --self-check
      --test-binary
      $<TARGET_FILE:singular-runtime-lane-tests>)
  set_tests_properties(b61n-publication-audit-trail-query-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 900)
  add_test(
    NAME b61n-publication-audit-fingerprint-regenerator-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/regenerate_b61n_publication_audit_fingerprints.py
      --self-check
      --test-binary
      $<TARGET_FILE:singular-runtime-lane-tests>)
  set_tests_properties(b61n-publication-audit-fingerprint-regenerator-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 900)
  add_test(
    NAME b61n-parity-status-summary
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/b61n_parity_status_summary.py
      --format
      json
      --include-runtime-audit
      --test-binary
      $<TARGET_FILE:singular-runtime-lane-tests>)
  set_tests_properties(b61n-parity-status-summary
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 900)
  add_test(
    NAME b61n-parity-status-summary-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/b61n_parity_status_summary.py
      --self-check)
  set_tests_properties(b61n-parity-status-summary-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME b61n-parity-status-summary-json-fixture
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_b61n_parity_status_json_fixture.py)
  set_tests_properties(b61n-parity-status-summary-json-fixture
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME b61n-parity-status-summary-json-fixture-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_b61n_parity_status_json_fixture.py
      --self-check)
  set_tests_properties(b61n-parity-status-summary-json-fixture-self-check
    PROPERTIES
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
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 600)
  add_test(
    NAME b63n-d7-numeric-point-binding-sweep
    COMMAND
      cutkosky-weighted-residue-tests
      --check-b63n-d7-numeric-point-binding-sweep)
  set_tests_properties(b63n-d7-numeric-point-binding-sweep PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 300)
  add_test(
    NAME b63n-d7-numeric-binding-format-invariance
    COMMAND
      cutkosky-weighted-residue-tests
      --check-b63n-d7-numeric-binding-format-invariance)
  set_tests_properties(b63n-d7-numeric-binding-format-invariance PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 300)
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
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 600)
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
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 600)
  add_test(
    NAME b63n-parity-status-summary
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/b63n_parity_status_summary.py
      --format
      json
      --include-runtime-audit
      --test-binary
      $<TARGET_FILE:cutkosky-weighted-residue-tests>)
  set_tests_properties(b63n-parity-status-summary
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 600)
  add_test(
    NAME b63n-parity-status-summary-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/b63n_parity_status_summary.py
      --self-check)
  set_tests_properties(b63n-parity-status-summary-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME b63n-parity-status-summary-json-fixture
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_b63n_parity_status_json_fixture.py)
  set_tests_properties(b63n-parity-status-summary-json-fixture
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME b63n-parity-status-summary-json-fixture-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/assert_b63n_parity_status_json_fixture.py
      --self-check)
  set_tests_properties(b63n-parity-status-summary-json-fixture-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME parity-status-sidecar-audit
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/audit_parity_status_sidecars.py)
  set_tests_properties(parity-status-sidecar-audit
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  get_property(AMFLOW_FIXTURE_TREE_GUARDED_TESTS DIRECTORY PROPERTY TESTS)
  set(
    AMFLOW_FIXTURE_TREE_STATE
    ${CMAKE_CURRENT_BINARY_DIR}/fixture-tree-immutability/state.json)
  add_test(
    NAME fixture-tree-immutability-setup
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_fixture_tree_immutability.py
      --snapshot
      --root
      ${CMAKE_CURRENT_SOURCE_DIR}
      --state
      ${AMFLOW_FIXTURE_TREE_STATE}
      --path
      tests
      --path
      specs
      --path
      tools/reference-harness/specs
      --path
      tools/reference-harness/templates
      --path
      references/snapshots)
  set_tests_properties(fixture-tree-immutability-setup
    PROPERTIES
      FIXTURES_SETUP amflow-fixture-tree-immutability
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 180)
  add_test(
    NAME fixture-tree-immutability
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_fixture_tree_immutability.py
      --verify
      --root
      ${CMAKE_CURRENT_SOURCE_DIR}
      --state
      ${AMFLOW_FIXTURE_TREE_STATE})
  set_tests_properties(fixture-tree-immutability
    PROPERTIES
      DEPENDS fixture-tree-immutability-setup
      FIXTURES_CLEANUP amflow-fixture-tree-immutability
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 180)
  add_test(
    NAME fixture-tree-immutability-self-check
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_fixture_tree_immutability.py
      --self-check)
  set_tests_properties(fixture-tree-immutability-self-check
    PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  if(AMFLOW_FIXTURE_TREE_GUARDED_TESTS)
    set_property(
      TEST ${AMFLOW_FIXTURE_TREE_GUARDED_TESTS}
      APPEND
      PROPERTY FIXTURES_REQUIRED amflow-fixture-tree-immutability)
  endif()

  if(AMFLOW_ENABLE_CTEST_TIME_BASELINE_GATE)
    set(
      AMFLOW_CTEST_TIME_BASELINE
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/specs/release/ctest-time-baseline.json)
    if(NOT EXISTS ${AMFLOW_CTEST_TIME_BASELINE})
      message(FATAL_ERROR "missing CTest execution-time baseline: ${AMFLOW_CTEST_TIME_BASELINE}")
    endif()
    file(READ ${AMFLOW_CTEST_TIME_BASELINE} AMFLOW_CTEST_TIME_BASELINE_JSON)
    string(JSON AMFLOW_CTEST_TIME_TOOL GET "${AMFLOW_CTEST_TIME_BASELINE_JSON}" tool)
    if(NOT AMFLOW_CTEST_TIME_TOOL STREQUAL "ctest-execution-time-baseline")
      message(FATAL_ERROR "CTest execution-time baseline has unexpected tool field")
    endif()
    string(JSON AMFLOW_CTEST_TIME_COUNT LENGTH "${AMFLOW_CTEST_TIME_BASELINE_JSON}" tests)
    if(AMFLOW_CTEST_TIME_COUNT GREATER 0)
      math(EXPR AMFLOW_CTEST_TIME_LAST_INDEX "${AMFLOW_CTEST_TIME_COUNT} - 1")
      foreach(AMFLOW_CTEST_TIME_INDEX RANGE 0 ${AMFLOW_CTEST_TIME_LAST_INDEX})
        string(
          JSON AMFLOW_CTEST_TIME_TEST_NAME
          GET "${AMFLOW_CTEST_TIME_BASELINE_JSON}" tests ${AMFLOW_CTEST_TIME_INDEX} name)
        string(
          JSON AMFLOW_CTEST_TIME_MAX_SECONDS
          GET "${AMFLOW_CTEST_TIME_BASELINE_JSON}" tests ${AMFLOW_CTEST_TIME_INDEX} max_seconds)
        if(TEST "${AMFLOW_CTEST_TIME_TEST_NAME}")
          set_property(
            TEST
            "${AMFLOW_CTEST_TIME_TEST_NAME}"
            PROPERTY TIMEOUT "${AMFLOW_CTEST_TIME_MAX_SECONDS}")
        endif()
      endforeach()
    endif()

    add_test(
      NAME ctest-execution-time-baseline
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_ctest_time_baseline.py
        --baseline
        ${AMFLOW_CTEST_TIME_BASELINE}
        --build-dir
        ${CMAKE_CURRENT_BINARY_DIR}
        --ctest
        ${CMAKE_CTEST_COMMAND})
    set_tests_properties(ctest-execution-time-baseline PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(
      NAME ctest-execution-time-baseline-self-check
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/verify_ctest_time_baseline.py
        --self-check)
    set_tests_properties(ctest-execution-time-baseline-self-check PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()

  add_dependencies(
    amflow-tests
    amflow-cli
    singular-runtime-lane-tests
    cutkosky-weighted-residue-tests)
endif()
