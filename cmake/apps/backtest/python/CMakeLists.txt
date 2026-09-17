qp_dir(include QP_INC)
get_filename_component(QP_BACKTEST_INC "${QP_INC}" DIRECTORY)

pybind11_add_module(qp_python_backtest ${CMAKE_SOURCE_DIR}/src/apps/backtest/python/bindings.cpp)
target_include_directories(qp_python_backtest PRIVATE ${QP_BACKTEST_INC})
target_link_libraries(qp_python_backtest PRIVATE
    qp_engine qp_transport qp_transport_backtest_in_process qp_execution_sim
    qp_execution_sim_matcher_last_trade
    qp_data_source qp_sink_fanout qp_risk_python qp_strategy_python
)
