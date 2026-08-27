# wire — the on-disk MarketEvent codec (see docs/data_source/wire/DESIGN.md).
# Pure, no file I/O: wire.hpp (bytes <-> MarketEvent), zstd_stream.hpp (bytes
# <-> zstd bytes), partition.hpp (day/segment naming). Substrate shared by
# `source` (FileReplaySource, read side) and `sink` (FileRecorder, write
# side) — see docs/decisions.md D12/D19 — so both point down at this instead
# of one depending on the other.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

find_package(zstd CONFIG REQUIRED)

add_library(qp_wire INTERFACE)
target_include_directories(qp_wire INTERFACE ${QP_INC})
# zstd::libzstd is PUBLIC (interface libraries only have PUBLIC/INTERFACE,
# which are equivalent here): zstd_stream.hpp names ZSTD_CCtx/ZSTD_DCtx/
# ZSTD_EndDirective directly, so every consumer transitively needs zstd's
# include path too — same PUBLIC-vs-PRIVATE reasoning as qp_sink's own link
# line (now inherited from here instead of declared twice).
target_link_libraries(qp_wire INTERFACE qp_core zstd::libzstd)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_wire_tests
        ${QP_TESTS}/test_wire.cpp
        ${QP_TESTS}/test_partition.cpp
        ${QP_TESTS}/test_zstd_stream.cpp
    )
    target_link_libraries(qp_wire_tests PRIVATE qp_wire qp_core_test_support GTest::gtest GTest::gtest_main)
    qp_register_test(qp_wire_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_wire_bench ${QP_BENCH}/bench_wire.cpp)
    target_link_libraries(qp_wire_bench PRIVATE qp_wire benchmark::benchmark_main)
endif()
