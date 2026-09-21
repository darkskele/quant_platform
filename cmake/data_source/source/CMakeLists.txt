# source — the Source concept. Header-only, depends on core only.
qp_dir(include QP_INC)

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE ${QP_INC})
target_link_libraries(qp_source_pure INTERFACE qp_core)
