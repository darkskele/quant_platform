# python — PythonRiskGate adapter forwarding RiskGate calls to a py::object.
qp_dir(include QP_INC)

add_library(qp_risk_python INTERFACE)
target_include_directories(qp_risk_python INTERFACE ${QP_INC})
target_link_libraries(qp_risk_python INTERFACE qp_risk pybind11::headers)
