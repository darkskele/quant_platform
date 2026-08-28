# execution — the ExecutionGateway/Matcher concepts + SimExecution (see
# docs/core/DESIGN.md; LiveExecution deferred, docs/decisions.md D25).
# Header-only, depends on core only.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_execution INTERFACE)
target_include_directories(qp_execution INTERFACE ${QP_INC})
target_link_libraries(qp_execution INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_execution_tests ${QP_TESTS}/test_sim_execution.cpp)
    target_link_libraries(qp_execution_tests PRIVATE qp_execution qp_core_test_support GTest::gtest
                                                      GTest::gtest_main)
    qp_register_test(qp_execution_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_execution_bench
        ${QP_BENCH}/bench_last_trade_matcher.cpp
        ${QP_BENCH}/bench_sim_execution.cpp
    )
    target_link_libraries(qp_execution_bench PRIVATE qp_execution benchmark::benchmark_main)
endif()
