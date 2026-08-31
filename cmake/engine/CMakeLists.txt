# engine — the Engine<Tx,Clk,Exec,Risk,Strategy,Book> composition root.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_subdirectory(transport)

add_library(qp_engine INTERFACE)
target_include_directories(qp_engine INTERFACE ${QP_INC})
target_link_libraries(qp_engine INTERFACE qp_core qp_clock qp_execution qp_strategy qp_risk qp_transport)

add_library(qp_engine_test_support INTERFACE)
target_include_directories(qp_engine_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_engine_tests ${QP_TESTS}/test_engine.cpp)
    target_link_libraries(qp_engine_tests PRIVATE qp_engine qp_execution_sim
                                                   qp_execution_sim_matcher_last_trade
                                                   qp_core_test_support qp_strategy_test_support
                                                   qp_risk_test_support qp_engine_test_support GTest::gtest
                                                   GTest::gtest_main)
    qp_register_test(qp_engine_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_engine_bench ${QP_BENCH}/bench_engine.cpp)
    target_link_libraries(qp_engine_bench PRIVATE qp_engine qp_execution_sim
                                                   qp_execution_sim_matcher_last_trade
                                                   qp_core_test_support qp_strategy_test_support
                                                   qp_risk_test_support qp_engine_test_support
                                                   benchmark::benchmark_main)
endif()
