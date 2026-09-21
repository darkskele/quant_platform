#pragma once
#include <pybind11/pybind11.h>

namespace qp::python {

/// Registers core's types on m: the exchange and side enums, every event
/// payload, the event envelope, intents, orders and the subscription. Every
/// other level's bindings refer to these, so this runs first.
void bind_core(pybind11::module_& m);

}  // namespace qp::python
