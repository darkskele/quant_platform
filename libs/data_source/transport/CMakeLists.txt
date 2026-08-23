# transport — the Transport concept + InProcessTransport/CombinedTransport
# (see libs/data_source/transport/include). Header-only, depends on core
# only (MarketEvent, SpmcRing). New top-level city under data_source,
# sibling to source/sink/wire — not nested under source, since Transport is
# the seam Engine consumes, independent of how a source produces events.

add_library(qp_transport INTERFACE)
target_include_directories(qp_transport INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_transport INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_transport_tests
        tests/test_transport.cpp
        tests/test_in_process_transport.cpp
        tests/test_combined_transport.cpp
    )
    target_link_libraries(qp_transport_tests PRIVATE qp_transport GTest::gtest GTest::gtest_main)
    qp_register_test(qp_transport_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_transport_bench benchmarks/bench_transport.cpp)
    target_link_libraries(qp_transport_bench PRIVATE qp_transport benchmark::benchmark_main)
endif()
