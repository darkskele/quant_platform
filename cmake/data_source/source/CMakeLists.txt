# source — the "prod streamer" town (see docs/DESIGN.md): resync_coordinator.hpp/
# resync.hpp/gap_detector.hpp/backoff.hpp/parser.hpp/source.hpp/
# live_websocket_source.hpp are pure logic plus the one concrete live
# transport (Boost.Beast) — venue-agnostic, live_websocket_source.cpp is the
# only piece that needs Boost/OpenSSL. Two villages: venue (Binance glue,
# independently buildable/testable, add_subdirectory'd) and resync (the
# AlignmentRule concept + concrete market policies, D13/D36 — header-only,
# no separate target needed, just another include path).
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE
    ${QP_INC}
    ${QP_INC}/resync
)
target_link_libraries(qp_source_pure INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_source_tests
        ${QP_TESTS}/test_gap_detector.cpp
        ${QP_TESTS}/test_backoff.cpp
        ${QP_TESTS}/test_resync.cpp
        ${QP_TESTS}/test_resync_coordinator.cpp
        ${QP_TESTS}/test_resync_policy.cpp
    )
    target_link_libraries(qp_source_tests PRIVATE qp_source_pure GTest::gtest GTest::gtest_main)
    qp_register_test(qp_source_tests)
endif()

add_subdirectory(venue)

# FileReplaySource — the backtest Source implementation (D18/D19): reads
# back what FileRecorder wrote, via the shared wire codec (qp_wire) rather
# than a second implementation of the format. No Boost/OpenSSL needed (no
# live transport), so this builds unconditionally, unlike qp_source below.
add_library(qp_file_replay ${QP_SRC}/file_replay_source.cpp)
target_link_libraries(qp_file_replay PUBLIC qp_source_pure qp_wire)

if(BUILD_TESTING)
    add_executable(qp_file_replay_tests ${QP_TESTS}/test_file_replay_source.cpp)
    # qp_sink is test-only here: fixtures are written via a real FileRecorder
    # (not a hand-rolled copy of its write logic — see the test file's own
    # comment), never linked into qp_file_replay itself. Production
    # dependency graph stays clean (source depends on wire, not sink).
    target_link_libraries(qp_file_replay_tests
        PRIVATE qp_file_replay qp_sink qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_file_replay_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_source_bench
        ${QP_BENCH}/bench_gap_detector.cpp
        ${QP_BENCH}/bench_venue_types.cpp
        ${QP_BENCH}/bench_backoff.cpp
        ${QP_BENCH}/resync/bench_resync_policy.cpp
        ${QP_BENCH}/bench_resync_coordinator.cpp
    )
    target_link_libraries(qp_source_bench PRIVATE qp_source_pure qp_venue benchmark::benchmark_main)

    add_executable(qp_file_replay_bench ${QP_BENCH}/bench_file_replay_source.cpp)
    target_link_libraries(qp_file_replay_bench
        PRIVATE qp_file_replay qp_core_test_support benchmark::benchmark_main
    )
endif()

# Live transport (GenericLiveWebSocketSource, live_websocket_source.cpp) —
# needs Boost/OpenSSL. QUIET + a Boost_FOUND guard (not REQUIRED) so the
# town's pure pieces still configure/test on a machine without vcpkg/Boost
# wired up yet. Formerly its own "protocol" village/subdirectory; collapsed
# up into this town directly now that it's the only transport that exists
# (see docs/data_source/source/DECISIONS.md).
find_package(Boost QUIET COMPONENTS system)
find_package(OpenSSL QUIET)

if(Boost_FOUND AND OpenSSL_FOUND)
    find_package(Threads REQUIRED)

    add_library(qp_source ${QP_SRC}/live_websocket_source.cpp)
    # No target_include_directories here: live_websocket_source.hpp lives in
    # this same include/, already exported by qp_source_pure (linked PUBLIC
    # below) — a second target_include_directories call would just
    # duplicate that path.
    target_link_libraries(qp_source
        # qp_venue is PUBLIC, not PRIVATE: live_websocket_source.hpp (the public
        # header) itself includes binance.hpp and uses
        # WsEndpoint/RestEndpoint/SymbolTable in the class interface, so
        # consumers transitively need venue's include path too. Boost/
        # OpenSSL stay PRIVATE — confined to the .cpp, never named in the
        # public header.
        PUBLIC  qp_source_pure qp_venue
        PRIVATE Boost::system OpenSSL::SSL OpenSSL::Crypto Threads::Threads
    )

    # Mock WS/HTTP server test infrastructure (tests/support/), exposed as its
    # own target so apps/collector's integration test can reuse it — written
    # once for this lib's own integration test, needed a second time there,
    # not duplicated a third time. Include dir is the shared top-level tests/
    # root (D50), so #include "support/mock_binance_server.hpp" resolves the
    # same way from any consumer.
    add_library(qp_source_test_support INTERFACE)
    target_include_directories(qp_source_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(qp_source_test_support INTERFACE qp_source Boost::system Threads::Threads)

    if(BUILD_TESTING)
        add_executable(qp_source_integration_tests ${QP_TESTS}/test_live_websocket_source_integration.cpp)
        target_link_libraries(qp_source_integration_tests
            PRIVATE qp_source_test_support GTest::gtest GTest::gtest_main
        )
        qp_register_test(qp_source_integration_tests)
    endif()

    set(QP_SOURCE_LIVE_BUILT TRUE)

    # No qp_source_live_bench: on_message() is private and only reachable by
    # standing up a real mock WS+HTTP server pair per iteration, which times
    # server startup/socket/handshake overhead, not the parse/gap-check/
    # queue-push logic actually worth measuring — needs a throughput-style
    # harness, not GBench's per-iteration model. Tried, deleted (see
    # docs/decisions.md D53); revisit with a different harness shape, not a
    # patch on this one.
else()
    message(STATUS "source: Boost/OpenSSL not found — qp_source "
                    "(LiveWebSocketSource) and its integration test skipped. Needed once vcpkg is "
                    "wired; the town's pure pieces (resync/gap-detector/backoff tests, the "
                    "message-processing benchmark) still build.")
endif()

# Town-level aggregate targets (G3, docs/DESIGN.md): one command builds/tests
# everything under this town, children included. DEPENDS-only, not added to
# ALL — building the target builds whatever it depends on.
# qp_source_integration_tests only exists when the live-transport Boost/
# OpenSSL guard passed, so it's conditionally included.
set(_qp_prod_streamer_test_deps qp_source_tests qp_venue_tests)
if(BUILD_TESTING)
    list(APPEND _qp_prod_streamer_test_deps qp_file_replay_tests)
endif()
if(QP_SOURCE_LIVE_BUILT AND BUILD_TESTING)
    list(APPEND _qp_prod_streamer_test_deps qp_source_integration_tests)
endif()
add_custom_target(qp_prod_streamer_tests DEPENDS ${_qp_prod_streamer_test_deps})

if(QP_BUILD_BENCHMARKS)
    add_custom_target(qp_prod_streamer_bench
        DEPENDS qp_source_bench qp_file_replay_bench qp_venue_bench
    )
endif()
