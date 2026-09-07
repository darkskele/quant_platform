# risk
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_risk INTERFACE)
target_include_directories(qp_risk INTERFACE ${QP_INC})
target_link_libraries(qp_risk INTERFACE qp_core)

# Test doubles (AlwaysApproveRiskGate, AlwaysRejectRiskGate) shared across
# risk's own tests and engine's tests/benchmarks. 
add_library(qp_risk_test_support INTERFACE)
target_include_directories(qp_risk_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(qp_risk_test_support INTERFACE qp_risk)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_risk_tests ${QP_TESTS}/test_risk_gate.cpp)
    target_link_libraries(qp_risk_tests PRIVATE qp_risk qp_risk_test_support qp_core_test_support
                                                 GTest::gtest GTest::gtest_main)
    qp_register_test(qp_risk_tests)
endif()

add_subdirectory(basic)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
