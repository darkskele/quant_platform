# engine — the Engine<Tx,Clk,Exec,Risk,NumWorkers,Strategies...> composition
# root (see libs/engine/docs/DESIGN.md; docs/decisions.md D27, D33). Header-
# only, depends on the seam concepts only — never a concrete adapter.
# qp_source_pure, not qp_source: Engine only needs the Source concept, not
# GenericLiveWebSocketSource's Boost/OpenSSL dependency.

add_library(qp_engine INTERFACE)
target_include_directories(qp_engine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_engine INTERFACE qp_core qp_clock qp_execution qp_strategy qp_risk qp_source_pure)

# InfiniteTransport — Engine-specific (never-exhausts Source double), shared
# between engine's own tests and benchmarks. Unconditional, same convention
# as qp_core_test_support.
add_library(qp_engine_test_support INTERFACE)
target_include_directories(qp_engine_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_engine_tests tests/test_engine.cpp)
    target_link_libraries(qp_engine_tests PRIVATE qp_engine qp_core_test_support qp_strategy_test_support
                                                   qp_risk_test_support qp_engine_test_support GTest::gtest
                                                   GTest::gtest_main)
    qp_register_test(qp_engine_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_engine_bench benchmarks/bench_engine.cpp)
    target_link_libraries(qp_engine_bench PRIVATE qp_engine qp_core_test_support qp_strategy_test_support
                                                   qp_risk_test_support qp_engine_test_support
                                                   benchmark::benchmark_main)
endif()
