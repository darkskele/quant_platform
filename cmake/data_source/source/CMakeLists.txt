# source — the Source concept. Header-only, depends on core only.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE ${QP_INC})
target_link_libraries(qp_source_pure INTERFACE qp_core)

add_subdirectory(venue)

add_library(qp_csv_source INTERFACE)
target_include_directories(qp_csv_source INTERFACE ${QP_INC})
target_link_libraries(qp_csv_source INTERFACE qp_source_pure qp_source_venue)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_source_tests ${QP_TESTS}/test_source.cpp)
    target_link_libraries(qp_source_tests PRIVATE qp_source_pure GTest::gtest GTest::gtest_main)
    qp_register_test(qp_source_tests)

    add_executable(qp_csv_source_tests ${QP_TESTS}/test_csv_source.cpp)
    target_link_libraries(qp_csv_source_tests
        PRIVATE qp_csv_source qp_source_venue_binance_historical qp_core_test_support
                GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_csv_source_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_csv_source_bench ${QP_BENCH}/bench_csv_source.cpp)
    target_link_libraries(qp_csv_source_bench
        PRIVATE qp_csv_source qp_source_venue_binance_historical qp_core_test_support
                benchmark::benchmark_main
    )
endif()
