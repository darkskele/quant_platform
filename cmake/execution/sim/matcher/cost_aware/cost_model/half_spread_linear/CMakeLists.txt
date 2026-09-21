# execution/sim/matcher/cost_aware/cost_model/half_spread_linear
qp_dir(include QP_INC)

add_library(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear INTERFACE)
target_include_directories(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
                             INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
                        INTERFACE qp_execution_sim_matcher_cost_aware_cost_model)
