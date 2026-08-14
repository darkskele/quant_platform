# core — the lingua franca. Header-only. Depends on nothing.
add_library(qp_core INTERFACE)
target_include_directories(qp_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(qp_core INTERFACE cxx_std_20)
