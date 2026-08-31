# strategy — the Strategy concept
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_strategy INTERFACE)
target_include_directories(qp_strategy INTERFACE ${QP_INC})
target_link_libraries(qp_strategy INTERFACE qp_core)

# Test doubles (NoopStrategy, AlwaysIntentStrategy) shared across strategy's
# own tests and engine's tests/benchmarks.
add_library(qp_strategy_test_support INTERFACE)
target_include_directories(qp_strategy_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(qp_strategy_test_support INTERFACE qp_strategy)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_strategy_tests ${QP_TESTS}/test_strategy.cpp)
    target_link_libraries(qp_strategy_tests PRIVATE qp_strategy qp_strategy_test_support GTest::gtest
                                                     GTest::gtest_main)
    qp_register_test(qp_strategy_tests)
endif()

add_subdirectory(carry)
