# protocol — the "protocol" village of the prod-streamer town (see
# libs/data_source/source/docs/DESIGN.md): the Boost.Beast websocket transport
# (GenericLiveWebSocketSource) + its integration test + shared mock-server
# test support (reused by apps/collector's own integration test). QUIET + a
# Boost_FOUND guard (not REQUIRED) so the town's pure pieces still
# configure/test on a machine without vcpkg/Boost wired up yet.
find_package(Boost QUIET COMPONENTS system)
find_package(OpenSSL QUIET)

if(Boost_FOUND AND OpenSSL_FOUND)
    find_package(Threads REQUIRED)

    add_library(qp_protocol src/live_websocket_source.cpp)
    target_include_directories(qp_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(qp_protocol
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
    # not duplicated a third time. include dir is tests/ itself (not
    # tests/support/) so #include "support/mock_binance_server.hpp" resolves
    # the same way from any consumer.
    add_library(qp_protocol_test_support INTERFACE)
    target_include_directories(qp_protocol_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(qp_protocol_test_support INTERFACE qp_protocol Boost::system Threads::Threads)

    if(BUILD_TESTING)
        add_executable(qp_protocol_integration_tests tests/test_live_websocket_source_integration.cpp)
        target_link_libraries(qp_protocol_integration_tests
            PRIVATE qp_protocol_test_support GTest::gtest GTest::gtest_main
        )
        add_test(NAME qp_protocol_integration_tests COMMAND qp_protocol_integration_tests)
    endif()

    set(QP_PROTOCOL_WEBSOCKET_BUILT TRUE PARENT_SCOPE)
else()
    message(STATUS "source/protocol: Boost/OpenSSL not found — qp_protocol "
                    "(LiveWebSocketSource) and its integration test skipped. Needed once vcpkg is "
                    "wired; the town's pure pieces (resync/gap-detector/backoff tests, the "
                    "message-processing benchmark) still build.")
endif()
