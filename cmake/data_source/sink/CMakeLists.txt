# sink — the "sinks" city (see docs/DESIGN.md): FileRecorder, the one
# persistence Sink implementation, and FanoutSink, the in-process fan-out
# one. The on-disk wire format itself (encode/decode, zstd codec,
# day/segment naming) lives in data_source/wire, shared with
# FileReplaySource's read side — see docs/decisions.md D12/D19.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)
qp_dir(tests QP_TESTS)

find_package(Threads REQUIRED)

add_library(qp_sink ${QP_SRC}/file_recorder.cpp)
target_include_directories(qp_sink PUBLIC ${QP_INC})
target_link_libraries(qp_sink
    # qp_wire is PUBLIC, not PRIVATE: file_recorder.hpp (the public header)
    # names wire::DayKey/wire::ZstdCompressor directly in the class
    # interface, so consumers transitively need wire's include path too.
    # Threads stays PRIVATE — confined to file_recorder.cpp's own thread,
    # never named in a public header.
    PUBLIC  qp_core qp_wire
    PRIVATE Threads::Threads
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_sink_integration_tests ${QP_TESTS}/test_file_recorder_integration.cpp)
    target_link_libraries(qp_sink_integration_tests
        PRIVATE qp_sink qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_sink_integration_tests)

    add_executable(qp_sink_tests ${QP_TESTS}/test_fanout_sink.cpp)
    target_link_libraries(qp_sink_tests PRIVATE qp_sink GTest::gtest GTest::gtest_main)
    qp_register_test(qp_sink_tests)
endif()
