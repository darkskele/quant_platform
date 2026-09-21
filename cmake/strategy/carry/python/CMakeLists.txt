# python — the funding carry strategy's bindings.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

add_library(qp_strategy_carry_bindings STATIC ${QP_SRC}/carry_bindings.cpp)
# Linked into the shared python modules, and pybind's types are hidden.
set_target_properties(qp_strategy_carry_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)
target_include_directories(qp_strategy_carry_bindings PUBLIC ${QP_INC})
target_link_libraries(qp_strategy_carry_bindings PUBLIC qp_strategy_carry pybind11::pybind11)
