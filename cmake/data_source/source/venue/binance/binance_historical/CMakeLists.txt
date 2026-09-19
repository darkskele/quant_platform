# binance_historical — Binance Vision historical source.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

qp_dir(benchmarks QP_BENCH)

find_package(Threads REQUIRED)

add_library(qp_binance_historical STATIC
    ${QP_SRC}/listing.cpp
    ${QP_SRC}/http_fetch_pool.cpp
)
target_include_directories(qp_binance_historical PUBLIC ${QP_INC} ${QP_INC}/parsers)
target_link_libraries(qp_binance_historical
    PUBLIC qp_source_pure Threads::Threads
    PRIVATE qp_network_http qp_archive_zip
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_binance_historical_tests
        ${QP_TESTS}/test_endpoints.cpp
        ${QP_TESTS}/test_listing.cpp
        ${QP_TESTS}/test_stream.cpp
        ${QP_TESTS}/test_http_fetch_pool.cpp
        ${QP_TESTS}/parsers/test_csv_field.cpp
        ${QP_TESTS}/parsers/test_funding.cpp
        ${QP_TESTS}/parsers/test_klines.cpp
        ${QP_TESTS}/parsers/test_price_klines.cpp
    )
    target_link_libraries(qp_binance_historical_tests
        PRIVATE qp_binance_historical qp_network_http qp_core_test_support
                GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_binance_historical_tests)
endif()
