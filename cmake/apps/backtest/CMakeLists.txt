# backtest — thin apps/ wiring: each leg's config::Source -> its own
# config::Sink -> BacktestInProcessTransport (merges both legs by
# MarketEvent::ts, coordinated via ControlChannel) -> config::EngineType
# (D48/D5x). Every actual component choice (Source+Parser, Sink, Matcher,
# RiskGate, Strategy) is a macro-selected combo resolved in
# include/apps/backtest/config/*.hpp, not named here — this file only picks
# *which* combo via the cache options below. Exactly one real combo exists
# today; add a branch in the matching config/*.hpp header (never here) when
# a second one does.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

set(QP_BACKTEST_SOURCE "csv_binhist" CACHE STRING "Source+Parser combo (see config/source_config.hpp)")
set(QP_BACKTEST_SINK "fanout" CACHE STRING "Sink combo (see config/sink_config.hpp)")
set(QP_BACKTEST_MATCHER "last_trade" CACHE STRING "Matcher combo (see config/engine_config.hpp)")
set(QP_BACKTEST_RISK "basic" CACHE STRING "RiskGate combo (see config/engine_config.hpp)")
set(QP_BACKTEST_STRATEGY "funding_carry" CACHE STRING "Strategy combo (see config/engine_config.hpp)")

# What this binary trades — build-time, not a runtime flag (D5x): see
# config/compose.hpp's own comment for why. tools/fetch_backtest_data.sh
# writes to QP_BACKTEST_DATA_DIR in the layout config/data_layout.hpp reads.
set(QP_BACKTEST_DATA_DIR "data/binance_historical" CACHE STRING "Root dir tools/fetch_backtest_data.sh wrote to")
set(QP_BACKTEST_SYMBOL "BTCUSDT" CACHE STRING "Must be one of config::FuturesTable's compile-time symbols")
set(QP_BACKTEST_FIRST_DAY "2024-01-01" CACHE STRING "Inclusive UTC start date, YYYY-MM-DD")
set(QP_BACKTEST_LAST_DAY "2024-01-01" CACHE STRING "Inclusive UTC end date, YYYY-MM-DD")

string(TOUPPER "${QP_BACKTEST_SOURCE}" _qp_bt_source)
string(TOUPPER "${QP_BACKTEST_SINK}" _qp_bt_sink)
string(TOUPPER "${QP_BACKTEST_MATCHER}" _qp_bt_matcher)
string(TOUPPER "${QP_BACKTEST_RISK}" _qp_bt_risk)
string(TOUPPER "${QP_BACKTEST_STRATEGY}" _qp_bt_strategy)

add_library(qp_backtest_core ${QP_SRC}/backtest.cpp)
target_include_directories(qp_backtest_core PUBLIC ${QP_INC})
target_compile_definitions(qp_backtest_core PUBLIC
    QP_SOURCE_${_qp_bt_source}
    QP_SINK_${_qp_bt_sink}
    QP_MATCHER_${_qp_bt_matcher}
    QP_RISK_${_qp_bt_risk}
    QP_STRATEGY_${_qp_bt_strategy}
    QP_BACKTEST_DATA_DIR="${QP_BACKTEST_DATA_DIR}"
    QP_BACKTEST_SYMBOL="${QP_BACKTEST_SYMBOL}"
    QP_BACKTEST_FIRST_DAY="${QP_BACKTEST_FIRST_DAY}"
    QP_BACKTEST_LAST_DAY="${QP_BACKTEST_LAST_DAY}"
)
target_link_libraries(qp_backtest_core PUBLIC qp_csv_source qp_source_venue_binance_historical
                                               qp_sink_fanout qp_transport qp_engine qp_execution_sim
                                               qp_execution_sim_matcher_last_trade qp_risk_basic
                                               qp_strategy_carry)

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
