# python — PythonBacktest variant. Same compose shape as the outer backtest
# but with the python strategy/risk adapters selected via engine_config.hpp.
qp_dir(include QP_INC)

add_library(qp_backtest_python_compose INTERFACE)
target_include_directories(qp_backtest_python_compose INTERFACE ${QP_INC})
target_compile_definitions(qp_backtest_python_compose INTERFACE
    QP_SOURCE_CSV_BINHIST
    QP_SINK_FANOUT
    QP_MATCHER_LAST_TRADE
    QP_RISK_PYTHON
    QP_STRATEGY_PYTHON
)
target_link_libraries(qp_backtest_python_compose INTERFACE qp_data_source qp_csv_source
                                                            qp_source_venue_binance_historical
                                                            qp_sink_fanout qp_transport
                                                            qp_transport_backtest_in_process qp_engine
                                                            qp_execution_sim
                                                            qp_execution_sim_matcher_last_trade
                                                            qp_risk_python qp_strategy_python)

pybind11_add_module(qp_python_backtest ${CMAKE_SOURCE_DIR}/src/apps/backtest/python/bindings.cpp)
target_link_libraries(qp_python_backtest PRIVATE qp_backtest_python_compose)
