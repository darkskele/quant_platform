# strategy — the Strategy concept (see libs/docs/DESIGN.md; docs/decisions.md
# D27/D34). Header-only, depends on core only.

add_library(qp_strategy INTERFACE)
target_include_directories(qp_strategy INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_strategy INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_strategy_tests tests/test_strategy.cpp)
    target_link_libraries(qp_strategy_tests PRIVATE qp_strategy GTest::gtest GTest::gtest_main)
    qp_register_test(qp_strategy_tests)
endif()
