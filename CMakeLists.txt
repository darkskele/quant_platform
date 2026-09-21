cmake_minimum_required(VERSION 3.19)

# Auto-detect via VCPKG_ROOT, set before project() since that's what triggers 
# toolchain detection.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(quant_platform CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Feeds .vscode/c_cpp_properties.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra)

# An active conda env puts its prefix on find_package's search path.
if(DEFINED ENV{CONDA_PREFIX} AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.23)
    list(APPEND CMAKE_IGNORE_PREFIX_PATH "$ENV{CONDA_PREFIX}")
endif()

# Transparent compiler-output caching for fast incremental rebuilds.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

include(cmake/utils.cmake)                      # qp_dir() — cmake/<mod> -> src|include|tests|benchmarks/<mod>

include(CTest)                                  # BUILD_TESTING, default ON
option(QP_BUILD_BENCHMARKS "Build Google Benchmark suites" ON)

if(QP_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    FetchContent_MakeAvailable(googlebenchmark)
endif()

option(QP_BUILD_PYTHON "Build the Python bindings" OFF)
if(QP_BUILD_PYTHON)
    # Use the modern FindPython instead of pybind11's deprecated
    # FindPythonLibs path, which trips CMP0148.
    set(PYBIND11_FINDPYTHON ON)

    # FindPython reads Python_EXECUTABLE. Without this forward, a
    # -DPYTHON_EXECUTABLE is accepted and then ignored, and the module silently
    # builds against whichever interpreter is first on PATH.
    if(DEFINED PYTHON_EXECUTABLE AND NOT DEFINED Python_EXECUTABLE)
        set(Python_EXECUTABLE "${PYTHON_EXECUTABLE}" CACHE FILEPATH "" FORCE)
    endif()

    find_package(pybind11 CONFIG REQUIRED)
    message(STATUS "Python bindings target ${Python_EXECUTABLE} (${Python_VERSION})")
endif()

add_subdirectory(cmake/core)
add_subdirectory(cmake/execution)
add_subdirectory(cmake/strategy)
add_subdirectory(cmake/risk)
add_subdirectory(cmake/data_source/sink)
add_subdirectory(cmake/data_source/source)
add_subdirectory(cmake/data_source/network/http)
add_subdirectory(cmake/data_source/archive)
add_subdirectory(cmake/data_source/source/exchange/binance/binance_historical)


add_subdirectory(cmake/data_source)
add_subdirectory(cmake/engine)     
add_subdirectory(cmake/apps/backtest) 

# After data_source, whose targets it links.
add_subdirectory(cmake/integration)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    # Every test_*.cpp under tests/. CONFIGURE_DEPENDS re-globs at build
    # time, so a new file needs no edit here.
    file(GLOB_RECURSE QP_TEST_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/test_*.cpp)
    # Anything hitting the network runs for minutes, so it lives in the
    # integration binary rather than the fast suite.
    list(FILTER QP_TEST_SRCS EXCLUDE REGEX "/integration/")
    list(FILTER QP_TEST_SRCS EXCLUDE REGEX "_integration\\.cpp$")
    # The python tests need an embedded interpreter, so they only join when the
    # bindings are built.
    if(NOT QP_BUILD_PYTHON)
        list(FILTER QP_TEST_SRCS EXCLUDE REGEX "/python/")
    endif()
    add_executable(qp_tests ${QP_TEST_SRCS})
    target_link_libraries(qp_tests PRIVATE
        qp_core qp_core_test_support qp_execution qp_execution_sim
        qp_execution_sim_matcher_last_trade
        qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
        qp_source_pure qp_sink qp_sink_fanout qp_data_source
        qp_engine qp_transport qp_transport_backtest_in_process qp_strategy qp_strategy_test_support
        qp_strategy_carry qp_risk
        qp_risk_test_support qp_risk_basic qp_engine_test_support
        qp_backtest_compose Threads::Threads GTest::gtest GTest::gtest_main
    )
    add_test(NAME qp_tests COMMAND qp_tests)

    if(QP_BUILD_PYTHON)
        # One interpreter and one set of test types for every python test in
        # the binary. pybind refuses a type registered twice in one process.
        target_sources(qp_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/support/python_test_types.cpp)
        target_include_directories(qp_tests PRIVATE ${CMAKE_SOURCE_DIR}/include/apps/backtest)
        target_link_libraries(qp_tests PRIVATE qp_risk_python qp_strategy_python pybind11::embed)
        # pybind's types are hidden, so anything holding a py::object has to be
        # too, or every such class warns.
        set_target_properties(qp_tests PROPERTIES CXX_VISIBILITY_PRESET hidden)
        # The interpreter leaks by design at exit, which LeakSanitizer reports.
        set_tests_properties(qp_tests PROPERTIES ENVIRONMENT
            "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tools/sanitizers/lsan_python.supp")
    endif()
endif()

if(QP_BUILD_BENCHMARKS)
    # One binary, every benchmark case, one results table. bench_*.cpp plus
    # the qp_bench_main.cpp entry point, globbed recursively. CONFIGURE_DEPENDS
    # re-globs at build time, so a new file needs no edit here.
    file(GLOB_RECURSE QP_BENCH_SRCS CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/benchmarks/bench_*.cpp
        ${CMAKE_SOURCE_DIR}/benchmarks/qp_bench_main.cpp)
    # apps/ benches need per-consumer QP_BACKTEST_* defines, so they build as
    # their own targets, not in this aggregate.
    list(FILTER QP_BENCH_SRCS EXCLUDE REGEX "/apps/")
    add_executable(qp_bench ${QP_BENCH_SRCS})
    target_link_libraries(qp_bench PRIVATE
        qp_core qp_core_test_support qp_execution qp_execution_sim
        qp_execution_sim_matcher_last_trade
        qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
        qp_source_pure qp_binance_historical
        qp_sink qp_sink_fanout qp_data_source qp_engine qp_transport qp_transport_backtest_in_process
        qp_strategy_test_support
        qp_risk_test_support qp_engine_test_support qp_strategy_carry qp_risk qp_risk_basic
        benchmark::benchmark
    )
    target_compile_definitions(qp_bench PRIVATE QP_REPO_ROOT="${CMAKE_SOURCE_DIR}")
endif()

if(QP_BUILD_PYTHON)
    pybind11_add_module(qp_binance_funding_carry src/apps/backtest/funding_carry/bindings.cpp)
    target_link_libraries(qp_binance_funding_carry PRIVATE qp_backtest_compose)
endif()
