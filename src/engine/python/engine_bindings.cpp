#include "engine_bindings.hpp"

#include "recorder/equity_series/equity_series_recorder.hpp"

namespace py = pybind11;

namespace qp::python {

void bind_engine(py::module_& m) {
    using engine::EquityPoint;
    py::class_<EquityPoint>(m, "EquityPoint")
        .def_readonly("ts", &EquityPoint::ts)
        .def_readonly("equity", &EquityPoint::equity);
}

}  // namespace qp::python
