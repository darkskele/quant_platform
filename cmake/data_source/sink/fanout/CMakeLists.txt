# sink/fanout — FanoutSink.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_sink_fanout INTERFACE)
target_include_directories(qp_sink_fanout INTERFACE ${QP_INC})
target_link_libraries(qp_sink_fanout INTERFACE qp_sink)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_sink_fanout_tests ${QP_TESTS}/test_fanout_sink.cpp)
    target_link_libraries(qp_sink_fanout_tests PRIVATE qp_sink_fanout GTest::gtest GTest::gtest_main)
    qp_register_test(qp_sink_fanout_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_sink_fanout_bench ${QP_BENCH}/bench_fanout_sink.cpp)
    target_link_libraries(qp_sink_fanout_bench PRIVATE qp_sink_fanout benchmark::benchmark_main)
endif()
