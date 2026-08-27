# venue — Binance-specific glue, the "venue" village of the prod-streamer
# town (see docs/data_source/source/DESIGN.md). Depends on core, qp_source_pure
# (binance.hpp publicly aliases the town-level venue-agnostic SymbolTable/
# WsEndpoint/RestEndpoint/DepthSnapshot — see venue_types.hpp), and simdjson;
# add_subdirectory'd from data_source/source's CMakeLists.txt, after
# qp_source_pure exists.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

find_package(simdjson CONFIG REQUIRED)

add_library(qp_venue ${QP_SRC}/binance.cpp)
target_include_directories(qp_venue PUBLIC ${QP_INC})
target_link_libraries(qp_venue PUBLIC qp_core qp_source_pure PRIVATE simdjson::simdjson)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_venue_tests ${QP_TESTS}/test_binance_parser.cpp)
    target_link_libraries(qp_venue_tests PRIVATE qp_venue GTest::gtest GTest::gtest_main)
    qp_register_test(qp_venue_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # googlebenchmark is fetched once, at the top level.
    add_executable(qp_venue_bench ${QP_BENCH}/bench_binance_parser.cpp)
    target_link_libraries(qp_venue_bench PRIVATE qp_venue benchmark::benchmark_main)
endif()
