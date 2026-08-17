# record — on-disk wire format + FileRecorder sink. wire.hpp is pure (no
# I/O, no compression, just serialization) — shared by FileRecorder (write
# side) and FileReplaySource (Phase 1, read side) so both ends of the
# collector<->backtest boundary use the exact same format, not two
# implementations kept in sync by discipline. See docs/decisions.md D12.

add_library(qp_sink_pure INTERFACE)
target_include_directories(qp_sink_pure INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_sink_pure INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_sink_tests
        tests/test_wire.cpp
        tests/test_partition.cpp
    )
    target_link_libraries(qp_sink_tests PRIVATE qp_sink_pure GTest::gtest GTest::gtest_main)
    add_test(NAME qp_sink_tests COMMAND qp_sink_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_sink_bench benchmarks/bench_wire.cpp)
    target_link_libraries(qp_sink_bench PRIVATE qp_sink_pure benchmark::benchmark_main)
endif()

# FileRecorder — needs zstd. libzstd's dev headers/CMake config aren't in the
# apt libzstd1 package on this box (runtime only, no -dev), but are
# available via conda (same source as Boost/OpenSSL/simdjson/GTest already
# are) — resolved reliably enough from there to be REQUIRED, same as
# simdjson/GTest, not QUIET-guarded like Boost/OpenSSL (which were genuinely
# uncertain when that guard was added).
find_package(zstd CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(qp_sink src/file_recorder.cpp)
target_link_libraries(qp_sink
    # zstd::libzstd is PUBLIC, not PRIVATE: file_recorder.hpp (the public
    # header) transitively includes zstd_stream.hpp, which includes
    # <zstd.h> for ZstdCompressor's ZSTD_CCtx/ZSTD_EndDirective types — same
    # PUBLIC-vs-PRIVATE bug already hit once with qp_venue/qp_source
    # (D-something: a public header naming a dependency's types means
    # consumers transitively need that dependency's include path too).
    # Threads stays PRIVATE — confined to the .cpp's own thread, never named
    # in a public header.
    PUBLIC  qp_sink_pure zstd::libzstd
    PRIVATE Threads::Threads
)

if(BUILD_TESTING)
    add_executable(qp_sink_integration_tests tests/test_file_recorder_integration.cpp)
    target_link_libraries(qp_sink_integration_tests
        PRIVATE qp_sink GTest::gtest GTest::gtest_main
    )
    add_test(NAME qp_sink_integration_tests COMMAND qp_sink_integration_tests)
endif()
