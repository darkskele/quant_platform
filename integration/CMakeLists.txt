# tests — cross-lib integration/parity tests that can't live inside any
# single lib's own tests/ (see docs/DESIGN.md here): they compose two libs
# that deliberately never depend on each other (source/sink, dependency-
# direction rule, docs/repo-layout.md). Unit and single-lib integration
# tests stay colocated per-lib; this directory is only for proving two
# independently-built pieces actually agree when composed.

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_parity_tests test_recorder_replay_parity.cpp)
    target_link_libraries(qp_parity_tests
        PRIVATE qp_sink qp_file_replay qp_core_test_support GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_parity_tests)
endif()
