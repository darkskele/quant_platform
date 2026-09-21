#pragma once
#include <pybind11/pybind11.h>

namespace qp::python {

/// Registers what the engine hands back, the equity series points, on m.
void bind_engine(pybind11::module_& m);

}  // namespace qp::python
