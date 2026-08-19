#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "file_recorder.hpp"
#include "file_replay_source.hpp"
#include "partition.hpp"
#include "support/market_event_builders.hpp"
#include "support/scratch_dir.hpp"
#include "types.hpp"

using qp::MarketEvent;
using qp::Price;
using qp::PriceLevel;
using qp::Qty;
using qp::Side;
using qp::SymbolId;
using qp::sink::FileRecorder;
using qp::source::FileReplaySource;
using qp::test::ScratchDir;
using qp::wire::day_key_for;

namespace {

constexpr std::int64_t kNanosPerDay = 86400LL * 1'000'000'000LL;

MarketEvent book_diff(SymbolId symbol, std::int64_t ts, std::uint64_t seq,
                      std::vector<PriceLevel> bids, std::vector<PriceLevel> asks) {
    return qp::test::make_book_diff(symbol, ts, seq, seq + 4, seq - 1, std::move(bids),
                                    std::move(asks));
}

MarketEvent trade(SymbolId symbol, std::int64_t ts, Price price, Qty qty, Side side) {
    return qp::test::make_trade(symbol, ts, price, qty, side);
}

MarketEvent funding(SymbolId symbol, std::int64_t ts, double rate) {
    return qp::test::make_funding(symbol, ts, rate);
}

void expect_equal(const MarketEvent& a, const MarketEvent& b) {
    EXPECT_EQ(a.kind, b.kind);
    EXPECT_EQ(a.ts, b.ts);
    EXPECT_EQ(a.first_seq, b.first_seq);
    EXPECT_EQ(a.seq, b.seq);
    EXPECT_EQ(a.prev_seq, b.prev_seq);
    EXPECT_EQ(a.symbol, b.symbol);
    ASSERT_EQ(a.bids.size(), b.bids.size());
    for (std::size_t i = 0; i < a.bids.size(); ++i) {
        EXPECT_EQ(a.bids[i].price, b.bids[i].price);
        EXPECT_EQ(a.bids[i].qty, b.bids[i].qty);
    }
    ASSERT_EQ(a.asks.size(), b.asks.size());
    for (std::size_t i = 0; i < a.asks.size(); ++i) {
        EXPECT_EQ(a.asks[i].price, b.asks[i].price);
        EXPECT_EQ(a.asks[i].qty, b.asks[i].qty);
    }
    EXPECT_EQ(a.price, b.price);
    EXPECT_EQ(a.qty, b.qty);
    EXPECT_EQ(a.side, b.side);
    EXPECT_EQ(a.funding_rate, b.funding_rate);
}

std::vector<MarketEvent> replay_all(FileReplaySource& source) {
    std::vector<MarketEvent> events;
    while (auto ev = source.next()) events.push_back(std::move(*ev));
    return events;
}

}  // namespace

// The property the wire-lib split (D19) exists to guarantee: FileRecorder
// and FileReplaySource are two independently-built classes that
// deliberately never depend on each other (source/sink each depend only on
// wire) — so the one thing neither lib's own tests can prove is that they
// actually agree when composed. That's this file's entire job: real
// FileRecorder writes, real FileReplaySource reads, byte-for-byte/field-
// for-field identical output, not just "both independently satisfy the
// wire-format spec." See source/docs/DESIGN.md G2's success metric.
TEST(RecorderReplayParity, SingleSymbolRoundTripsExactly) {
    ScratchDir   dir;
    std::int64_t base_ts =
        1'700'000'000LL * 1'000'000'000LL;  // arbitrary, real-ish, ns since epoch

    std::vector<MarketEvent> written = {
        book_diff(0, base_ts + 0, 100, {{50000.0, 1.5}, {49999.5, 0.3}}, {{50000.5, 2.0}}),
        trade(0, base_ts + 1000, 50123.45, 0.02, Side::Sell),
        funding(0, base_ts + 2000, 0.0001),
        book_diff(0, base_ts + 3000, 108, {}, {{50001.0, 0.5}}),
    };

    {
        FileRecorder recorder(dir.path, {"BTCUSDT"});
        for (const auto& ev : written) recorder.record(ev);
    }  // destructor: drains the queue, flushes and cleanly closes the partition

    FileReplaySource source(dir.path, day_key_for(base_ts), day_key_for(base_ts));
    auto             replayed = replay_all(source);

    ASSERT_EQ(replayed.size(), written.size());
    for (std::size_t i = 0; i < written.size(); ++i) expect_equal(written[i], replayed[i]);
}

TEST(RecorderReplayParity, MultiSymbolRoundTripsInGlobalTimestampOrder) {
    ScratchDir   dir;
    std::int64_t base_ts = 1'700'000'000LL * 1'000'000'000LL;

    {
        // Recorded in per-symbol arrival order (FileRecorder partitions by
        // symbol) — replay has to reassemble a single global-timestamp-
        // ordered stream across both partitions, not just play each back
        // in its own recorded order.
        FileRecorder recorder(dir.path, {"BTCUSDT", "ETHUSDT"});
        recorder.record(trade(0, base_ts + 0, 50000.0, 1.0, Side::Buy));
        recorder.record(trade(0, base_ts + 1000, 50010.0, 0.5, Side::Buy));
        recorder.record(trade(1, base_ts + 500, 3000.0, 2.0, Side::Sell));
        recorder.record(trade(1, base_ts + 1500, 3005.0, 1.0, Side::Sell));
    }

    FileReplaySource source(dir.path, day_key_for(base_ts), day_key_for(base_ts));
    auto             replayed = replay_all(source);

    ASSERT_EQ(replayed.size(), 4u);
    for (std::size_t i = 1; i < replayed.size(); ++i) EXPECT_LE(replayed[i - 1].ts, replayed[i].ts);
    EXPECT_EQ(replayed[0].symbol, 0u);  // ts base+0
    EXPECT_EQ(replayed[1].symbol, 1u);  // ts base+500
    EXPECT_EQ(replayed[2].symbol, 0u);  // ts base+1000
    EXPECT_EQ(replayed[3].symbol, 1u);  // ts base+1500
}

TEST(RecorderReplayParity, CrossesDayBoundaryCleanly) {
    ScratchDir   dir;
    std::int64_t day0_ts = 19723 * kNanosPerDay + 1;  // just after 2024-01-01 UTC midnight
    std::int64_t day1_ts = 19724 * kNanosPerDay + 1;  // just after 2024-01-02 UTC midnight

    {
        FileRecorder recorder(dir.path, {"BTCUSDT"});
        recorder.record(trade(0, day0_ts, 100.0, 1.0, Side::Buy));
        recorder.record(trade(0, day1_ts, 101.0, 1.0, Side::Buy));
    }  // FileRecorder rotates partitions mid-stream on the day boundary

    FileReplaySource source(dir.path, day_key_for(day0_ts), day_key_for(day1_ts));
    auto             replayed = replay_all(source);

    ASSERT_EQ(replayed.size(), 2u);
    EXPECT_EQ(replayed[0].ts, day0_ts);
    EXPECT_EQ(replayed[1].ts, day1_ts);
}
