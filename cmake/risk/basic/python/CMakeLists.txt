# python — BasicRiskGate's bindings.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

add_library(qp_risk_basic_bindings STATIC ${QP_SRC}/basic_risk_bindings.cpp)
# Linked into the shared python modules, and pybind's types are hidden.
set_target_properties(qp_risk_basic_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)
target_include_directories(qp_risk_basic_bindings PUBLIC ${QP_INC})
target_link_libraries(qp_risk_basic_bindings PUBLIC qp_risk_basic pybind11::pybind11)
