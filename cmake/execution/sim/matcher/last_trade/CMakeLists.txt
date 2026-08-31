# execution/sim/matcher/last_trade
qp_dir(include QP_INC)
qp_dir(benchmarks QP_BENCH)

add_library(qp_execution_sim_matcher_last_trade INTERFACE)
target_include_directories(qp_execution_sim_matcher_last_trade INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim_matcher_last_trade INTERFACE qp_execution_sim_matcher)

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_execution_sim_matcher_last_trade_bench ${QP_BENCH}/bench_last_trade_matcher.cpp)
    target_link_libraries(qp_execution_sim_matcher_last_trade_bench PRIVATE
                           qp_execution_sim_matcher_last_trade qp_core_test_support benchmark::benchmark_main)
endif()
