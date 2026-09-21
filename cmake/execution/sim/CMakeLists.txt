# execution/sim 
qp_dir(include QP_INC)

add_subdirectory(matcher)

add_library(qp_execution_sim INTERFACE)
target_include_directories(qp_execution_sim INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim INTERFACE qp_execution qp_execution_sim_matcher)
