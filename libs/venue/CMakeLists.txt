# venue — Binance-specific glue. Depends on core + simdjson.
find_package(simdjson CONFIG REQUIRED)

add_library(qp_venue src/binance.cpp)
target_include_directories(qp_venue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_venue PUBLIC qp_core PRIVATE simdjson::simdjson)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_venue_tests tests/test_binance_parser.cpp)
    target_link_libraries(qp_venue_tests PRIVATE qp_venue GTest::gtest GTest::gtest_main)
    add_test(NAME qp_venue_tests COMMAND qp_venue_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # Built from source, not found on the system: apt's libbenchmark-dev
    # ships a .so compiled without NDEBUG, so it prints "Library was built
    # as DEBUG" and adds harness overhead regardless of our own flags — a
    # property baked into that .so, not something our CMAKE_BUILD_TYPE can
    # reach. A FetchContent build inherits the parent configure's build
    # type, so under the release preset this is a real -DNDEBUG release
    # build, same as our own code. Superseded once vcpkg lands (its release
    # triplet builds benchmark the same way).
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

    add_executable(qp_venue_bench benchmarks/bench_binance_parser.cpp)
    target_link_libraries(qp_venue_bench PRIVATE qp_venue benchmark::benchmark)
endif()
