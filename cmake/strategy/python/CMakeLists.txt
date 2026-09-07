# python — PythonStrategy adapter forwarding Strategy calls to a py::object.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_strategy_python INTERFACE)
target_include_directories(qp_strategy_python INTERFACE ${QP_INC})
target_link_libraries(qp_strategy_python INTERFACE qp_strategy pybind11::headers)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_strategy_python_tests ${QP_TESTS}/test_python_strategy.cpp)
    target_link_libraries(qp_strategy_python_tests PRIVATE qp_strategy_python qp_core_test_support
                                                            pybind11::embed GTest::gtest
                                                            GTest::gtest_main)
    qp_register_test(qp_strategy_python_tests)
endif()
