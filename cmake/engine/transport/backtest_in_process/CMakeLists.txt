# engine/transport/backtest_in_process
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_transport_backtest_in_process INTERFACE)
target_include_directories(qp_transport_backtest_in_process INTERFACE ${QP_INC})
target_link_libraries(qp_transport_backtest_in_process INTERFACE qp_transport)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_transport_backtest_in_process_tests ${QP_TESTS}/test_backtest_in_process_transport.cpp)
    target_link_libraries(qp_transport_backtest_in_process_tests PRIVATE
        qp_transport_backtest_in_process GTest::gtest GTest::gtest_main)
    qp_register_test(qp_transport_backtest_in_process_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_transport_backtest_in_process_bench ${QP_BENCH}/bench_backtest_in_process_transport.cpp)
    target_link_libraries(qp_transport_backtest_in_process_bench PRIVATE
        qp_transport_backtest_in_process benchmark::benchmark_main)
endif()
