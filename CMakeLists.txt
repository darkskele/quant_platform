cmake_minimum_required(VERSION 3.19)
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

# Each lib is its own CMake target (qp_core, qp_venue, ...) plus, where it has
# them, its own *_tests / *_bench targets — `cmake --build build --target
# qp_venue_tests` builds exactly that and its deps, nothing else in the tree.
# No shared test/benchmark-registration macro yet: venue is the first lib to
# need one, and a helper for a pattern used once is premature — add it when a
# third lib repeats this (seams first, generality later).
add_subdirectory(libs/core)
add_subdirectory(libs/venue)
add_subdirectory(libs/marketdata)  # degrades gracefully without Boost — see its CMakeLists.txt

# Blocked on vcpkg bringing in Boost.Beast/OpenSSL:
#   add_subdirectory(libs/record)
#   add_subdirectory(apps/collector)
