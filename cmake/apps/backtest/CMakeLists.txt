# backtest — thin apps/ wiring: each leg's FileReplaySource -> its own
# FanoutSink (driven by its own thread, exactly like collector's
# run_data_source) -> BacktestInProcessTransport (merges both legs by
# MarketEvent::ts, coordinated via ControlChannel) -> Engine<...,
# BasicRiskGate, 1, FundingCarryStrategy> (D48/D5x). Needs qp_sink now
# (FanoutSink lives there alongside FileRecorder) in addition to
# qp_file_replay; still no Boost/OpenSSL — FileReplaySource has no live
# transport.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

add_library(qp_backtest_core ${QP_SRC}/backtest.cpp)
target_include_directories(qp_backtest_core PUBLIC ${QP_INC})
target_link_libraries(qp_backtest_core PUBLIC qp_file_replay qp_sink qp_transport qp_engine
                                               qp_execution_sim qp_execution_sim_matcher_last_trade
                                               qp_risk_basic qp_strategy_carry)

add_executable(qp_backtest ${QP_SRC}/main.cpp)
target_link_libraries(qp_backtest PRIVATE qp_backtest_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_backtest_integration_tests ${QP_TESTS}/test_backtest_integration.cpp)
    target_link_libraries(qp_backtest_integration_tests
        PRIVATE qp_backtest_core qp_sink qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_backtest_integration_tests)
endif()

# No qp_backtest_bench: apps/ is thin composition wiring, not its own
# thing to benchmark — every component it wires (FileReplaySource,
# FanoutSink, BacktestInProcessTransport, Engine, FundingCarryStrategy,
# BasicRiskGate) already has its own benchmark. A full-pipeline benchmark
# here would just be a slower, noisier re-measurement of the same
# components' costs bundled together (see docs/decisions.md D53).
