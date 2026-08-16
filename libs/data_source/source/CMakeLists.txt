# source — the "prod streamer" town (see docs/DESIGN.md): resync_coordinator.hpp/
# resync.hpp/gap_detector.hpp/backoff.hpp/parser.hpp/source.hpp are pure
# logic, zero deps beyond core — protocol- and venue-agnostic, always
# buildable/testable regardless of which villages below are available.
# Villages nested here: venue (Binance glue) and protocol (the
# Boost.Beast transport) — each independently buildable/testable,
# add_subdirectory'd so the town aggregates its children.

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_source_pure INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_source_tests
        tests/test_gap_detector.cpp
        tests/test_backoff.cpp
        tests/test_resync.cpp
        tests/test_resync_coordinator.cpp
    )
    target_link_libraries(qp_source_tests PRIVATE qp_source_pure GTest::gtest GTest::gtest_main)
    add_test(NAME qp_source_tests COMMAND qp_source_tests)
endif()

add_subdirectory(venue)

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_source_bench benchmarks/bench_message_processing.cpp)
    target_link_libraries(qp_source_bench PRIVATE qp_source_pure qp_venue benchmark::benchmark_main)
endif()

add_subdirectory(protocol)

# Town-level aggregate targets (G3, docs/DESIGN.md): one command builds/tests
# everything under this town, children included. DEPENDS-only, not added to
# ALL — building the target builds whatever it depends on.
# qp_protocol_integration_tests only exists when protocol's Boost/OpenSSL
# guard passed, so it's conditionally included.
set(_qp_prod_streamer_test_deps qp_source_tests qp_venue_tests)
if(QP_PROTOCOL_WEBSOCKET_BUILT AND BUILD_TESTING)
    list(APPEND _qp_prod_streamer_test_deps qp_protocol_integration_tests)
endif()
add_custom_target(qp_prod_streamer_tests DEPENDS ${_qp_prod_streamer_test_deps})

if(QP_BUILD_BENCHMARKS)
    add_custom_target(qp_prod_streamer_bench DEPENDS qp_source_bench qp_venue_bench)
endif()
