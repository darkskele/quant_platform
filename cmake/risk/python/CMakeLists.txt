# python — PythonRiskGate adapter forwarding RiskGate calls to a py::object.
qp_dir(include QP_INC)

add_library(qp_risk_python INTERFACE)
target_include_directories(qp_risk_python INTERFACE ${QP_INC})
target_link_libraries(qp_risk_python INTERFACE qp_risk pybind11::headers)

qp_dir(src QP_SRC)

# The risk gate's own bindings, separate from the adapter above.
add_library(qp_risk_bindings STATIC ${QP_SRC}/risk_bindings.cpp)
# Linked into the shared python modules, and pybind's types are hidden.
set_target_properties(qp_risk_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)
target_include_directories(qp_risk_bindings PUBLIC ${QP_INC})
target_link_libraries(qp_risk_bindings PUBLIC qp_risk pybind11::pybind11)
