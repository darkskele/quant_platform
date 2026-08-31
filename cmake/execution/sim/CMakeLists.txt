# execution/sim 
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_subdirectory(matcher)

add_library(qp_execution_sim INTERFACE)
target_include_directories(qp_execution_sim INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim INTERFACE qp_execution qp_execution_sim_matcher)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_execution_sim_tests ${QP_TESTS}/test_sim_execution.cpp)
    target_link_libraries(qp_execution_sim_tests PRIVATE qp_execution_sim
                                                          qp_execution_sim_matcher_last_trade
                                                          qp_core_test_support GTest::gtest
                                                          GTest::gtest_main)
    qp_register_test(qp_execution_sim_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_execution_sim_bench ${QP_BENCH}/bench_sim_execution.cpp)
    target_link_libraries(qp_execution_sim_bench PRIVATE qp_execution_sim
                                                          qp_execution_sim_matcher_last_trade
                                                          benchmark::benchmark_main)
endif()
