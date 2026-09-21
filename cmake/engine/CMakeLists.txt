# engine — the Engine<Tx,Exec,Risk,Strategy,Recorder> composition root.
qp_dir(include QP_INC)

add_subdirectory(transport)

add_library(qp_engine INTERFACE)
target_include_directories(qp_engine INTERFACE ${QP_INC})
target_link_libraries(qp_engine INTERFACE qp_core qp_execution qp_strategy qp_risk qp_transport)

add_library(qp_engine_test_support INTERFACE)
target_include_directories(qp_engine_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
