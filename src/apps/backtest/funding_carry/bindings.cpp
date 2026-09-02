#include <pybind11/pybind11.h>

#include "funding_carry/funding_carry_backtest.hpp"

namespace py = pybind11;

// Only the runtime-swept knobs are exposed. Symbol, dataset, date range,
// and component types are compile-time, baked in by the target's defines.
PYBIND11_MODULE(qp_backtest, m) {
    m.doc() =
        "Funding-carry backtest: set the runtime Config, run, get Results with an equity series.";

    using CarryConfig = qp::strategy::carry::Config;
    py::class_<CarryConfig>(m, "CarryConfig")
        .def(py::init<>())
        .def_readwrite("target_qty", &CarryConfig::target_qty)
        .def_readwrite("entry_funding_rate", &CarryConfig::entry_funding_rate)
        .def_readwrite("exit_funding_rate", &CarryConfig::exit_funding_rate);

    using RiskConfig = qp::risk::basic::BasicRiskGateConfig;
    py::class_<RiskConfig>(m, "RiskConfig")
        .def(py::init<>())
        .def_readwrite("max_position_qty", &RiskConfig::max_position_qty)
        .def_readwrite("max_drawdown", &RiskConfig::max_drawdown);

    py::class_<qp::engine::EquityPoint>(m, "EquityPoint")
        .def_readonly("ts", &qp::engine::EquityPoint::ts)
        .def_readonly("equity", &qp::engine::EquityPoint::equity);

    m.def(
        "run",
        [](CarryConfig carry, RiskConfig risk) {
            qp::backtest::funding_carry::FundingCarryBacktest bt;
            bt.set_carry_config(carry);
            bt.set_risk_config(risk);
            auto r = bt.run();

            // Materialize the series into Python-owned memory while bt (and
            // its pool) is still alive; the span would dangle afterward.
            py::list series;
            for (const auto& p : r.equity_series) series.append(p);

            py::dict out;
            out["final_cash"]             = r.final_cash;
            out["final_equity"]           = r.final_equity;
            out["final_spot_position"]    = r.final_spot_position;
            out["final_futures_position"] = r.final_futures_position;
            out["equity_series"]          = series;
            return out;
        },
        py::arg("carry") = CarryConfig{}, py::arg("risk") = RiskConfig{},
        "Run the funding-carry backtest against the compiled dataset and return a results dict.");
}
