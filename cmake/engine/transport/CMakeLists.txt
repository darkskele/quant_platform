# transport — the Transport concept.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_transport INTERFACE)
target_include_directories(qp_transport INTERFACE ${QP_INC})
target_link_libraries(qp_transport INTERFACE qp_core)

add_subdirectory(backtest_in_process)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_transport_tests ${QP_TESTS}/test_transport.cpp)
    target_link_libraries(qp_transport_tests PRIVATE qp_transport GTest::gtest GTest::gtest_main)
    qp_register_test(qp_transport_tests)
endif()
