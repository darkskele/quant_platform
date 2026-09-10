# python — PythonBacktest variant. Same compose shape as the outer backtest
# but with the python strategy/risk adapters selected via engine_config.hpp.
# The matcher axis follows QP_BACKTEST_MATCHER so cost_aware backtests can
# also drive Python strategies.
qp_dir(include QP_INC)
# bindings.cpp and python_backtest.hpp include config/ and python/ from the
# backtest include root, one level above this module's own include dir.
get_filename_component(QP_BACKTEST_INC "${QP_INC}" DIRECTORY)

if(QP_BACKTEST_MATCHER STREQUAL "last_trade")
    set(_qp_bt_py_matcher_macro QP_MATCHER_LAST_TRADE)
    set(_qp_bt_py_matcher_lib qp_execution_sim_matcher_last_trade)
elseif(QP_BACKTEST_MATCHER STREQUAL "cost_aware")
    set(_qp_bt_py_matcher_macro QP_MATCHER_COST_AWARE)
    set(_qp_bt_py_matcher_lib qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear)
else()
    message(FATAL_ERROR "unknown QP_BACKTEST_MATCHER '${QP_BACKTEST_MATCHER}'")
endif()

add_library(qp_backtest_python_compose INTERFACE)
target_include_directories(qp_backtest_python_compose INTERFACE ${QP_BACKTEST_INC})
target_compile_definitions(qp_backtest_python_compose INTERFACE
    QP_SOURCE_CSV_BINHIST
    QP_SINK_FANOUT
    ${_qp_bt_py_matcher_macro}
    QP_RISK_PYTHON
    QP_STRATEGY_PYTHON
)
target_link_libraries(qp_backtest_python_compose INTERFACE qp_data_source qp_csv_source
                                                            qp_source_venue_binance_historical
                                                            qp_sink_fanout qp_transport
                                                            qp_transport_backtest_in_process qp_engine
                                                            qp_execution_sim
                                                            ${_qp_bt_py_matcher_lib}
                                                            qp_risk_python qp_strategy_python)

pybind11_add_module(qp_python_backtest ${CMAKE_SOURCE_DIR}/src/apps/backtest/python/bindings.cpp)
target_link_libraries(qp_python_backtest PRIVATE qp_backtest_python_compose)
