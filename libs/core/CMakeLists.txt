# core — the lingua franca. Header-only. Depends on nothing but the
# standard library + pthread (round_robin_pool.hpp uses std::thread —
# production code, not test-only, so this is unconditional, not gated by
# BUILD_TESTING like the block below).
add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(qp_core INTERFACE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(qp_core INTERFACE Threads::Threads)

# Test/benchmark-only support (ScratchDir): every real-file-I/O test or
# benchmark in the repo needs a unique, self-cleaning temp directory —
# extracted here once duplicated a third time. Unconditional (not gated by
# BUILD_TESTING): QP_BUILD_BENCHMARKS-only consumers need it too, and an
# unused INTERFACE library costs nothing. include dir is tests/ itself (not
# tests/support/), same convention as qp_source_test_support, so
# #include "support/scratch_dir.hpp" resolves the same way from any consumer.
add_library(qp_core_test_support INTERFACE)
target_include_directories(qp_core_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(qp_core_tests tests/test_spsc_queue.cpp tests/test_spmc_ring.cpp tests/test_portfolio.cpp
                                  tests/test_round_robin_pool.cpp)
    target_link_libraries(qp_core_tests PRIVATE qp_core qp_core_test_support Threads::Threads GTest::gtest
                                                 GTest::gtest_main)
    qp_register_test(qp_core_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # googlebenchmark is fetched once, at the top level.
    add_executable(qp_core_bench benchmarks/bench_spsc_queue.cpp benchmarks/bench_spmc_ring.cpp
                                  benchmarks/bench_round_robin_pool.cpp)
    target_link_libraries(qp_core_bench PRIVATE qp_core benchmark::benchmark_main)
endif()
