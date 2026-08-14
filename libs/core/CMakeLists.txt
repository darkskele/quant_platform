# core — the lingua franca. Header-only. Depends on nothing.
add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(qp_core INTERFACE cxx_std_20)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(qp_core_tests tests/test_spsc_queue.cpp)
    target_link_libraries(qp_core_tests PRIVATE qp_core Threads::Threads GTest::gtest GTest::gtest_main)
    add_test(NAME qp_core_tests COMMAND qp_core_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    # googlebenchmark is fetched once, at the top level.
    add_executable(qp_core_bench benchmarks/bench_spsc_queue.cpp)
    target_link_libraries(qp_core_bench PRIVATE qp_core benchmark::benchmark)
endif()
