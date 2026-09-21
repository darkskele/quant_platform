#include "risk_bindings.hpp"

#include <pybind11/stl.h>

#include <optional>

#include "risk_gate.hpp"
#include "types.hpp"

namespace py = pybind11;

namespace qp::python {

void bind_risk(py::module_& m) {
    py::enum_<risk::RiskOutcome>(m, "RiskOutcome")
        .value("Approved", risk::RiskOutcome::Approved)
        .value("Resized", risk::RiskOutcome::Resized)
        .value("Rejected", risk::RiskOutcome::Rejected);

    py::class_<risk::RiskDecision>(m, "RiskDecision")
        .def(py::init([](risk::RiskOutcome oc, std::optional<Order> ord) {
                 return risk::RiskDecision{.outcome = oc, .order = ord};
             }),
             py::arg("outcome"), py::arg("order") = std::nullopt)
        .def_readwrite("outcome", &risk::RiskDecision::outcome)
        .def_readwrite("order", &risk::RiskDecision::order);
}

}  // namespace qp::python
