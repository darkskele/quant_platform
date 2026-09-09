# execution/sim/matcher/cost_aware/cost_model/half_spread_linear
qp_dir(include QP_INC)
qp_dir(benchmarks QP_BENCH)

add_library(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear INTERFACE)
target_include_directories(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
                             INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
                        INTERFACE qp_execution_sim_matcher_cost_aware_cost_model)

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear_bench
                     ${QP_BENCH}/bench_half_spread_linear.cpp)
    target_link_libraries(qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear_bench
                           PRIVATE
                           qp_execution_sim_matcher_cost_aware_cost_model_half_spread_linear
                           qp_core_test_support benchmark::benchmark_main)
endif()
