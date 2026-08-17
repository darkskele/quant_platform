# sink — the "sinks" city (see docs/DESIGN.md): FileRecorder, the one Sink
# implementation. The on-disk wire format itself (encode/decode, zstd
# codec, day/segment naming) lives in libs/data_source/wire, shared with
# FileReplaySource's read side — see docs/decisions.md D12/D19.

find_package(Threads REQUIRED)

add_library(qp_sink src/file_recorder.cpp)
target_link_libraries(qp_sink
    # qp_wire is PUBLIC, not PRIVATE: file_recorder.hpp (the public header)
    # names wire::DayKey/wire::ZstdCompressor directly in the class
    # interface, so consumers transitively need wire's include path too.
    # Threads stays PRIVATE — confined to the .cpp's own thread, never named
    # in a public header.
    PUBLIC  qp_core qp_wire
    PRIVATE Threads::Threads
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_sink_integration_tests tests/test_file_recorder_integration.cpp)
    target_link_libraries(qp_sink_integration_tests
        PRIVATE qp_sink GTest::gtest GTest::gtest_main
    )
    add_test(NAME qp_sink_integration_tests COMMAND qp_sink_integration_tests)
endif()
