# backtest — thin apps/ wiring: FileReplaySource (futures+spot legs) ->
# CombinedTransport -> Engine<..., BasicRiskGate, 1, FundingCarryStrategy>
# (D48). Unlike apps/collector, needs no Boost/OpenSSL — FileReplaySource
# only depends on qp_file_replay (wire codec), no live transport.

add_library(qp_backtest_core backtest.cpp)
target_link_libraries(qp_backtest_core PUBLIC qp_file_replay qp_transport qp_engine qp_risk
                                               qp_strategy_carry)

add_executable(qp_backtest main.cpp)
target_link_libraries(qp_backtest PRIVATE qp_backtest_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_backtest_integration_tests tests/test_backtest_integration.cpp)
    target_link_libraries(qp_backtest_integration_tests
        PRIVATE qp_backtest_core qp_sink qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_backtest_integration_tests)
endif()
