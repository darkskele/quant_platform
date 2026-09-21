#include "basic_risk_bindings.hpp"

#include "basic_risk_gate.hpp"

namespace py = pybind11;

namespace qp::python {

void bind_basic_risk(py::module_& m) {
    using Config = risk::basic::BasicRiskGateConfig;
    py::class_<Config>(m, "RiskConfig")
        .def(py::init<>())
        .def_readwrite("max_position_qty", &Config::max_position_qty)
        .def_readwrite("max_drawdown", &Config::max_drawdown);
}

}  // namespace qp::python
