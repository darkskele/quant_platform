# backtest — thin apps/ wiring: each leg's Source -> its own FanoutSink ->
# BacktestInProcessTransport (merges both legs by MarketEvent::ts,
# coordinated via ControlChannel) -> Engine<..., BasicRiskGate<Portfolio>,
# FundingCarryStrategy<Portfolio>, Portfolio> (D48/D5x).
#
# CURRENTLY BROKEN: backtest.cpp still constructs the old FileReplaySource,
# deleted along with the rest of the wire/zstd lineage (see
# cmake/data_source/source/CMakeLists.txt) in favor of CsvSource. Needs
# rewiring to CsvSource<BinHistVenue, N> before this target compiles again
# — not done here since that's a real design pass (file-list construction,
# N per leg), not a mechanical rename.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

add_library(qp_backtest_core ${QP_SRC}/backtest.cpp)
target_include_directories(qp_backtest_core PUBLIC ${QP_INC})
target_link_libraries(qp_backtest_core PUBLIC qp_csv_source qp_sink_fanout qp_transport qp_engine
                                               qp_execution_sim qp_execution_sim_matcher_last_trade
                                               qp_risk_basic qp_strategy_carry)

add_executable(qp_backtest ${QP_SRC}/main.cpp)
target_link_libraries(qp_backtest PRIVATE qp_backtest_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_backtest_integration_tests ${QP_TESTS}/test_backtest_integration.cpp)
    target_link_libraries(qp_backtest_integration_tests
        PRIVATE qp_backtest_core qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_backtest_integration_tests)
endif()

# No qp_backtest_bench: apps/ is thin composition wiring, not its own
# thing to benchmark — every component it wires (FileReplaySource,
# FanoutSink, BacktestInProcessTransport, Engine, FundingCarryStrategy,
# BasicRiskGate) already has its own benchmark. A full-pipeline benchmark
# here would just be a slower, noisier re-measurement of the same
# components' costs bundled together (see docs/decisions.md D53).
