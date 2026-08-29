# execution — the ExecutionGateway concept (see docs/core/DESIGN.md;
# LiveExecution deferred, docs/decisions.md D25). SimExecution/Matcher live
# under sim/ — Sim-only, Live never uses them. Header-only, depends on core
# only.
qp_dir(include QP_INC)

add_library(qp_execution INTERFACE)
target_include_directories(qp_execution INTERFACE ${QP_INC})
target_link_libraries(qp_execution INTERFACE qp_core)

add_subdirectory(sim)
