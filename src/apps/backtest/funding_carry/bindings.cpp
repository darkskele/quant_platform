#include <pybind11/pybind11.h>

#include <memory>
#include <string>

#include "basic_risk_gate.hpp"
#include "funding_carry/funding_carry_backtest.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "subscription.hpp"

namespace py = pybind11;

namespace {

using Matcher     = qp::execution::sim::matcher::last_trade::LastTradeMatcher;
using Risk        = qp::risk::basic::BasicRiskGate;
using RiskConfig  = qp::risk::basic::BasicRiskGateConfig;
using Backtest    = qp::backtest::funding_carry::FundingCarryBacktest<Matcher, Risk, RiskConfig>;
using CarryConfig = qp::strategy::carry::Config;
using Results     = qp::backtest::funding_carry::Results;
using EquityPoint = qp::engine::EquityPoint;

}  // namespace

PYBIND11_MODULE(qp_binance_funding_carry, m) {
    m.doc() = "Funding-carry backtest.";

    py::enum_<qp::ExchangeId>(m, "ExchangeId").value("Binance", qp::ExchangeId::Binance);

    py::class_<CarryConfig>(m, "CarryConfig")
        .def(py::init<>())
        .def_readwrite("target_qty", &CarryConfig::target_qty)
        .def_readwrite("entry_funding_rate", &CarryConfig::entry_funding_rate)
        .def_readwrite("exit_funding_rate", &CarryConfig::exit_funding_rate);

    py::class_<RiskConfig>(m, "RiskConfig")
        .def(py::init<>())
        .def_readwrite("max_position_qty", &RiskConfig::max_position_qty)
        .def_readwrite("max_drawdown", &RiskConfig::max_drawdown);

    py::class_<EquityPoint>(m, "EquityPoint")
        .def_readonly("ts", &EquityPoint::ts)
        .def_readonly("equity", &EquityPoint::equity);

    py::class_<Results>(m, "Results")
        .def_readonly("final_cash", &Results::final_cash)
        .def_readonly("final_equity", &Results::final_equity)
        .def_readonly("final_spot_position", &Results::final_spot_position)
        .def_readonly("final_futures_position", &Results::final_futures_position);

    py::class_<qp::Subscription::Instrument>(m, "Instrument")
        .def_readonly("exchange", &qp::Subscription::Instrument::exchange)
        .def_readonly("market", &qp::Subscription::Instrument::market)
        .def_readonly("symbol", &qp::Subscription::Instrument::symbol);

    py::class_<qp::Subscription>(m, "Subscription")
        .def("resolve", &qp::Subscription::resolve, py::arg("exchange"), py::arg("market"),
             py::arg("symbol"));

    py::class_<qp::SubscriptionBuilder>(m, "SubscriptionBuilder")
        .def(py::init<>())
        .def("add", &qp::SubscriptionBuilder::add, py::arg("exchange"), py::arg("market"),
             py::arg("symbol"))
        .def("build", [](qp::SubscriptionBuilder& self) { return std::move(self).build(); });

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
