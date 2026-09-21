# core.
qp_dir(include QP_INC)

add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${QP_INC})
target_compile_features(qp_core INTERFACE cxx_std_23)
find_package(Threads REQUIRED)
target_link_libraries(qp_core INTERFACE Threads::Threads)

# Test/benchmark-only support.
add_library(qp_core_test_support INTERFACE)
target_include_directories(qp_core_test_support INTERFACE ${CMAKE_SOURCE_DIR}/tests)
