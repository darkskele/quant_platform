#include "carry_bindings.hpp"

#include "funding_carry_strategy.hpp"

namespace py = pybind11;

namespace qp::python {

void bind_carry(py::module_& m) {
    using Config = strategy::carry::Config;
    py::class_<Config>(m, "CarryConfig")
        .def(py::init<>())
        .def_readwrite("target_qty", &Config::target_qty)
        .def_readwrite("entry_funding_rate", &Config::entry_funding_rate)
        .def_readwrite("exit_funding_rate", &Config::exit_funding_rate);
}

}  // namespace qp::python
