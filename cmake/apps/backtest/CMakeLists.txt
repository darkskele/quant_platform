qp_dir(include QP_INC)

add_library(qp_backtest_compose INTERFACE)
target_include_directories(qp_backtest_compose INTERFACE ${QP_INC})
target_link_libraries(qp_backtest_compose INTERFACE
    qp_data_source qp_sink_fanout qp_transport qp_transport_backtest_in_process
    qp_engine qp_execution_sim
    qp_execution_sim_matcher_last_trade
    qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
    qp_risk_basic qp_strategy_carry
)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
