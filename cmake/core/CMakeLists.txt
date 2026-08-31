# core.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${QP_INC})
target_compile_features(qp_core INTERFACE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(qp_core INTERFACE Threads::Threads)

# Test/benchmark-only support.
add_library(qp_core_test_support INTERFACE)
target_include_directories(qp_core_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(qp_core_tests
        ${QP_TESTS}/test_spsc_queue.cpp
        ${QP_TESTS}/test_spmc_ring.cpp
        ${QP_TESTS}/test_portfolio.cpp
        ${QP_TESTS}/test_control_channel.cpp
        ${QP_TESTS}/test_viewable_pool.cpp
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
        ${QP_BENCH}/bench_control_channel.cpp
        ${QP_BENCH}/bench_portfolio.cpp
        ${QP_BENCH}/bench_viewable_pool.cpp
    )
    target_link_libraries(qp_core_bench PRIVATE qp_core benchmark::benchmark_main)
endif()
