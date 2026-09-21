#include <pybind11/pybind11.h>

#include <memory>
#include <string>

#include "basic_risk_bindings.hpp"
#include "basic_risk_gate.hpp"
#include "carry_bindings.hpp"
#include "core_bindings.hpp"
#include "engine_bindings.hpp"
#include "funding_carry/funding_carry_backtest.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "subscription.hpp"

namespace py = pybind11;

namespace {

using Matcher    = qp::execution::sim::matcher::last_trade::LastTradeMatcher;
using Risk       = qp::risk::basic::BasicRiskGate;
using RiskConfig = qp::risk::basic::BasicRiskGateConfig;
using Backtest   = qp::backtest::funding_carry::FundingCarryBacktest<Matcher, Risk, RiskConfig>;
using Results    = qp::backtest::funding_carry::Results;

}  // namespace

PYBIND11_MODULE(qp_binance_funding_carry, m) {
    m.doc() = "Funding-carry backtest.";

    qp::python::bind_core(m);
    qp::python::bind_engine(m);
    qp::python::bind_basic_risk(m);
    qp::python::bind_carry(m);

    py::class_<Results>(m, "Results")
        .def_readonly("final_cash", &Results::final_cash)
        .def_readonly("final_equity", &Results::final_equity)
        .def_readonly("final_spot_position", &Results::final_spot_position)
        .def_readonly("final_futures_position", &Results::final_futures_position);

    py::class_<Backtest>(m, "FundingCarryBacktest")
        .def(py::init([](qp::Subscription subscription, qp::Subscription::Instrument futures_leg,
                         qp::Subscription::Instrument spot_leg) {
                 // Built in place, since the equity collector's drain thread
                 // holds its address and the backtest cannot move.
                 return std::make_unique<Backtest>(
                     std::move(subscription), futures_leg, spot_leg,
                     [](qp::Portfolio& book, const qp::Subscription&) { return Matcher{book}; });
             }),
             py::arg("subscription"), py::arg("futures_leg"), py::arg("spot_leg"))
        .def("set_carry_config", &Backtest::set_carry_config, py::arg("carry"))
        .def("set_risk_config", &Backtest::set_risk_config, py::arg("risk"))
        .def("run", &Backtest::run)
        .def("results", &Backtest::results);
}
