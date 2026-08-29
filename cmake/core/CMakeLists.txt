# core — the lingua franca. Header-only. Depends on nothing but the
# standard library + pthread (round_robin_pool.hpp uses std::thread —
# production code, not test-only, so this is unconditional, not gated by
# BUILD_TESTING like the block below).
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${QP_INC})
target_compile_features(qp_core INTERFACE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(qp_core INTERFACE Threads::Threads)

# Test/benchmark-only support (ScratchDir, and every other shared test
# double): every real-file-I/O test or benchmark in the repo needs a
# unique, self-cleaning temp directory — extracted here once duplicated a
# third time. Unconditional (not gated by BUILD_TESTING): QP_BUILD_BENCHMARKS-
# only consumers need it too, and an unused INTERFACE library costs nothing.
# include dir is the shared top-level tests/ root (D50 consolidated every
# module's test doubles into one tests/support/), not this module's own
# tests/core/, so #include "support/scratch_dir.hpp" resolves the same way
# from any consumer.
add_library(qp_core_test_support INTERFACE)
target_include_directories(qp_core_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(qp_core_tests
        ${QP_TESTS}/test_spsc_queue.cpp
        ${QP_TESTS}/test_spmc_ring.cpp
        ${QP_TESTS}/test_portfolio.cpp
        ${QP_TESTS}/test_round_robin_pool.cpp
        ${QP_TESTS}/test_control_channel.cpp
    )
    target_link_libraries(qp_core_tests PRIVATE qp_core qp_core_test_support Threads::Threads GTest::gtest
                                                 GTest::gtest_main)
    qp_register_test(qp_core_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # googlebenchmark is fetched once, at the top level.
    add_executable(qp_core_bench
        ${QP_BENCH}/bench_spsc_queue.cpp
        ${QP_BENCH}/bench_spmc_ring.cpp
        ${QP_BENCH}/bench_mpsc_queue.cpp
        ${QP_BENCH}/bench_round_robin_pool.cpp
        ${QP_BENCH}/bench_control_channel.cpp
        ${QP_BENCH}/bench_portfolio.cpp
        ${QP_BENCH}/bench_viewable_pool.cpp
    )
    target_link_libraries(qp_core_bench PRIVATE qp_core benchmark::benchmark_main)
endif()
