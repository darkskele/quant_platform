# python — core's bindings, which every other level's bindings refer to.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

add_library(qp_core_bindings STATIC ${QP_SRC}/core_bindings.cpp)
# Linked into the shared python modules, and pybind's types are hidden.
set_target_properties(qp_core_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)
target_include_directories(qp_core_bindings PUBLIC ${QP_INC})
target_link_libraries(qp_core_bindings PUBLIC qp_core pybind11::pybind11)
