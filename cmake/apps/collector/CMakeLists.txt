# collector — thin apps/ wiring: a venue-selected GenericLiveWebSocketSource
# -> SelectedRecorder (see collector_venue.hpp/recorder.hpp below), no
# Engine, no strategies (architecture-principles.md: "the collector is NOT
# an Engine"). Needs Boost/OpenSSL (via qp_source) and zstd (via qp_sink ->
# qp_wire) — both already required by their own libs, but re-checked here:
# add_subdirectory scoping means data_source/source's and data_source/wire's
# own find_package results (Boost_FOUND/OpenSSL_FOUND, the zstd::libzstd
# imported target) don't propagate to this sibling directory.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

find_package(Boost QUIET COMPONENTS system)
find_package(OpenSSL QUIET)
find_package(zstd CONFIG REQUIRED)

# Which venue this collector binary is built against — the one place a
# concrete venue gets named at build-configuration level (collector_venue.hpp,
# the only C++ file allowed to name one, reads this back as QP_VENUE_<NAME>).
# "binance" is the only real option today; a second venue adds a STRINGS
# entry here and a branch in collector_venue.hpp, nothing else.
set(QP_COLLECTOR_VENUE "binance" CACHE STRING "Venue apps/collector is built against")
set_property(CACHE QP_COLLECTOR_VENUE PROPERTY STRINGS binance)
string(TOUPPER "${QP_COLLECTOR_VENUE}" QP_COLLECTOR_VENUE_UPPER)

# Which Sink/recorder implementation this collector binary is built against
# — an orthogonal axis to venue (recorder.hpp reads this back as
# QP_RECORDER_<NAME>). "file" (sink::FileRecorder) is the only real option
# today; a different file type/recorder adds a STRINGS entry here and a
# branch in recorder.hpp, nothing else.
set(QP_COLLECTOR_RECORDER "file" CACHE STRING "Recorder/Sink implementation apps/collector is built against")
set_property(CACHE QP_COLLECTOR_RECORDER PROPERTY STRINGS file)
string(TOUPPER "${QP_COLLECTOR_RECORDER}" QP_COLLECTOR_RECORDER_UPPER)

if(Boost_FOUND AND OpenSSL_FOUND)
    add_library(qp_collector_core ${QP_SRC}/collector.cpp)
    target_include_directories(qp_collector_core PUBLIC ${QP_INC})
    # qp_data_source: run_data_source.hpp (D43), the driver that pairs the
    # two legs' sources with their recorders and stamps MarketEvent::venue.
    target_link_libraries(qp_collector_core PUBLIC qp_source qp_sink qp_data_source)
    target_compile_definitions(qp_collector_core PUBLIC
        QP_VENUE_${QP_COLLECTOR_VENUE_UPPER}
        QP_RECORDER_${QP_COLLECTOR_RECORDER_UPPER}
    )

    add_executable(qp_collector ${QP_SRC}/main.cpp)
    target_link_libraries(qp_collector PRIVATE qp_collector_core)

    if(BUILD_TESTING)
        find_package(GTest CONFIG REQUIRED)
        add_executable(qp_collector_integration_tests ${QP_TESTS}/test_collector_integration.cpp)
        target_link_libraries(qp_collector_integration_tests
            PRIVATE qp_collector_core qp_source_test_support qp_core_test_support zstd::libzstd
                    GTest::gtest GTest::gtest_main
        )
        qp_register_test(qp_collector_integration_tests)
    endif()

    # No qp_collector_bench: apps/ is thin composition wiring, not its own
    # thing to benchmark — GenericLiveWebSocketSource and FileRecorder
    # already have their own benchmarks. A full-collector-run benchmark
    # here would just be a slower, noisier re-measurement of the same two
    # components' costs bundled together (see docs/decisions.md D53).
else()
    message(STATUS "collector: Boost/OpenSSL not found — apps/collector skipped (needs qp_source).")
endif()
