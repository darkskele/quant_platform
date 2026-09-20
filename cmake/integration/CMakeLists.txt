# integration — end to end tests against the live bucket.
qp_dir(tests QP_TESTS)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)

    # Not registered with ctest. It fetches real files and takes minutes, so it
    # is run on purpose rather than with the fast suite.
    file(GLOB QP_INTEGRATION_SRCS CONFIGURE_DEPENDS ${QP_TESTS}/test_*.cpp)
    add_executable(qp_integration_tests ${QP_INTEGRATION_SRCS})
    target_link_libraries(qp_integration_tests
        PRIVATE qp_core qp_binance_historical qp_network_http qp_archive_zip qp_data_source
                Threads::Threads GTest::gtest GTest::gtest_main
    )
endif()
