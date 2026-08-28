# data_source's own shared header, not a city: run_data_source.hpp (D44)
# pairs N Sources with N Sinks positionally and stamps MarketEvent::venue as
# it does. Depends on both source::Source and sink::Sink, which is exactly
# why it can't live inside either city — neither depends on the other
# (D19), so the thing that bridges them sits at their parent level instead.
# Header-only.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_data_source INTERFACE)
target_include_directories(qp_data_source INTERFACE ${QP_INC})
target_link_libraries(qp_data_source INTERFACE qp_source_pure qp_sink)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(qp_data_source_tests ${QP_TESTS}/test_run_data_source.cpp)
    target_link_libraries(qp_data_source_tests
        PRIVATE qp_data_source Threads::Threads GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_data_source_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_data_source_bench ${QP_BENCH}/bench_run_data_source.cpp)
    target_link_libraries(qp_data_source_bench PRIVATE qp_data_source benchmark::benchmark_main)
endif()
