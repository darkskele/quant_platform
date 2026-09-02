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

add_custom_target(qp_all_tests)
function(qp_register_test name)
    add_test(NAME ${name} COMMAND ${name})
    add_dependencies(qp_all_tests ${name})
endfunction()

add_subdirectory(cmake/core)
add_subdirectory(cmake/clock)              
add_subdirectory(cmake/execution)          
add_subdirectory(cmake/strategy) 
add_subdirectory(cmake/risk)             
add_subdirectory(cmake/data_source/sink)
add_subdirectory(cmake/data_source/source) 


add_subdirectory(cmake/data_source)
add_subdirectory(cmake/engine)     
add_subdirectory(cmake/apps/backtest) 

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    # Every test_*.cpp under tests/. CONFIGURE_DEPENDS re-globs at build
    # time, so a new file needs no edit here.
    file(GLOB_RECURSE QP_TEST_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/test_*.cpp)
    add_executable(qp_tests ${QP_TEST_SRCS})
    target_link_libraries(qp_tests PRIVATE
        qp_core qp_core_test_support qp_clock qp_execution qp_execution_sim
        qp_execution_sim_matcher_last_trade qp_csv_source qp_sink qp_sink_fanout qp_data_source
        qp_engine qp_transport qp_transport_backtest_in_process qp_strategy qp_strategy_test_support
        qp_strategy_carry qp_risk
        qp_risk_test_support qp_risk_basic qp_engine_test_support qp_source_venue_binance_historical
        qp_backtest_compose Threads::Threads GTest::gtest GTest::gtest_main
    )
    target_compile_definitions(qp_tests PRIVATE
        QP_BACKTEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/apps/backtest/fixtures"
        QP_BACKTEST_SYMBOL="BTCUSDT"
        QP_BACKTEST_FIRST_DAY="2024-01-01"
        QP_BACKTEST_LAST_DAY="2024-01-07"
    )
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
        qp_core qp_core_test_support qp_clock qp_execution qp_execution_sim
        qp_execution_sim_matcher_last_trade qp_source_pure qp_csv_source
        qp_sink qp_sink_fanout qp_data_source qp_engine qp_transport qp_transport_backtest_in_process
        qp_strategy_test_support
        qp_risk_test_support qp_engine_test_support qp_strategy_carry qp_risk qp_risk_basic
        qp_source_venue_binance_historical
        benchmark::benchmark
    )
    target_compile_definitions(qp_bench PRIVATE QP_REPO_ROOT="${CMAKE_SOURCE_DIR}")
endif()

# Python bindings for the funding-carry backtest. Off by default; needs
# python3 dev headers and pybind11.
option(QP_BUILD_PYTHON "Build the Python bindings" OFF)
if(QP_BUILD_PYTHON)
    # Use the modern FindPython instead of pybind11's deprecated
    # FindPythonLibs path, which trips CMP0148.
    set(PYBIND11_FINDPYTHON ON)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(qp_backtest src/apps/backtest/funding_carry/bindings.cpp)
    target_link_libraries(qp_backtest PRIVATE qp_backtest_compose)
endif()
