# execution/sim/matcher — the Matcher concept SimExecution's fill
# sophistication plugs into. LastTradeMatcher lives under last_trade/.
# Header-only, depends on core only.
qp_dir(include QP_INC)

add_subdirectory(last_trade)

add_library(qp_execution_sim_matcher INTERFACE)
target_include_directories(qp_execution_sim_matcher INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim_matcher INTERFACE qp_core)
