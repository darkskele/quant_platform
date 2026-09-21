#pragma once
#include <pybind11/pybind11.h>

namespace qp::python {

/// Registers BasicRiskGate's config on m.
void bind_basic_risk(pybind11::module_& m);

}  // namespace qp::python
