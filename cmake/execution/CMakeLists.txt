# execution — the ExecutionGateway/Matcher concepts + SimExecution (see
# libs/docs/DESIGN.md; LiveExecution deferred, docs/decisions.md D25).
# Header-only, depends on core only.

add_library(qp_execution INTERFACE)
target_include_directories(qp_execution INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_execution INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_execution_tests tests/test_sim_execution.cpp)
    target_link_libraries(qp_execution_tests PRIVATE qp_execution qp_core_test_support GTest::gtest GTest::gtest_main)
    qp_register_test(qp_execution_tests)
endif()
