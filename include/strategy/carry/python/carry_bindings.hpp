#pragma once
#include <pybind11/pybind11.h>

namespace qp::python {

/// Registers the funding carry strategy's config on m.
void bind_carry(pybind11::module_& m);

}  // namespace qp::python
