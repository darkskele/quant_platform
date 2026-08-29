# engine — the Engine<Tx,Clk,Exec,Risk,NumWorkers,Strategies...> composition
# root (see docs/engine/DESIGN.md; docs/decisions.md D27, D33, D38).
# Header-only, depends on the seam concepts only — never a concrete adapter.
# qp_transport, not qp_source_pure: Engine is constrained on transport::Transport
# (D22/D38), not source::Source — it never needs venue-specific Source machinery.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_subdirectory(transport)  # Transport concept + CombinedTransport/BacktestInProcessTransport

add_library(qp_engine INTERFACE)
target_include_directories(qp_engine INTERFACE ${QP_INC})
target_link_libraries(qp_engine INTERFACE qp_core qp_clock qp_execution qp_strategy qp_risk qp_transport)

# InfiniteTransport — Engine-specific (never-exhausts Source double), shared
# between engine's own tests and benchmarks. Unconditional, same convention
# as qp_core_test_support. Include dir is the shared top-level tests/ root
# (D50), not this module's own tests/engine/.
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
