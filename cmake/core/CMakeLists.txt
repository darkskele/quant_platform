# core.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${QP_INC})
target_compile_features(qp_core INTERFACE cxx_std_23)
find_package(Threads REQUIRED)
target_link_libraries(qp_core INTERFACE Threads::Threads)

# Test/benchmark-only support.
add_library(qp_core_test_support INTERFACE)
target_include_directories(qp_core_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    # Every test_*.cpp in this module. CONFIGURE_DEPENDS re-globs at build
    # time, so a new file needs no edit here.
    file(GLOB QP_CORE_TESTS CONFIGURE_DEPENDS ${QP_TESTS}/test_*.cpp)
    add_executable(qp_core_tests ${QP_CORE_TESTS})
    target_link_libraries(qp_core_tests PRIVATE qp_core qp_core_test_support Threads::Threads GTest::gtest
                                                 GTest::gtest_main)
    qp_register_test(qp_core_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # googlebenchmark is fetched once, at the top level.
    file(GLOB QP_CORE_BENCH CONFIGURE_DEPENDS ${QP_BENCH}/bench_*.cpp)
    add_executable(qp_core_bench ${QP_CORE_BENCH})
    target_link_libraries(qp_core_bench PRIVATE qp_core benchmark::benchmark_main)
endif()
