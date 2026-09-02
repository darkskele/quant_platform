# sink — the Sink concept.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_sink INTERFACE)
target_include_directories(qp_sink INTERFACE ${QP_INC})
target_link_libraries(qp_sink INTERFACE qp_core)

add_subdirectory(fanout)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_sink_tests ${QP_TESTS}/test_sink.cpp)
    target_link_libraries(qp_sink_tests PRIVATE qp_sink GTest::gtest GTest::gtest_main)
    qp_register_test(qp_sink_tests)
endif()
