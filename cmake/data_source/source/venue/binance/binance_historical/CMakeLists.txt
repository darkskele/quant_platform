# binance_historical — Binance Vision historical source.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

qp_dir(benchmarks QP_BENCH)

add_library(qp_binance_historical STATIC ${QP_SRC}/listing.cpp)
target_include_directories(qp_binance_historical PUBLIC ${QP_INC} ${QP_INC}/parsers)
target_link_libraries(qp_binance_historical PUBLIC qp_source_pure PRIVATE qp_network_http)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_binance_historical_tests
        ${QP_TESTS}/test_endpoints.cpp
        ${QP_TESTS}/test_listing.cpp
        ${QP_TESTS}/parsers/test_csv_field.cpp
        ${QP_TESTS}/parsers/test_klines.cpp
    )
    target_link_libraries(qp_binance_historical_tests
        PRIVATE qp_binance_historical qp_network_http GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_binance_historical_tests)
endif()
