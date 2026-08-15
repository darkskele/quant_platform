# marketdata — sources (live now, replay later). Depends on core and venue,
# both PUBLIC: MarketEvent (core) and WsEndpoint/RestEndpoint/SymbolTable
# (venue) all appear in live_ws_source.hpp's own public interface, not just
# its .cpp.
#
# Split deliberately: gap_detector.hpp/backoff.hpp are pure logic, zero
# deps beyond core, no Boost — always buildable/testable. LiveWebSocketSource
# needs the full Beast transport (Boost::system + OpenSSL). QUIET + a
# Boost_FOUND guard (not REQUIRED) so the pure pieces still configure and
# test on a machine without vcpkg/Boost yet; the transport and its
# integration test build once they're available.
find_package(Boost QUIET COMPONENTS system)
find_package(OpenSSL QUIET)

add_library(qp_marketdata_pure INTERFACE)
target_include_directories(qp_marketdata_pure INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# qp_venue, not just qp_core: resync_coordinator.hpp's on_snapshot() takes a
# venue::binance::DepthSnapshot by reference, so this "pure" (no Boost/OpenSSL)
# header now genuinely depends on venue's (simdjson-only, equally pure) types.
target_link_libraries(qp_marketdata_pure INTERFACE qp_core qp_venue)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_marketdata_tests
        tests/test_gap_detector.cpp
        tests/test_backoff.cpp
        tests/test_resync.cpp
        tests/test_resync_coordinator.cpp
    )
    target_link_libraries(qp_marketdata_tests PRIVATE qp_marketdata_pure GTest::gtest GTest::gtest_main)
    add_test(NAME qp_marketdata_tests COMMAND qp_marketdata_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_marketdata_bench benchmarks/bench_message_processing.cpp)
    target_link_libraries(qp_marketdata_bench PRIVATE qp_marketdata_pure qp_venue benchmark::benchmark)
endif()

if(Boost_FOUND AND OpenSSL_FOUND)
    find_package(Threads REQUIRED)

    add_library(qp_marketdata src/live_ws_source.cpp)
    target_link_libraries(qp_marketdata
        # qp_venue is PUBLIC, not PRIVATE: live_ws_source.hpp (the public
        # header) itself includes qp/venue/binance.hpp and uses WsEndpoint/
        # RestEndpoint/SymbolTable in the class interface, so consumers
        # transitively need venue's include path too. Boost/OpenSSL stay
        # PRIVATE — confined to the .cpp, never named in the public header.
        PUBLIC  qp_marketdata_pure qp_venue
        PRIVATE Boost::system OpenSSL::SSL OpenSSL::Crypto Threads::Threads
    )

    if(BUILD_TESTING)
        add_executable(qp_marketdata_integration_tests tests/test_live_ws_source_integration.cpp)
        target_link_libraries(qp_marketdata_integration_tests
            PRIVATE qp_marketdata Boost::system Threads::Threads GTest::gtest GTest::gtest_main
        )
        add_test(NAME qp_marketdata_integration_tests COMMAND qp_marketdata_integration_tests)
    endif()
else()
    message(STATUS "marketdata: Boost/OpenSSL not found — qp_marketdata (LiveWebSocketSource) and its "
                    "integration test skipped. Needed once vcpkg is wired; gap_detector/backoff tests "
                    "and the message-processing benchmark still build.")
endif()
