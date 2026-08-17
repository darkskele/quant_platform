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
# else in the tree. No shared test/benchmark-registration macro yet: a
# helper for this pattern is premature until a third *top-level* lib repeats
# it verbatim (seams first, generality later) — libs/data_source/source now
# add_subdirectory's its remaining village (venue)
# internally, see its CMakeLists.txt.
add_subdirectory(libs/core)
add_subdirectory(libs/data_source/source)  # degrades gracefully without Boost — see its CMakeLists.txt
add_subdirectory(libs/data_source/sink)
add_subdirectory(apps/collector)  # degrades gracefully without Boost — see its CMakeLists.txt

if(QP_BUILD_BENCHMARKS)
    # One binary, every benchmark case, one results table. Per-lib *_bench
    # targets (above, in each lib's own CMakeLists.txt) stay for targeted
    # builds — this just also links them into one executable; Google
    # Benchmark's own --benchmark_filter narrows a single run when needed.
    add_executable(qp_bench
        libs/core/benchmarks/bench_spsc_queue.cpp
        libs/data_source/source/benchmarks/bench_message_processing.cpp
        libs/data_source/source/venue/benchmarks/bench_binance_parser.cpp
        libs/data_source/sink/benchmarks/bench_wire.cpp
    )
    target_link_libraries(qp_bench PRIVATE
        qp_core qp_source_pure qp_venue qp_sink_pure benchmark::benchmark_main
    )
endif()
