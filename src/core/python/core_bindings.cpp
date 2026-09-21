#include "core_bindings.hpp"

#include <pybind11/stl.h>

#include <memory>
#include <utility>
#include <variant>

#include "exchange.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace py = pybind11;

namespace qp::python {

void bind_core(py::module_& m) {
    py::enum_<ExchangeId>(m, "ExchangeId").value("Binance", ExchangeId::Binance);

    py::enum_<Side>(m, "Side").value("Buy", Side::Buy).value("Sell", Side::Sell);

    py::enum_<EventKind>(m, "EventKind")
        .value("Trade", EventKind::Trade)
        .value("Funding", EventKind::Funding)
        .value("Kline", EventKind::Kline)
        .value("MarkPriceKline", EventKind::MarkPriceKline)
        .value("PremiumIndexKline", EventKind::PremiumIndexKline)
        .value("OpenInterest", EventKind::OpenInterest)
        .value("BookDepth", EventKind::BookDepth);

    py::class_<TradeEvent>(m, "TradeEvent")
        .def_readonly("id", &TradeEvent::id)
        .def_readonly("first_trade_id", &TradeEvent::first_trade_id)
        .def_readonly("last_trade_id", &TradeEvent::last_trade_id)
        .def_readonly("price", &TradeEvent::price)
        .def_readonly("qty", &TradeEvent::qty)
        .def_readonly("side", &TradeEvent::side);

    py::class_<FundingEvent>(m, "FundingEvent")
        .def_readonly("funding_rate", &FundingEvent::funding_rate)
        .def_readonly("interval_hours", &FundingEvent::interval_hours);

    py::class_<KlineEvent>(m, "KlineEvent")
        .def_readonly("close_time", &KlineEvent::close_time)
        .def_readonly("open", &KlineEvent::open)
        .def_readonly("high", &KlineEvent::high)
        .def_readonly("low", &KlineEvent::low)
        .def_readonly("close", &KlineEvent::close)
        .def_readonly("volume", &KlineEvent::volume);

    py::class_<MarkPriceKlineEvent>(m, "MarkPriceKlineEvent")
        .def_readonly("close_time", &MarkPriceKlineEvent::close_time)
        .def_readonly("open", &MarkPriceKlineEvent::open)
        .def_readonly("high", &MarkPriceKlineEvent::high)
        .def_readonly("low", &MarkPriceKlineEvent::low)
        .def_readonly("close", &MarkPriceKlineEvent::close);

    py::class_<PremiumIndexKlineEvent>(m, "PremiumIndexKlineEvent")
        .def_readonly("close_time", &PremiumIndexKlineEvent::close_time)
        .def_readonly("open", &PremiumIndexKlineEvent::open)
        .def_readonly("high", &PremiumIndexKlineEvent::high)
        .def_readonly("low", &PremiumIndexKlineEvent::low)
        .def_readonly("close", &PremiumIndexKlineEvent::close);

    // Coin-M leaves the three long short ratios empty, so those arrive as NaN
    // rather than raising.
    py::class_<OpenInterestEvent>(m, "OpenInterestEvent")
        .def_readonly("open_interest", &OpenInterestEvent::open_interest)
        .def_readonly("open_interest_value", &OpenInterestEvent::open_interest_value)
        .def_readonly("toptrader_account_ratio", &OpenInterestEvent::toptrader_account_ratio)
        .def_readonly("toptrader_position_ratio", &OpenInterestEvent::toptrader_position_ratio)
        .def_readonly("account_long_short_ratio", &OpenInterestEvent::account_long_short_ratio)
        .def_readonly("taker_long_short_volume_ratio",
                      &OpenInterestEvent::taker_long_short_volume_ratio);

    py::class_<DepthBand>(m, "DepthBand")
        .def_readonly("depth", &DepthBand::depth)
        .def_readonly("notional", &DepthBand::notional);

    py::class_<BookDepthBands, std::shared_ptr<BookDepthBands>>(m, "BookDepthBands")
        .def_readonly("bids", &BookDepthBands::bids)
        .def_readonly("asks", &BookDepthBands::asks);

    py::class_<BookDepthEvent>(m, "BookDepthEvent").def_readonly("bands", &BookDepthEvent::bands);

    py::class_<EventBase>(m, "EventBase")
        .def_readonly("kind", &EventBase::kind)
        .def_readonly("exchange", &EventBase::exchange)
        .def_readonly("market", &EventBase::market)
        .def_readonly("symbol", &EventBase::symbol)
        .def_readonly("ts", &EventBase::ts);

    py::class_<MarketEvent>(m, "MarketEvent")
        .def_readonly("base", &MarketEvent::base)
        .def_property_readonly("payload", [](const MarketEvent& e) -> py::object {
            return std::visit([](const auto& p) { return py::cast(p); }, e.payload);
        });

    py::class_<Intent>(m, "Intent")
        .def(py::init([](std::uint16_t ex, std::uint16_t mk, std::uint16_t sy, Qty q) {
                 return Intent{.exchange = ex, .market = mk, .symbol = sy, .target_position = q};
             }),
             py::arg("exchange"), py::arg("market"), py::arg("symbol"), py::arg("target_position"))
        .def_readwrite("exchange", &Intent::exchange)
        .def_readwrite("market", &Intent::market)
        .def_readwrite("symbol", &Intent::symbol)
        .def_readwrite("target_position", &Intent::target_position);

    py::class_<Order>(m, "Order")
        .def(py::init([](OrderId id, std::uint16_t ex, std::uint16_t mk, std::uint16_t sy,
                         Side side, Qty q) {
                 return Order{
                     .id = id, .exchange = ex, .market = mk, .symbol = sy, .side = side, .qty = q};
             }),
             py::arg("id"), py::arg("exchange"), py::arg("market"), py::arg("symbol"),
             py::arg("side"), py::arg("qty"))
        .def_readwrite("id", &Order::id)
        .def_readwrite("exchange", &Order::exchange)
        .def_readwrite("market", &Order::market)
        .def_readwrite("symbol", &Order::symbol)
        .def_readwrite("side", &Order::side)
        .def_readwrite("qty", &Order::qty);

    py::class_<Subscription::Instrument>(m, "Instrument")
        .def_readonly("exchange", &Subscription::Instrument::exchange)
        .def_readonly("market", &Subscription::Instrument::market)
        .def_readonly("symbol", &Subscription::Instrument::symbol);

    py::class_<Subscription>(m, "Subscription")
        .def("resolve", &Subscription::resolve, py::arg("exchange"), py::arg("market"),
             py::arg("symbol"));

    py::class_<SubscriptionBuilder>(m, "SubscriptionBuilder")
        .def(py::init<>())
        .def("add", &SubscriptionBuilder::add, py::arg("exchange"), py::arg("market"),
             py::arg("symbol"))
        .def("build", [](SubscriptionBuilder& self) { return std::move(self).build(); });
}

}  // namespace qp::python
