# integration — end to end tests against the live bucket.
qp_dir(tests QP_TESTS)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Threads REQUIRED)

    # Not registered with ctest. It fetches real files and takes minutes, so it
    # is run on purpose rather than with the fast suite.
    file(GLOB QP_INTEGRATION_SRCS CONFIGURE_DEPENDS ${QP_TESTS}/test_*.cpp)
    add_executable(qp_integration_tests ${QP_INTEGRATION_SRCS})
    target_link_libraries(qp_integration_tests
        PRIVATE qp_core qp_binance_historical qp_network_http qp_archive_zip qp_data_source
                Threads::Threads GTest::gtest GTest::gtest_main
    )

    # The python surface end to end, in the same binary when the bindings are
    # built.
    if(QP_BUILD_PYTHON)
        target_sources(qp_integration_tests
            PRIVATE ${CMAKE_SOURCE_DIR}/tests/python/test_python_backtest_integration.cpp
        )
        target_include_directories(qp_integration_tests
            PRIVATE ${CMAKE_SOURCE_DIR}/include/apps/backtest
        )
        target_link_libraries(qp_integration_tests
            PRIVATE qp_engine qp_transport qp_transport_backtest_in_process qp_execution_sim
                    qp_execution_sim_matcher_last_trade qp_sink_fanout
                    qp_risk_python qp_strategy_python pybind11::embed
        )
        # pybind's types are hidden, so anything holding a py::object has to be
        # too, or every such class warns.
        set_target_properties(qp_integration_tests PROPERTIES CXX_VISIBILITY_PRESET hidden)
    endif()
endif()
