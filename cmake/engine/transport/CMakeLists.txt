# transport — the Transport concept.
qp_dir(include QP_INC)

add_library(qp_transport INTERFACE)
target_include_directories(qp_transport INTERFACE ${QP_INC})
target_link_libraries(qp_transport INTERFACE qp_core)

add_subdirectory(backtest_in_process)
