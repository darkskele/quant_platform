#include <gtest/gtest.h>

#include "backtest.hpp"
#include "file_recorder.hpp"
#include "partition.hpp"
#include "support/market_event_builders.hpp"
#include "support/scratch_dir.hpp"
#include "types.hpp"

using qp::sink::FileRecorder;
using qp::test::make_funding;
using qp::test::make_trade;
using qp::test::ScratchDir;
using qp::wire::day_key_for;

namespace {

constexpr qp::VenueId kFuturesVenue = 0;  // D48: matches apps/collector's run_data_source pairing
constexpr qp::VenueId kSpotVenue    = 1;

// Writes one leg's recorded data — same shape apps/collector itself
// produces (one FileRecorder per leg, D41), just constructed directly
// rather than driven by a live/mock source, since this test only needs the
// bytes on disk, not a real collector run.
void write_leg(const std::filesystem::path& leg_dir, const std::vector<qp::MarketEvent>& events) {
    FileRecorder recorder(leg_dir, {"BTCUSDT"});
    for (const auto& ev : events) recorder.record(ev);
}  // destructor: drains the queue, flushes and cleanly closes the partition

}  // namespace

// End-to-end proof that apps/backtest's wiring — FileReplaySource (both
// legs) -> BacktestInProcessTransport -> Engine<..., BasicRiskGate<Portfolio>,
// FundingCarryStrategy, Portfolio> — actually produces the delta-neutral
// position funding carry is supposed to, not just that each piece compiles.
TEST(BacktestIntegration, EntersDeltaNeutralPositionOnAFundingEventAboveThreshold) {
    ScratchDir   dir;
    std::int64_t base_ts = 1'700'000'000LL * 1'000'000'000LL;

    auto futures_trade = make_trade(0, base_ts, /*price=*/100.0, 1.0, qp::Side::Buy, kFuturesVenue);
    auto spot_trade = make_trade(0, base_ts + 500, /*price=*/100.0, 1.0, qp::Side::Buy, kSpotVenue);
    auto funding_event = make_funding(0, base_ts + 1000, /*rate=*/0.0002, /*mark_price=*/100.0,
                                      kFuturesVenue);  // clears the default 0.0001 entry threshold

    write_leg(dir.path / "futures", {futures_trade, funding_event});
    write_leg(dir.path / "spot", {spot_trade});

    qp::backtest::Config config{
        .data_dir  = dir.path,
        .symbol    = "BTCUSDT",
        .first_day = day_key_for(base_ts),
        .last_day  = day_key_for(base_ts),
    };

    auto results = qp::backtest::run(config);

    EXPECT_DOUBLE_EQ(results.final_spot_position, 1.0);
    EXPECT_DOUBLE_EQ(results.final_futures_position, -1.0);
    // Both legs fill at the same price (100.0), so the only net effect is
    // the two fills' taker fees — no PnL, no funding payment applies (this
    // is the event that opened the position, settlement is against the
    // position as it stood *before* the strategy reacted to it).
    EXPECT_NEAR(results.final_cash, -0.08, 1e-9);
    EXPECT_NEAR(results.final_equity, -0.08, 1e-9);
}

TEST(BacktestIntegration, StaysFlatWhenFundingNeverClearsTheEntryThreshold) {
    ScratchDir   dir;
    std::int64_t base_ts = 1'700'000'000LL * 1'000'000'000LL;

    auto futures_trade = make_trade(0, base_ts, /*price=*/100.0, 1.0, qp::Side::Buy, kFuturesVenue);
    auto funding_event = make_funding(0, base_ts + 1000, /*rate=*/0.00001, /*mark_price=*/100.0,
                                      kFuturesVenue);  // below the default 0.0001 entry threshold

    write_leg(dir.path / "futures", {futures_trade, funding_event});
    write_leg(dir.path / "spot", {});

    qp::backtest::Config config{
        .data_dir  = dir.path,
        .symbol    = "BTCUSDT",
        .first_day = day_key_for(base_ts),
        .last_day  = day_key_for(base_ts),
    };

    auto results = qp::backtest::run(config);

    EXPECT_DOUBLE_EQ(results.final_spot_position, 0.0);
    EXPECT_DOUBLE_EQ(results.final_futures_position, 0.0);
    EXPECT_DOUBLE_EQ(results.final_cash, 0.0);
}
