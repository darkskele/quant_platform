# risk
qp_dir(include QP_INC)

add_library(qp_risk INTERFACE)
target_include_directories(qp_risk INTERFACE ${QP_INC})
target_link_libraries(qp_risk INTERFACE qp_core)

# Test doubles (AlwaysApproveRiskGate, AlwaysRejectRiskGate) shared across
# risk's own tests and engine's tests/benchmarks. 
add_library(qp_risk_test_support INTERFACE)
target_include_directories(qp_risk_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(qp_risk_test_support INTERFACE qp_risk)

add_subdirectory(basic)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
