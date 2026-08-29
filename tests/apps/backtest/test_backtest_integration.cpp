#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <utility>

#include "backtest.hpp"
#include "partition.hpp"
#include "support/market_event_builders.hpp"
#include "support/scratch_dir.hpp"
#include "types.hpp"
#include "wire.hpp"
#include "zstd_stream.hpp"

using qp::data_source::wire::day_key_for;
using qp::data_source::wire::DayKey;
using qp::test::make_funding;
using qp::test::make_trade;
using qp::test::ScratchDir;

namespace {

constexpr qp::VenueId kFuturesVenue = 0;  // D48: matches apps/collector's run_data_source pairing
constexpr qp::VenueId kSpotVenue    = 1;

// Writes one leg's data directly via qp_wire's own primitives — same shape
// apps/collector itself used to produce (one FileRecorder per leg, D41),
// hand-rolled now that FileRecorder no longer exists as a Sink (removed
// with the rest of the live-collector machinery). Groups `events` by day
// (day_key_for(ts)) since a real recording is one segment per (symbol,
// day); every test here only ever spans one day, so in practice this
// writes exactly one segment, but it stays correct if that changes.
void write_leg(const std::filesystem::path& leg_dir, const std::vector<qp::MarketEvent>& events) {
    std::filesystem::path symbol_dir = leg_dir / "BTCUSDT";
    std::filesystem::create_directories(symbol_dir);

    std::ofstream manifest(leg_dir / "symbols.manifest");
    manifest << "BTCUSDT\n";

    std::map<DayKey, std::vector<qp::MarketEvent>> by_day;
    for (const auto& ev : events) by_day[day_key_for(ev.ts)].push_back(ev);

    for (const auto& [day, day_events] : by_day) {
        qp::data_source::wire::ZstdCompressor compressor;
        std::vector<std::byte>                compressed;
        for (const auto& ev : day_events) {
            std::vector<std::byte> encoded;
            qp::data_source::wire::write_event(encoded, ev);
            compressor.compress(encoded, compressed);
        }
        compressor.finish(compressed);

        std::ofstream out(qp::data_source::wire::segment_path(symbol_dir, day, 0),
                          std::ios::binary);
        out.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
    }
}

}  // namespace

// End-to-end proof that apps/backtest's wiring — FileReplaySource (both
// legs) -> engine::transport::BacktestInProcessTransport ->
// engine::Engine<..., BasicRiskGate<Portfolio>, FundingCarryStrategy<Portfolio>,
// Portfolio> — actually produces the delta-neutral position funding carry
// is supposed to, not just that each piece compiles.
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
