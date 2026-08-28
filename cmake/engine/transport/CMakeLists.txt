# transport — the Transport concept + BacktestInProcessTransport (see
# docs/engine/transport/DESIGN.md). Header-only, depends on core only
# (MarketEvent, SpmcRing, ControlChannel). Lives under engine, not
# data_source: Transport is the seam Engine consumes, not a data-source
# concern in its own right. BacktestInProcessTransport merges a fixed set
# of rings in MarketEvent::ts order — backtest's own policy, always used
# this way, backtest and live alike: Source(s) -> run_data_source (its own
# thread) -> FanoutSink(s) -> rings -> BacktestInProcessTransport ->
# Engine, so a backtest run exercises the identical queue-polling code a
# live run depends on.
#
# CombinedTransport (round-robin merge over N heterogeneous Transports)
# removed: no real caller ever used it (apps/backtest uses
# BacktestInProcessTransport; apps/collector has no Transport-consuming
# side at all) — built ahead of a live multi-source composition that
# hasn't been built yet. Rebuild it if/when that composition actually
# needs round-robin merging, not before.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_transport INTERFACE)
target_include_directories(qp_transport INTERFACE ${QP_INC})
target_link_libraries(qp_transport INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_transport_tests
        ${QP_TESTS}/test_transport.cpp
        ${QP_TESTS}/test_backtest_in_process_transport.cpp
    )
    target_link_libraries(qp_transport_tests PRIVATE qp_transport GTest::gtest GTest::gtest_main)
    qp_register_test(qp_transport_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_transport_bench ${QP_BENCH}/bench_backtest_in_process_transport.cpp)
    target_link_libraries(qp_transport_bench PRIVATE qp_transport benchmark::benchmark_main)
endif()
