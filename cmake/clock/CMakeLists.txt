# clock — the Clock concept + SimClock (see docs/core/DESIGN.md; WallClock
# deferred to the live milestone, docs/decisions.md D23). Header-only,
# depends on core only (Timestamp).
#
# No QP_BUILD_BENCHMARKS block: now()/advance() are a single scalar
# load/store, nothing to benchmark (see sim_clock.hpp) — deliberate, not an
# oversight.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)

add_library(qp_clock INTERFACE)
target_include_directories(qp_clock INTERFACE ${QP_INC})
target_link_libraries(qp_clock INTERFACE qp_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_clock_tests ${QP_TESTS}/test_clock.cpp)
    target_link_libraries(qp_clock_tests PRIVATE qp_clock GTest::gtest GTest::gtest_main)
    qp_register_test(qp_clock_tests)
endif()
