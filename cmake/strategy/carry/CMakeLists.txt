# carry — funding-rate/basis carry Strategy
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_strategy_carry INTERFACE)
target_include_directories(qp_strategy_carry INTERFACE ${QP_INC})
target_link_libraries(qp_strategy_carry INTERFACE qp_strategy)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_carry_tests ${QP_TESTS}/test_funding_carry_strategy.cpp)
    target_link_libraries(qp_carry_tests PRIVATE qp_strategy_carry qp_core_test_support GTest::gtest
                                                  GTest::gtest_main)
    qp_register_test(qp_carry_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_carry_bench ${QP_BENCH}/bench_funding_carry_strategy.cpp)
    target_link_libraries(qp_carry_bench PRIVATE qp_strategy_carry qp_core_test_support
                                                  benchmark::benchmark_main)
endif()
