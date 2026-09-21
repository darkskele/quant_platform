#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <vector>

#include "binance_historical_bindings.hpp"
#include "core_bindings.hpp"
#include "engine_bindings.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "python/python_binance_historical_backtest.hpp"
#include "risk_bindings.hpp"
#include "types.hpp"

namespace py = pybind11;

namespace {

using Matcher        = qp::execution::sim::matcher::last_trade::LastTradeMatcher;
using PythonBacktest = qp::backtest::python::PythonBinanceHistoricalBacktest<Matcher>;
using Results        = PythonBacktest::Results;
using EquityPoint    = qp::engine::EquityPoint;

}  // namespace

PYBIND11_MODULE(qp_python_backtest, m) {
    m.doc() = "Backtest with strategy and risk gate driven by python callbacks.";

    qp::python::bind_core(m);
    qp::python::bind_risk(m);
    qp::python::bind_engine(m);
    qp::python::bind_binance_historical(m);

    py::class_<Results>(m, "Results")
        .def_readonly("final_cash", &Results::final_cash)
        .def_readonly("final_equity", &Results::final_equity)
        .def_readonly("instruments", &Results::instruments)
        .def_readonly("fees", &Results::fees)
        .def_readonly("funding_received", &Results::funding_received)
        .def_readonly("funding_paid", &Results::funding_paid)
        .def_readonly("basis_pnl", &Results::basis_pnl)
        .def_property_readonly("equity_series", [](const Results& r) {
            return std::vector<EquityPoint>(r.equity_series.begin(), r.equity_series.end());
        });

    py::class_<PythonBacktest>(m, "PythonBacktest")
        .def(py::init([](qp::Subscription subscription, PythonBacktest::Config config) {
                 return std::make_unique<PythonBacktest>(
                     std::move(subscription), std::move(config),
                     [](qp::Portfolio& book, const qp::Subscription&) { return Matcher{book}; });
             }),
             py::arg("subscription"), py::arg("config"))
        .def("plan", &PythonBacktest::plan, py::call_guard<py::gil_scoped_release>())
        .def("stream_count", &PythonBacktest::stream_count)
        .def("reports", &PythonBacktest::reports)
        .def("fetch_stats", &PythonBacktest::fetch_stats)
        .def("fetch_failures", &PythonBacktest::fetch_failures)
        // The engine and source threads call back into Python and take
        // the GIL themselves, so holding it here would deadlock them.
        .def("run", &PythonBacktest::run, py::call_guard<py::gil_scoped_release>())
        .def("set_on_event", &PythonBacktest::set_on_event, py::arg("cb"))
        .def("set_on_timer", &PythonBacktest::set_on_timer, py::arg("cb"))
        .def("set_check", &PythonBacktest::set_check, py::arg("cb"))
        .def("set_on_tick", &PythonBacktest::set_on_tick, py::arg("cb"))
        .def("set_timer_period", &PythonBacktest::set_timer_period, py::arg("period_ns"))
        .def("set_event_kinds", &PythonBacktest::set_event_kinds, py::arg("kinds"))
        .def("equity", &PythonBacktest::equity)
        .def("cash", &PythonBacktest::cash)
        .def("position", &PythonBacktest::position, py::arg("exchange"), py::arg("market"),
             py::arg("symbol"))
        .def("mark", &PythonBacktest::mark, py::arg("exchange"), py::arg("market"),
             py::arg("symbol"))
        .def("results", &PythonBacktest::results);
}
