# binance_historical — Binance Vision historical source.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_binance_historical INTERFACE)
target_include_directories(qp_binance_historical INTERFACE ${QP_INC})
target_link_libraries(qp_binance_historical INTERFACE qp_source_pure)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_binance_historical_tests ${QP_TESTS}/test_endpoints.cpp)
    target_link_libraries(qp_binance_historical_tests
        PRIVATE qp_binance_historical qp_network_http GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_binance_historical_tests)
endif()
