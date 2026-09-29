#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "backtest_in_process_transport.hpp"
#include "binance_historical_source.hpp"
#include "engine.hpp"
#include "exchange.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "subscription.hpp"
#include "support/fake_fetch_pool.hpp"
#include "support/risk_gate_doubles.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using qp::EventKind;
using qp::Timestamp;

namespace {

// One spot hourly bar over 2024-01-01 00:00 UTC, in milliseconds.
constexpr long long kOpenMs  = 1704067200000LL;
constexpr long long kCloseMs = kOpenMs + 3'599'999LL;
constexpr Timestamp kMs      = 1'000'000;

constexpr std::uint16_t kSpot = static_cast<std::uint16_t>(binance::BinanceMarket::Spot);

using Source    = binance::BinanceHistoricalSource<qp::testing::FakeFetchPool>;
using Transport = qp::engine::transport::BacktestInProcessTransport<64, 1>;
using Exec =
    qp::execution::sim::SimExecution<qp::execution::sim::matcher::last_trade::LastTradeMatcher>;

// The bar closes on the price of its last trade.
const std::string kKlineFile =
    "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,"
    "taker_buy_quote_volume,ignore\n" +
    std::to_string(kOpenMs) + ",100.0,105.0,100.0,105.0,2.0," + std::to_string(kCloseMs) +
    ",0,2,0,0,0\n";

// One trade mid bar, one on the bar's last millisecond.
const std::string kTradesFile = "1,100.0,1.0,1,1," + std::to_string(kOpenMs + 600'000LL) +
                                ",False,True\n"
                                "2,105.0,1.0,2,2," +
                                std::to_string(kCloseMs) + ",False,True\n";

/// What the strategy saw and when, in the order it saw it.
struct Seen {
    Timestamp now{};
    EventKind kind{};
};

struct RecordingStrategy {
    std::vector<Seen>* seen;

    std::span<const qp::Intent> on_event(const qp::MarketEvent& event) {
        seen->push_back({event.base.ts, event.base.kind});
        return {};
    }

    std::span<const qp::Intent> on_timer(Timestamp) { return {}; }
};

using BarEngine =
    qp::engine::Engine<Transport, Exec, qp::test::AlwaysApproveRiskGate, RecordingStrategy>;

qp::Subscription universe() {
    qp::SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kSpot, "BTCUSDT");
    return std::move(builder).build();
}

/// Replays the source through the engine and returns what the strategy saw.
std::vector<Seen> replay(const qp::Subscription& subs) {
    Source source(
        binance::BinanceHistoricalConfig{.streams = {{binance::EndpointKind::Klines, "1h"},
                                                     {binance::EndpointKind::AggTrades, ""}},
                                         .from    = kOpenMs * kMs,
                                         .to      = kCloseMs * kMs},
        subs);
    source.plan_with([](std::string_view prefix) {
        return std::vector<std::string>{std::string(prefix) + "FILE-2024-01.zip"};
    });

    EXPECT_FALSE(source.next().has_value());
    EXPECT_TRUE(source.pool().deliver(kKlineFile));
    EXPECT_TRUE(source.pool().deliver(kTradesFile));

    Transport::Queue queue;
    while (auto pulled = source.next()) queue.push(std::move(*pulled));

    std::vector<Seen> seen;
    qp::Portfolio     portfolio{subs};
    Transport         transport({&queue}, {0});
    transport.flush();
    BarEngine engine{
        std::move(transport),
        Exec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        qp::test::AlwaysApproveRiskGate{}, RecordingStrategy{&seen}, portfolio};
    while (engine.step()) {
    }
    return seen;
}

}  // namespace

// A bar's close is unknowable until the bar ends, so the engine must not hand
// the strategy a kline before its close time.
TEST(BarDelivery, StrategyNeverSeesABarBeforeItCloses) {
    const auto subs = universe();
    const auto seen = replay(subs);

    ASSERT_EQ(seen.size(), 3u);
    for (const auto& event : seen) {
        if (event.kind == EventKind::Kline) {
            EXPECT_GE(event.now, kCloseMs * kMs);
        }
    }
}

// Every trade the bar summarises reaches the strategy before the bar does,
// including one stamped on the bar's last millisecond.
TEST(BarDelivery, BarArrivesAfterTheTradesInsideIt) {
    const auto subs = universe();
    const auto seen = replay(subs);

    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0].kind, EventKind::Trade);
    EXPECT_EQ(seen[1].kind, EventKind::Trade);
    EXPECT_EQ(seen[2].kind, EventKind::Kline);
    EXPECT_EQ(seen[1].now, seen[2].now) << "the last trade shares the bar's close stamp";
}
