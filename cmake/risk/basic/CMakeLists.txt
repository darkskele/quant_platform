# basic — BasicRiskGate, the first concrete RiskGate (D46): per-(symbol,
# venue) exposure cap plus an equity-drawdown kill switch. Header-only,
# depends on qp_risk only.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_risk_basic INTERFACE)
target_include_directories(qp_risk_basic INTERFACE ${QP_INC})
target_link_libraries(qp_risk_basic INTERFACE qp_risk)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_risk_basic_tests ${QP_TESTS}/test_basic_risk_gate.cpp)
    target_link_libraries(qp_risk_basic_tests PRIVATE qp_risk_basic qp_core_test_support GTest::gtest
                                                       GTest::gtest_main)
    qp_register_test(qp_risk_basic_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_risk_basic_bench ${QP_BENCH}/bench_basic_risk_gate.cpp)
    target_link_libraries(qp_risk_basic_bench PRIVATE qp_risk_basic qp_core_test_support
                                                       benchmark::benchmark_main)
endif()
