# python — PythonRiskGate adapter forwarding RiskGate calls to a py::object.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_risk_python INTERFACE)
target_include_directories(qp_risk_python INTERFACE ${QP_INC})
target_link_libraries(qp_risk_python INTERFACE qp_risk pybind11::headers)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_risk_python_tests ${QP_TESTS}/test_python_risk_gate.cpp)
    target_link_libraries(qp_risk_python_tests PRIVATE qp_risk_python qp_core_test_support
                                                        pybind11::embed GTest::gtest
                                                        GTest::gtest_main)
    qp_register_test(qp_risk_python_tests)
endif()
