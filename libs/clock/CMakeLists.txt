# clock — the Clock concept + SimClock (see libs/docs/DESIGN.md; WallClock
# deferred to the live milestone, docs/decisions.md D23). Header-only,
# depends on core only (Timestamp).
#
# No QP_BUILD_BENCHMARKS block: now()/advance() are a single scalar
# load/store, nothing to benchmark (see sim_clock.hpp) — deliberate, not an
# oversight.

add_library(qp_clock INTERFACE)
target_include_directories(qp_clock INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(qp_clock INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_clock_tests tests/test_clock.cpp)
    target_link_libraries(qp_clock_tests PRIVATE qp_clock GTest::gtest GTest::gtest_main)
    qp_register_test(qp_clock_tests)
endif()
