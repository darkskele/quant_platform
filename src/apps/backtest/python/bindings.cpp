#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "config/compose.hpp"
#include "python/python_backtest.hpp"
#include "types.hpp"

namespace py     = pybind11;
namespace config = qp::backtest::config;

namespace {

using PythonBacktest = qp::backtest::python::PythonBacktest;
using Results        = PythonBacktest::Results;
using EquityPoint    = qp::engine::EquityPoint;

// Runtime dataset selection, same shape as the funding-carry module's
// Dataset. Dates are "YYYY-MM-DD"; a bad one fails construction.
struct Dataset {
    std::filesystem::path       data_dir;
    std::vector<std::string>    symbols;
    std::chrono::year_month_day first_day;
    std::chrono::year_month_day last_day;

    Dataset(std::string dir, std::vector<std::string> syms, const std::string& first,
            const std::string& last)
        : data_dir(std::move(dir)), symbols(std::move(syms)) {
        auto f = config::parse_day(first);
        auto l = config::parse_day(last);
        if (!f) throw std::invalid_argument("bad first_day: " + first);
        if (!l) throw std::invalid_argument("bad last_day: " + last);
        first_day = *f, last_day = *l;
    }

    Dataset(std::string dir, const std::string& sym, const std::string& first,
            const std::string& last)
        : Dataset(std::move(dir), std::vector<std::string>{sym}, first, last) {}
};

}  // namespace

PYBIND11_MODULE(qp_python_backtest, m) {
    m.doc() = "Backtest with strategy and risk gate driven by python callbacks.";

    py::enum_<qp::Side>(m, "Side").value("Buy", qp::Side::Buy).value("Sell", qp::Side::Sell);

    py::enum_<qp::RiskOutcome>(m, "RiskOutcome")
        .value("Approved", qp::RiskOutcome::Approved)
        .value("Resized", qp::RiskOutcome::Resized)
        .value("Rejected", qp::RiskOutcome::Rejected);

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
        .def_readonly("kind", &qp::TradeEvent::kind)
        .def_readonly("venue", &qp::TradeEvent::venue)
        .def_readonly("symbol", &qp::TradeEvent::symbol)
        .def_readonly("ts", &qp::TradeEvent::ts)
        .def_readonly("side", &qp::TradeEvent::side)
        .def_readonly("price", &qp::TradeEvent::price)
        .def_readonly("qty", &qp::TradeEvent::qty);

    py::class_<qp::FundingEvent>(m, "FundingEvent")
        .def_readonly("kind", &qp::FundingEvent::kind)
        .def_readonly("venue", &qp::FundingEvent::venue)
        .def_readonly("symbol", &qp::FundingEvent::symbol)
        .def_readonly("ts", &qp::FundingEvent::ts)
        .def_readonly("funding_rate", &qp::FundingEvent::funding_rate);

    py::class_<qp::KlineEvent>(m, "KlineEvent")
        .def_readonly("kind", &qp::KlineEvent::kind)
        .def_readonly("venue", &qp::KlineEvent::venue)
        .def_readonly("symbol", &qp::KlineEvent::symbol)
        .def_readonly("ts", &qp::KlineEvent::ts)
        .def_readonly("close_time", &qp::KlineEvent::close_time)
        .def_readonly("open", &qp::KlineEvent::open)
        .def_readonly("high", &qp::KlineEvent::high)
        .def_readonly("low", &qp::KlineEvent::low)
        .def_readonly("close", &qp::KlineEvent::close)
        .def_readonly("volume", &qp::KlineEvent::volume);

    py::class_<qp::MarkPriceKlineEvent>(m, "MarkPriceKlineEvent")
        .def_readonly("kind", &qp::MarkPriceKlineEvent::kind)
        .def_readonly("venue", &qp::MarkPriceKlineEvent::venue)
        .def_readonly("symbol", &qp::MarkPriceKlineEvent::symbol)
        .def_readonly("ts", &qp::MarkPriceKlineEvent::ts)
        .def_readonly("close_time", &qp::MarkPriceKlineEvent::close_time)
        .def_readonly("open", &qp::MarkPriceKlineEvent::open)
        .def_readonly("high", &qp::MarkPriceKlineEvent::high)
        .def_readonly("low", &qp::MarkPriceKlineEvent::low)
        .def_readonly("close", &qp::MarkPriceKlineEvent::close);

    py::class_<qp::BookDiffEvent>(m, "BookDiffEvent")
        .def_readonly("kind", &qp::BookDiffEvent::kind)
        .def_readonly("venue", &qp::BookDiffEvent::venue)
        .def_readonly("symbol", &qp::BookDiffEvent::symbol)
        .def_readonly("ts", &qp::BookDiffEvent::ts)
        .def_readonly("first_seq", &qp::BookDiffEvent::first_seq)
        .def_readonly("seq", &qp::BookDiffEvent::seq)
        .def_readonly("prev_seq", &qp::BookDiffEvent::prev_seq)
        .def_readonly("levels", &qp::BookDiffEvent::levels);

    py::class_<qp::BookSnapshotEvent>(m, "BookSnapshotEvent")
        .def_readonly("kind", &qp::BookSnapshotEvent::kind)
        .def_readonly("venue", &qp::BookSnapshotEvent::venue)
        .def_readonly("symbol", &qp::BookSnapshotEvent::symbol)
        .def_readonly("ts", &qp::BookSnapshotEvent::ts)
        .def_readonly("levels", &qp::BookSnapshotEvent::levels);

    py::class_<qp::Intent>(m, "Intent")
        .def(py::init([](qp::SymbolId s, qp::VenueId v, qp::Qty q) {
                 return qp::Intent{.symbol = s, .venue = v, .target_position = q};
             }),
             py::arg("symbol"), py::arg("venue"), py::arg("target_position"))
        .def_readwrite("symbol", &qp::Intent::symbol)
        .def_readwrite("venue", &qp::Intent::venue)
        .def_readwrite("target_position", &qp::Intent::target_position);

    py::class_<qp::Order>(m, "Order")
        .def(py::init([](qp::OrderId id, qp::SymbolId s, qp::Side side, qp::VenueId v, qp::Qty q) {
                 return qp::Order{.id = id, .symbol = s, .side = side, .venue = v, .qty = q};
             }),
             py::arg("id"), py::arg("symbol"), py::arg("side"), py::arg("venue"), py::arg("qty"))
        .def_readwrite("id", &qp::Order::id)
        .def_readwrite("symbol", &qp::Order::symbol)
        .def_readwrite("side", &qp::Order::side)
        .def_readwrite("venue", &qp::Order::venue)
        .def_readwrite("qty", &qp::Order::qty);

    py::class_<qp::RiskDecision>(m, "RiskDecision")
        .def(py::init([](qp::RiskOutcome oc, std::optional<qp::Order> ord) {
                 return qp::RiskDecision{.outcome = oc, .order = ord};
             }),
             py::arg("outcome"), py::arg("order") = std::nullopt)
        .def_readwrite("outcome", &qp::RiskDecision::outcome)
        .def_readwrite("order", &qp::RiskDecision::order);

    py::class_<EquityPoint>(m, "EquityPoint")
        .def_readonly("ts", &EquityPoint::ts)
        .def_readonly("equity", &EquityPoint::equity);

    py::class_<Results>(m, "Results")
        .def_readonly("final_cash", &Results::final_cash)
        .def_readonly("final_equity", &Results::final_equity)
        .def_readonly("symbol_ids", &Results::symbol_ids)
        .def_property_readonly("equity_series", [](const Results& r) {
            return std::vector<EquityPoint>(r.equity_series.begin(), r.equity_series.end());
        });

    py::class_<Dataset>(m, "Dataset")
        .def(py::init<std::string, std::string, std::string, std::string>(), py::arg("data_dir"),
             py::arg("symbol"), py::arg("first_day"), py::arg("last_day"))
        .def(py::init<std::string, std::vector<std::string>, std::string, std::string>(),
             py::arg("data_dir"), py::arg("symbols"), py::arg("first_day"), py::arg("last_day"));

    py::class_<PythonBacktest>(m, "PythonBacktest")
        .def(py::init([](const Dataset& d) {
            return std::make_unique<PythonBacktest>(d.data_dir, d.symbols, d.first_day, d.last_day);
        }))
        .def("set_on_event", &PythonBacktest::set_on_event, py::arg("cb"))
        .def("set_on_timer", &PythonBacktest::set_on_timer, py::arg("cb"))
        .def("set_check", &PythonBacktest::set_check, py::arg("cb"))
        .def("set_on_tick", &PythonBacktest::set_on_tick, py::arg("cb"))
        // Engine runs on a worker thread that calls the python callbacks;
        // release the GIL here so those callbacks can acquire it.
        .def("run", &PythonBacktest::run, py::call_guard<py::gil_scoped_release>());
}
