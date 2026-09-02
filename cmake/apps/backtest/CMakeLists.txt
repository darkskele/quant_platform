# backtest.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

set(QP_BACKTEST_SOURCE "csv_binhist" CACHE STRING "Source+Parser combo (see config/source_config.hpp)")
set(QP_BACKTEST_SINK "fanout" CACHE STRING "Sink combo (see config/sink_config.hpp)")
set(QP_BACKTEST_MATCHER "last_trade" CACHE STRING "Matcher combo (see config/engine_config.hpp)")
set(QP_BACKTEST_RISK "basic" CACHE STRING "RiskGate combo (see config/engine_config.hpp)")
set(QP_BACKTEST_STRATEGY "funding_carry" CACHE STRING "Strategy combo (see config/engine_config.hpp)")

string(TOUPPER "${QP_BACKTEST_SOURCE}" _qp_bt_source)
string(TOUPPER "${QP_BACKTEST_SINK}" _qp_bt_sink)
string(TOUPPER "${QP_BACKTEST_MATCHER}" _qp_bt_matcher)
string(TOUPPER "${QP_BACKTEST_RISK}" _qp_bt_risk)
string(TOUPPER "${QP_BACKTEST_STRATEGY}" _qp_bt_strategy)

# The backtest is header-only (BacktestBase + the funding-carry variant),
# so there's no library or app binary to build — this INTERFACE target just
# carries the component-selection macros, include dir, and the composed
# component libs. Which data to read (QP_BACKTEST_DATA_DIR/SYMBOL/FIRST_DAY/
# LAST_DAY) is set per consumer target below. There is no production CLI
# entry point: a run is driven by constructing a backtest and calling run()
# (the pybind path), exercised here by the test and bench.
add_library(qp_backtest_compose INTERFACE)
target_include_directories(qp_backtest_compose INTERFACE ${QP_INC})
target_compile_definitions(qp_backtest_compose INTERFACE
    QP_SOURCE_${_qp_bt_source}
    QP_SINK_${_qp_bt_sink}
    QP_MATCHER_${_qp_bt_matcher}
    QP_RISK_${_qp_bt_risk}
    QP_STRATEGY_${_qp_bt_strategy}
)
target_link_libraries(qp_backtest_compose INTERFACE qp_data_source qp_csv_source
                                                    qp_source_venue_binance_historical
                                                    qp_sink_fanout qp_transport qp_engine
                                                    qp_execution_sim
                                                    qp_execution_sim_matcher_last_trade qp_risk_basic
                                                    qp_strategy_carry)

# Test + bench point the composition at the committed fixture dataset.
set(_qp_fixture_defs
    QP_BACKTEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/apps/backtest/fixtures"
    QP_BACKTEST_SYMBOL="BTCUSDT"
    QP_BACKTEST_FIRST_DAY="2024-01-01"
    QP_BACKTEST_LAST_DAY="2024-01-07"
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_backtest_pipeline_tests ${QP_TESTS}/test_backtest_pipeline.cpp)
    target_link_libraries(qp_backtest_pipeline_tests PRIVATE qp_backtest_compose GTest::gtest
                                                             GTest::gtest_main)
    target_compile_definitions(qp_backtest_pipeline_tests PRIVATE ${_qp_fixture_defs})
    qp_register_test(qp_backtest_pipeline_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_backtest_pipeline_bench ${QP_BENCH}/bench_backtest_pipeline.cpp)
    target_link_libraries(qp_backtest_pipeline_bench PRIVATE qp_backtest_compose
                                                            benchmark::benchmark_main)
    target_compile_definitions(qp_backtest_pipeline_bench PRIVATE ${_qp_fixture_defs})
endif()
