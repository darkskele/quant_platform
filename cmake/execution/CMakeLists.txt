# execution — the ExecutionGateway concept
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_execution INTERFACE)
target_include_directories(qp_execution INTERFACE ${QP_INC})
target_link_libraries(qp_execution INTERFACE qp_core)

add_subdirectory(sim)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_execution_tests ${QP_TESTS}/test_execution_gateway.cpp)
    target_link_libraries(qp_execution_tests PRIVATE qp_execution GTest::gtest GTest::gtest_main)
    qp_register_test(qp_execution_tests)
endif()
