cmake_minimum_required(VERSION 3.19)
project(quant_platform CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Each lib is its own CMake target (qp_core, qp_venue, ...) plus, where it has
# them, its own *_tests / *_bench targets — `cmake --build build --target
# qp_venue_tests` builds exactly that and its deps, nothing else in the tree.
# No shared test/benchmark-registration macro yet: venue is the first lib to
# need one, and a helper for a pattern used once is premature — add it when a
# third lib repeats this (seams first, generality later).
add_subdirectory(libs/core)
add_subdirectory(libs/venue)

# Blocked on vcpkg bringing in Boost.Beast/OpenSSL:
#   add_subdirectory(libs/marketdata)
#   add_subdirectory(libs/record)
#   add_subdirectory(apps/collector)
