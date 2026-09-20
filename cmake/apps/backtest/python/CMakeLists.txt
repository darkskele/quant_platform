qp_dir(include QP_INC)
get_filename_component(QP_BACKTEST_INC "${QP_INC}" DIRECTORY)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    # tests/python/ holds everything that exercises the python surface, so it
    # sits outside the tests/<module> mirror and is named in full.
    add_executable(qp_python_backtest_tests
        ${CMAKE_SOURCE_DIR}/tests/python/test_python_backtest.cpp
    )
    target_include_directories(qp_python_backtest_tests PRIVATE ${QP_BACKTEST_INC})
    target_link_libraries(qp_python_backtest_tests PRIVATE
        qp_engine qp_transport qp_transport_backtest_in_process qp_execution_sim
        qp_execution_sim_matcher_last_trade
        qp_data_source qp_sink_fanout qp_risk_python qp_strategy_python
        pybind11::embed Threads::Threads GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_python_backtest_tests)
endif()

pybind11_add_module(qp_python_backtest ${CMAKE_SOURCE_DIR}/src/apps/backtest/python/bindings.cpp)
target_include_directories(qp_python_backtest PRIVATE ${QP_BACKTEST_INC})
target_link_libraries(qp_python_backtest PRIVATE
    qp_engine qp_transport qp_transport_backtest_in_process qp_execution_sim
    qp_execution_sim_matcher_last_trade
    qp_data_source qp_sink_fanout qp_risk_python qp_strategy_python
    qp_binance_historical
)
