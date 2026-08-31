cmake_minimum_required(VERSION 3.19)

# vcpkg manifest mode (vcpkg.json): auto-detect via VCPKG_ROOT, set before
# project() since that's what triggers toolchain detection. No-op — same
# local apt/conda resolution as today — until VCPKG_ROOT is exported; once
# it is, the same find_package() calls resolve through vcpkg instead,
# unchanged.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(quant_platform CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Feeds .vscode/c_cpp_properties.json: IntelliSense reads the *actual* flags
# (standard, defines, include paths) per file from here instead of a
# hand-maintained, driftable copy — fixes both "unrecognized C++20 syntax"
# false errors and "can't find simdjson.h/boost" false errors in one go,
# since those live outside default include paths.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra)

# Transparent compiler-output caching for fast incremental rebuilds. Opt-in
# by presence only — no-op if ccache isn't installed, never a hard dependency.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

include(cmake/utils.cmake)                      # qp_dir() — cmake/<mod> -> src|include|tests|benchmarks/<mod>

include(CTest)                                  # BUILD_TESTING, default ON
option(QP_BUILD_BENCHMARKS "Build Google Benchmark suites" ON)

if(QP_BUILD_BENCHMARKS)
    # Built from source, not found on the system: apt's libbenchmark-dev
    # ships a .so compiled without NDEBUG ("Library was built as DEBUG"),
    # a property baked into that .so regardless of our own flags. A
    # FetchContent build inherits this configure's CMAKE_BUILD_TYPE, so
    # under the release preset it's a real -DNDEBUG build. Superseded once
    # vcpkg lands (its release triplet builds benchmark the same way).
    # Declared once here, not per-lib: every *_bench target links the same
    # benchmark::benchmark: no reason to re-fetch it per consumer.
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

# Each lib is its own CMake target (qp_core, qp_source, ...) plus, where
# it has them, its own *_tests / *_bench targets — `cmake --build build
# --target qp_source_tests` builds exactly that and its deps, nothing
# else in the tree. data_source/source now add_subdirectory's its
# remaining village (venue) internally, see its CMakeLists.txt.

# Aggregate target every *_tests binary registers into via
# qp_register_test() below, so "build everything testable" (.vscode/
# tasks.json's "Build: all tests", the /test skill's `all` mode) is one
# `--target qp_all_tests` instead of an enumerated list every new lib's
# CMakeLists.txt has to remember to also go add itself to elsewhere. Past
# the "third top-level lib repeats this verbatim" bar this file used to
# defer the pattern on.
add_custom_target(qp_all_tests)
function(qp_register_test name)
    add_test(NAME ${name} COMMAND ${name})
    add_dependencies(qp_all_tests ${name})
endfunction()

add_subdirectory(cmake/core)
add_subdirectory(cmake/clock)               # Clock concept + SimClock (WallClock deferred), depends on core only
add_subdirectory(cmake/execution)           # ExecutionGateway/Matcher concepts + SimExecution, depends on core only
add_subdirectory(cmake/strategy)            # Strategy concept, depends on core only
add_subdirectory(cmake/risk)                # RiskGate concept + RiskDecision, depends on core only
add_subdirectory(cmake/data_source/sink)
add_subdirectory(cmake/data_source/source)  # Source concept + CsvSource + parser (Parser concept, binance)
# run_data_source.hpp (D43) needs both source::Source and sink::Sink, so
# data_source (the parent, not its own "city") comes after both.
add_subdirectory(cmake/data_source)
add_subdirectory(cmake/engine)     # Engine<Tx,Clk,Exec,Risk,Strategy,Book> composition root; add_subdirectory's its own transport/ child
add_subdirectory(cmake/apps/backtest)   # each leg's Source -> FanoutSink -> BacktestInProcessTransport -> Engine (D48/D5x) — currently broken, mid-migration from FileReplaySource to CsvSource
add_subdirectory(integration)           # cross-lib integration/parity tests — see its CMakeLists.txt

if(QP_BUILD_BENCHMARKS)
    # One binary, every benchmark case, one results table, one launch — see
    # benchmarks/qp_bench_main.cpp: runs with the repo's standard
    # --benchmark_repetitions=5 --benchmark_report_aggregates_only=true
    # baked in as defaults (still overridable) and regenerates the
    # checked-in root BENCHMARKS.md every time it's run, no separate
    # tools/bench/bench_to_md.py step. Per-lib *_bench targets (above, in
    # each lib's own CMakeLists.txt) stay for targeted builds during dev —
    # this is the one binary that's actually meant to be run standalone;
    # Google Benchmark's own --benchmark_filter narrows a single run when
    # only a subset is wanted, still through this same binary.
    add_executable(qp_bench
        benchmarks/qp_bench_main.cpp
        benchmarks/clock/bench_sim_clock.cpp
        benchmarks/core/bench_spsc_queue.cpp
        benchmarks/core/bench_spmc_ring.cpp
        benchmarks/core/bench_mpsc_queue.cpp
        benchmarks/core/bench_control_channel.cpp
        benchmarks/core/bench_portfolio.cpp
        benchmarks/core/bench_viewable_pool.cpp
        benchmarks/data_source/source/bench_venue_types.cpp
        benchmarks/data_source/source/bench_csv_source.cpp
        benchmarks/data_source/source/venue/binance/binance_historical/bench_bin_hist_symbol_table.cpp
        benchmarks/data_source/source/venue/binance/binance_historical/bench_bin_hist_parser_policy.cpp
        benchmarks/engine/bench_engine.cpp
        benchmarks/engine/transport/bench_backtest_in_process_transport.cpp
        benchmarks/strategy/carry/bench_funding_carry_strategy.cpp
        benchmarks/risk/basic/bench_basic_risk_gate.cpp
        benchmarks/execution/sim/matcher/last_trade/bench_last_trade_matcher.cpp
        benchmarks/execution/sim/bench_sim_execution.cpp
        benchmarks/data_source/sink/fanout/bench_fanout_sink.cpp
        benchmarks/data_source/bench_run_data_source.cpp
    )
    target_link_libraries(qp_bench PRIVATE
        qp_core qp_core_test_support qp_clock qp_execution qp_execution_sim
        qp_execution_sim_matcher_last_trade qp_source_pure qp_csv_source
        qp_sink qp_sink_fanout qp_data_source qp_engine qp_transport qp_strategy_test_support
        qp_risk_test_support qp_engine_test_support qp_strategy_carry qp_risk qp_risk_basic
        qp_source_venue_binance_historical
        benchmark::benchmark
    )
    # benchmark::benchmark, not benchmark::benchmark_main: qp_bench_main.cpp
    # provides its own main() (defaults + BENCHMARKS.md regeneration), not
    # the stock one-liner benchmark_main.a supplies.
    target_compile_definitions(qp_bench PRIVATE QP_REPO_ROOT="${CMAKE_SOURCE_DIR}")
endif()
