# strategy — the Strategy concept
qp_dir(include QP_INC)

add_library(qp_strategy INTERFACE)
target_include_directories(qp_strategy INTERFACE ${QP_INC})
target_link_libraries(qp_strategy INTERFACE qp_core)

# Test doubles (NoopStrategy, AlwaysIntentStrategy) shared across strategy's
# own tests and engine's tests/benchmarks.
add_library(qp_strategy_test_support INTERFACE)
target_include_directories(qp_strategy_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(qp_strategy_test_support INTERFACE qp_strategy)

add_subdirectory(carry)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
