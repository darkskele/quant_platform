# venue — Binance-specific glue, the "venue" village of the
# prod-streamer town (see libs/data_source/source/docs/DESIGN.md). Depends on
# core, qp_source_pure (binance.hpp publicly aliases the town-level
# venue-agnostic SymbolTable/WsEndpoint/RestEndpoint/DepthSnapshot — see
# venue_types.hpp), and simdjson; add_subdirectory'd from
# libs/data_source/source/CMakeLists.txt, after qp_source_pure exists.
find_package(simdjson CONFIG REQUIRED)

add_library(qp_venue src/binance.cpp)
target_include_directories(qp_venue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_venue PUBLIC qp_core qp_source_pure PRIVATE simdjson::simdjson)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_venue_tests tests/test_binance_parser.cpp)
    target_link_libraries(qp_venue_tests PRIVATE qp_venue GTest::gtest GTest::gtest_main)
    add_test(NAME qp_venue_tests COMMAND qp_venue_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # googlebenchmark is fetched once, at the top level.
    add_executable(qp_venue_bench benchmarks/bench_binance_parser.cpp)
    target_link_libraries(qp_venue_bench PRIVATE qp_venue benchmark::benchmark_main)
endif()
