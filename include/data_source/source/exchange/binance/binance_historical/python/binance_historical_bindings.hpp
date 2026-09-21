#pragma once
#include <pybind11/pybind11.h>

namespace qp::python {

/// Registers the Binance historical source on m: its endpoints and cadences,
/// stream specs and config, the fetch pool's config and stats, the per stream
/// gap reports, and the source itself as an iterable of market events.
void bind_binance_historical(pybind11::module_& m);

}  // namespace qp::python
