# collector — thin apps/ wiring: a venue-selected GenericLiveWebSocketSource
# -> FileRecorder (see QP_COLLECTOR_VENUE below, venue.hpp), no Engine, no
# strategies (architecture-principles.md: "the collector is NOT an Engine").
# Needs Boost/OpenSSL (via qp_source) and zstd (via
# qp_sink) — both already required by their own libs, but re-checked here:
# add_subdirectory scoping means libs/data_source/source's and
# libs/data_source/sink's own find_package results (Boost_FOUND/OpenSSL_FOUND,
# the zstd::libzstd imported target) don't propagate to this sibling directory.
find_package(Boost QUIET COMPONENTS system)
find_package(OpenSSL QUIET)
find_package(zstd CONFIG REQUIRED)

# Which venue this collector binary is built against — the one place a
# concrete venue gets named at build-configuration level (venue.hpp, the
# only C++ file allowed to name one, reads this back as QP_VENUE_<NAME>).
# "binance" is the only real option today; a second venue adds a STRINGS
# entry here and a branch in venue.hpp, nothing else.
set(QP_COLLECTOR_VENUE "binance" CACHE STRING "Venue apps/collector is built against")
set_property(CACHE QP_COLLECTOR_VENUE PROPERTY STRINGS binance)
string(TOUPPER "${QP_COLLECTOR_VENUE}" QP_COLLECTOR_VENUE_UPPER)

if(Boost_FOUND AND OpenSSL_FOUND)
    add_library(qp_collector_core collector.cpp)
    target_link_libraries(qp_collector_core PUBLIC qp_source qp_sink)
    target_compile_definitions(qp_collector_core PUBLIC QP_VENUE_${QP_COLLECTOR_VENUE_UPPER})

    add_executable(qp_collector main.cpp)
    target_link_libraries(qp_collector PRIVATE qp_collector_core)

    if(BUILD_TESTING)
        find_package(GTest CONFIG REQUIRED)
        add_executable(qp_collector_integration_tests tests/test_collector_integration.cpp)
        target_link_libraries(qp_collector_integration_tests
            PRIVATE qp_collector_core qp_source_test_support zstd::libzstd GTest::gtest GTest::gtest_main
        )
        add_test(NAME qp_collector_integration_tests COMMAND qp_collector_integration_tests)
    endif()
else()
    message(STATUS "collector: Boost/OpenSSL not found — apps/collector skipped (needs qp_source).")
endif()
