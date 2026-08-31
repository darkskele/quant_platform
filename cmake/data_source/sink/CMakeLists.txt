# sink — the Sink concept.
qp_dir(include QP_INC)

add_library(qp_sink INTERFACE)
target_include_directories(qp_sink INTERFACE ${QP_INC})
target_link_libraries(qp_sink INTERFACE qp_core)

add_subdirectory(fanout)
