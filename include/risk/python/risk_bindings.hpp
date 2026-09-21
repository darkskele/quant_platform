#pragma once
#include <pybind11/pybind11.h>

namespace qp::python {

/// Registers the risk gate's outcome and decision on m.
void bind_risk(pybind11::module_& m);

}  // namespace qp::python
