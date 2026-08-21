# risk — the RiskGate concept + RiskDecision (see libs/docs/DESIGN.md;
# docs/decisions.md D27/D35). Header-only, depends on core only.

add_library(qp_risk INTERFACE)
target_include_directories(qp_risk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_risk INTERFACE qp_core)

# Test doubles (AlwaysApproveRiskGate, AlwaysRejectRiskGate) shared across
# risk's own tests and engine's tests/benchmarks. Unconditional, same
# convention as qp_core_test_support.
add_library(qp_risk_test_support INTERFACE)
target_include_directories(qp_risk_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(qp_risk_test_support INTERFACE qp_risk)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_risk_tests tests/test_risk_gate.cpp)
    target_link_libraries(qp_risk_tests PRIVATE qp_risk qp_risk_test_support GTest::gtest GTest::gtest_main)
    qp_register_test(qp_risk_tests)
endif()
