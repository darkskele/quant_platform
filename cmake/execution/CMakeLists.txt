# execution — the ExecutionGateway concept
qp_dir(include QP_INC)

add_library(qp_execution INTERFACE)
target_include_directories(qp_execution INTERFACE ${QP_INC})
target_link_libraries(qp_execution INTERFACE qp_core)

add_subdirectory(sim)
