#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "endpoints.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "python/python_binance_historical_backtest.hpp"
#include "types.hpp"

namespace py = pybind11;

namespace {

using Matcher        = qp::execution::sim::matcher::last_trade::LastTradeMatcher;
using PythonBacktest = qp::backtest::python::PythonBinanceHistoricalBacktest<Matcher>;
namespace binance    = qp::data_source::source::venue::binance;
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

    py::enum_<binance::EndpointKind>(m, "EndpointKind")
        .value("Klines", binance::EndpointKind::Klines)
        .value("MarkPriceKlines", binance::EndpointKind::MarkPriceKlines)
        .value("PremiumIndexKlines", binance::EndpointKind::PremiumIndexKlines)
        .value("FundingRate", binance::EndpointKind::FundingRate);

    py::enum_<binance::Cadence>(m, "Cadence")
        .value("Daily", binance::Cadence::Daily)
        .value("Monthly", binance::Cadence::Monthly);

    py::enum_<binance::BinanceMarket>(m, "BinanceMarket")
        .value("Spot", binance::BinanceMarket::Spot)
        .value("UsdM", binance::BinanceMarket::UsdM)
        .value("CoinM", binance::BinanceMarket::CoinM);

    py::class_<binance::StreamSpec>(m, "StreamSpec")
        .def(py::init([](binance::EndpointKind kind, std::string interval) {
                 return binance::StreamSpec{kind, std::move(interval)};
             }),
             py::arg("kind"), py::arg("interval") = std::string{})
        .def_readwrite("kind", &binance::StreamSpec::kind)
        .def_readwrite("interval", &binance::StreamSpec::interval);

    py::class_<binance::GapStats>(m, "GapStats")
        .def_readonly("files_planned", &binance::GapStats::files_planned)
        .def_readonly("files_read", &binance::GapStats::files_read)
        .def_readonly("files_failed", &binance::GapStats::files_failed)
        .def_readonly("header_rows", &binance::GapStats::header_rows)
        .def_readonly("blank_rows", &binance::GapStats::blank_rows)
        .def_readonly("rows_expected", &binance::GapStats::rows_expected)
        .def_readonly("rows_parsed", &binance::GapStats::rows_parsed)
        .def_readonly("rows_rejected", &binance::GapStats::rows_rejected)
        .def_readonly("backwards_stamps", &binance::GapStats::backwards_stamps);

    py::class_<binance::StreamReport>(m, "StreamReport")
        .def_readonly("market", &binance::StreamReport::market)
        .def_readonly("symbol", &binance::StreamReport::symbol)
        .def_readonly("kind", &binance::StreamReport::kind)
        .def_readonly("interval", &binance::StreamReport::interval)
        .def_readonly("stats", &binance::StreamReport::stats);

    py::class_<binance::FetchPoolStats>(m, "FetchPoolStats")
        .def_readonly("submitted", &binance::FetchPoolStats::submitted)
        .def_readonly("completed_ok", &binance::FetchPoolStats::completed_ok)
        .def_readonly("completed_failed", &binance::FetchPoolStats::completed_failed)
        .def_readonly("not_found", &binance::FetchPoolStats::not_found)
        .def_readonly("retries", &binance::FetchPoolStats::retries)
        .def_readonly("bytes_fetched", &binance::FetchPoolStats::bytes_fetched)
        .def_readonly("bytes_inflated", &binance::FetchPoolStats::bytes_inflated);

    py::class_<binance::HttpFetchPoolConfig>(m, "FetchPoolConfig")
        .def(py::init<>())
        .def_readwrite("workers", &binance::HttpFetchPoolConfig::workers)
        .def_readwrite("max_retries", &binance::HttpFetchPoolConfig::max_retries);

    py::class_<PythonBacktest::Config>(m, "BinanceHistoricalConfig")
        .def(py::init([](std::vector<binance::StreamSpec> streams, binance::Cadence cadence,
                         qp::Timestamp from, qp::Timestamp to, binance::HttpFetchPoolConfig pool) {
                 return PythonBacktest::Config{std::move(streams), pool, cadence, from, to};
             }),
             py::arg("streams"), py::arg("cadence") = binance::Cadence::Monthly,
             py::arg("from_ns") = 0, py::arg("to_ns") = 0,
             py::arg("pool") = binance::HttpFetchPoolConfig{});

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
