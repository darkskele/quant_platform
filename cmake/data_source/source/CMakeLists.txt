# source — the Source concept. Header-only, depends on core only.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE ${QP_INC})
target_link_libraries(qp_source_pure INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_source_tests ${QP_TESTS}/test_source.cpp)
    target_link_libraries(qp_source_tests PRIVATE qp_source_pure GTest::gtest GTest::gtest_main)
    qp_register_test(qp_source_tests)
endif()
