# python — PythonStrategy adapter forwarding Strategy calls to a py::object.
qp_dir(include QP_INC)

add_library(qp_strategy_python INTERFACE)
target_include_directories(qp_strategy_python INTERFACE ${QP_INC})
target_link_libraries(qp_strategy_python INTERFACE qp_strategy pybind11::headers)
