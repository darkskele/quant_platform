#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "matcher/last_trade/last_trade_matcher.hpp"
#include "python/python_backtest.hpp"
#include "types.hpp"

namespace py = pybind11;

namespace {

using Matcher        = qp::execution::sim::matcher::last_trade::LastTradeMatcher;
using PythonBacktest = qp::backtest::python::PythonBacktest<Matcher>;
using Results        = PythonBacktest::Results;
using EquityPoint    = qp::engine::EquityPoint;

}  // namespace

PYBIND11_MODULE(qp_python_backtest, m) {
    m.doc() = "Backtest with strategy and risk gate driven by python callbacks.";

    py::enum_<qp::ExchangeId>(m, "ExchangeId").value("Binance", qp::ExchangeId::Binance);

    py::enum_<qp::Side>(m, "Side").value("Buy", qp::Side::Buy).value("Sell", qp::Side::Sell);

    py::enum_<qp::risk::RiskOutcome>(m, "RiskOutcome")
        .value("Approved", qp::risk::RiskOutcome::Approved)
        .value("Resized", qp::risk::RiskOutcome::Resized)
        .value("Rejected", qp::risk::RiskOutcome::Rejected);

    py::enum_<qp::EventKind>(m, "EventKind")
        .value("BookDiff", qp::EventKind::BookDiff)
        .value("Trade", qp::EventKind::Trade)
        .value("Funding", qp::EventKind::Funding)
        .value("BookSnapshot", qp::EventKind::BookSnapshot)
        .value("Kline", qp::EventKind::Kline)
        .value("MarkPriceKline", qp::EventKind::MarkPriceKline);

    py::class_<qp::PriceLevel>(m, "PriceLevel")
        .def_readonly("price", &qp::PriceLevel::price)
        .def_readonly("qty", &qp::PriceLevel::qty);

    py::class_<qp::BookLevels, std::shared_ptr<qp::BookLevels>>(m, "BookLevels")
        .def_readonly("bids", &qp::BookLevels::bids)
        .def_readonly("asks", &qp::BookLevels::asks);

    py::class_<qp::TradeEvent>(m, "TradeEvent")
        .def_readonly("side", &qp::TradeEvent::side)
        .def_readonly("price", &qp::TradeEvent::price)
        .def_readonly("qty", &qp::TradeEvent::qty);

    py::class_<qp::FundingEvent>(m, "FundingEvent")
        .def_readonly("funding_rate", &qp::FundingEvent::funding_rate);

    py::class_<qp::KlineEvent>(m, "KlineEvent")
        .def_readonly("close_time", &qp::KlineEvent::close_time)
        .def_readonly("open", &qp::KlineEvent::open)
        .def_readonly("high", &qp::KlineEvent::high)
        .def_readonly("low", &qp::KlineEvent::low)
        .def_readonly("close", &qp::KlineEvent::close)
        .def_readonly("volume", &qp::KlineEvent::volume);

    py::class_<qp::MarkPriceKlineEvent>(m, "MarkPriceKlineEvent")
        .def_readonly("close_time", &qp::MarkPriceKlineEvent::close_time)
        .def_readonly("open", &qp::MarkPriceKlineEvent::open)
        .def_readonly("high", &qp::MarkPriceKlineEvent::high)
        .def_readonly("low", &qp::MarkPriceKlineEvent::low)
        .def_readonly("close", &qp::MarkPriceKlineEvent::close);

    py::class_<qp::BookDiffEvent>(m, "BookDiffEvent")
        .def_readonly("first_seq", &qp::BookDiffEvent::first_seq)
        .def_readonly("seq", &qp::BookDiffEvent::seq)
        .def_readonly("prev_seq", &qp::BookDiffEvent::prev_seq)
        .def_readonly("levels", &qp::BookDiffEvent::levels);

    py::class_<qp::BookSnapshotEvent>(m, "BookSnapshotEvent")
        .def_readonly("levels", &qp::BookSnapshotEvent::levels);

    py::class_<qp::EventBase>(m, "EventBase")
        .def_readonly("kind", &qp::EventBase::kind)
        .def_readonly("exchange", &qp::EventBase::exchange)
        .def_readonly("market", &qp::EventBase::market)
        .def_readonly("symbol", &qp::EventBase::symbol)
        .def_readonly("ts", &qp::EventBase::ts);

    py::class_<qp::MarketEvent>(m, "MarketEvent")
        .def_readonly("base", &qp::MarketEvent::base)
        .def_property_readonly("payload", [](const qp::MarketEvent& e) -> py::object {
            return std::visit([](const auto& p) { return py::cast(p); }, e.payload);
        });

    py::class_<qp::Intent>(m, "Intent")
        .def(py::init([](std::uint16_t ex, std::uint16_t mk, std::uint16_t sy, qp::Qty q) {
                 return qp::Intent{
                     .exchange = ex, .market = mk, .symbol = sy, .target_position = q};
             }),
             py::arg("exchange"), py::arg("market"), py::arg("symbol"), py::arg("target_position"))
        .def_readwrite("exchange", &qp::Intent::exchange)
        .def_readwrite("market", &qp::Intent::market)
        .def_readwrite("symbol", &qp::Intent::symbol)
        .def_readwrite("target_position", &qp::Intent::target_position);

    py::class_<qp::Order>(m, "Order")
        .def(py::init([](qp::OrderId id, std::uint16_t ex, std::uint16_t mk, std::uint16_t sy,
                         qp::Side side, qp::Qty q) {
                 return qp::Order{
                     .id = id, .exchange = ex, .market = mk, .symbol = sy, .side = side, .qty = q};
             }),
             py::arg("id"), py::arg("exchange"), py::arg("market"), py::arg("symbol"),
             py::arg("side"), py::arg("qty"))
        .def_readwrite("id", &qp::Order::id)
        .def_readwrite("exchange", &qp::Order::exchange)
        .def_readwrite("market", &qp::Order::market)
        .def_readwrite("symbol", &qp::Order::symbol)
        .def_readwrite("side", &qp::Order::side)
        .def_readwrite("qty", &qp::Order::qty);

    py::class_<qp::risk::RiskDecision>(m, "RiskDecision")
        .def(py::init([](qp::risk::RiskOutcome oc, std::optional<qp::Order> ord) {
                 return qp::risk::RiskDecision{.outcome = oc, .order = ord};
             }),
             py::arg("outcome"), py::arg("order") = std::nullopt)
        .def_readwrite("outcome", &qp::risk::RiskDecision::outcome)
        .def_readwrite("order", &qp::risk::RiskDecision::order);

    py::class_<EquityPoint>(m, "EquityPoint")
        .def_readonly("ts", &EquityPoint::ts)
        .def_readonly("equity", &EquityPoint::equity);

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
        .def(py::init([](qp::Subscription subscription) {
                 return PythonBacktest(
                     std::move(subscription),
                     [](qp::Portfolio& book, const qp::Subscription&) { return Matcher{book}; });
             }),
             py::arg("subscription"))
        .def("run", &PythonBacktest::run)
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
