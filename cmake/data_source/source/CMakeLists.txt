# source — the Source concept and the venue-agnostic types
# (SymbolTable/WsEndpoint/RestEndpoint/DepthSnapshot) a Parser is built
# against (see docs/DESIGN.md). Header-only, depends on core only.
#
# The live-transport machinery (LiveWebSocketSource, resync/gap-detection/
# backoff, the Binance WS parser) was removed along with apps/collector
# (docs/decisions.md) — this town is being redesigned around historical/
# replay data instead of live capture; venue/ (add_subdirectory'd below)
# is where a venue's wire-protocol glue and symbol universe live now —
# one subdirectory per (exchange, source-type), e.g. venue/binance/
# binance_historical/.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE ${QP_INC})
target_link_libraries(qp_source_pure INTERFACE qp_core)

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_source_pure_bench ${QP_BENCH}/bench_venue_types.cpp)
    target_link_libraries(qp_source_pure_bench PRIVATE qp_source_pure benchmark::benchmark_main)
endif()

add_subdirectory(venue)

# FileReplaySource — the backtest Source implementation: reads back what
# was recorded, via the shared wire codec (qp_wire) rather than a second
# implementation of the format. No Boost/OpenSSL needed (no live
# transport), so this builds unconditionally.
add_library(qp_file_replay ${QP_SRC}/file_replay_source.cpp)
target_link_libraries(qp_file_replay PUBLIC qp_source_pure qp_wire)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_file_replay_tests ${QP_TESTS}/test_file_replay_source.cpp)
    target_link_libraries(qp_file_replay_tests
        PRIVATE qp_file_replay qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_file_replay_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_file_replay_bench ${QP_BENCH}/bench_file_replay_source.cpp)
    target_link_libraries(qp_file_replay_bench
        PRIVATE qp_file_replay qp_core_test_support benchmark::benchmark_main
    )
endif()
