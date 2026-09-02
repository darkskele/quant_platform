# execution/sim/matcher
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_subdirectory(last_trade)

add_library(qp_execution_sim_matcher INTERFACE)
target_include_directories(qp_execution_sim_matcher INTERFACE ${QP_INC})
target_link_libraries(qp_execution_sim_matcher INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_execution_sim_matcher_tests ${QP_TESTS}/test_matcher.cpp)
    target_link_libraries(qp_execution_sim_matcher_tests PRIVATE qp_execution_sim_matcher
                                                                 GTest::gtest GTest::gtest_main)
    qp_register_test(qp_execution_sim_matcher_tests)
endif()
