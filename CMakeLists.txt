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
  src/de/eta_reduction_execution.cpp
  src/de/eta_reduction_preparation.cpp
  src/de/invariant_reduction_execution.cpp
  src/de/invariant_reduction_preparation.cpp
  src/de/reduction_assembly.cpp
  src/io/problem_spec_io.cpp
  src/io/sample_data.cpp
  src/kira/kira_backend.cpp
  src/kira/kira_insert_prefactors.cpp
  src/runtime/artifact_store.cpp
  src/runtime/auxiliary_family.cpp
  src/runtime/boundary_generation.cpp
  src/runtime/continuation_path.cpp
  src/runtime/eta_mode.cpp
  src/runtime/ending_scheme.cpp
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
endif()
