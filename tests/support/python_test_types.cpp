#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <optional>

#include "risk_gate.hpp"
#include "types.hpp"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(qp_test_types, m) {
    py::class_<qp::Intent>(m, "Intent")
        .def(py::init<>())
        .def(py::init([](std::uint16_t ex, std::uint16_t mk, std::uint16_t sy, qp::Qty q) {
                 return qp::Intent{
                     .exchange = ex, .market = mk, .symbol = sy, .target_position = q};
             }),
             py::arg("exchange"), py::arg("market"), py::arg("symbol"), py::arg("target_position"))
        .def_readwrite("exchange", &qp::Intent::exchange)
        .def_readwrite("market", &qp::Intent::market)
        .def_readwrite("symbol", &qp::Intent::symbol)
        .def_readwrite("target_position", &qp::Intent::target_position);

    py::enum_<qp::Side>(m, "Side").value("Buy", qp::Side::Buy).value("Sell", qp::Side::Sell);

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

    py::enum_<qp::risk::RiskOutcome>(m, "RiskOutcome")
        .value("Approved", qp::risk::RiskOutcome::Approved)
        .value("Resized", qp::risk::RiskOutcome::Resized)
        .value("Rejected", qp::risk::RiskOutcome::Rejected);

    py::class_<qp::risk::RiskDecision>(m, "RiskDecision")
        .def(py::init([](qp::risk::RiskOutcome oc, std::optional<qp::Order> ord) {
                 return qp::risk::RiskDecision{.outcome = oc, .order = ord};
             }),
             py::arg("outcome"), py::arg("order") = std::nullopt);

    py::class_<qp::EventBase>(m, "EventBase").def_readonly("ts", &qp::EventBase::ts);
    py::class_<qp::MarketEvent>(m, "MarketEvent").def_readonly("base", &qp::MarketEvent::base);
}
