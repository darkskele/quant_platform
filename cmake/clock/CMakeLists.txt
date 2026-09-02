# clock — the Clock concept.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_clock INTERFACE)
target_include_directories(qp_clock INTERFACE ${QP_INC})
target_link_libraries(qp_clock INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_clock_tests ${QP_TESTS}/test_clock.cpp)
    target_link_libraries(qp_clock_tests PRIVATE qp_clock GTest::gtest GTest::gtest_main)
    qp_register_test(qp_clock_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_clock_bench ${QP_BENCH}/bench_sim_clock.cpp)
    target_link_libraries(qp_clock_bench PRIVATE qp_clock benchmark::benchmark_main)
endif()
