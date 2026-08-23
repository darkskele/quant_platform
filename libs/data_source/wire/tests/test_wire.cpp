#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

#include "support/market_event_builders.hpp"
#include "wire.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::PriceLevel;
using qp::Side;

namespace {

// Fixed symbol/ts per kind — this file only cares about the fields under
// test (seq numbers, levels, price/qty/side, rate), not identity/timing.
MarketEvent book_diff(std::uint64_t first_seq, std::uint64_t seq, std::uint64_t prev_seq,
                      std::vector<PriceLevel> bids, std::vector<PriceLevel> asks) {
    return qp::test::make_book_diff(7, 1'700'000'000'000, first_seq, seq, prev_seq, std::move(bids),
                                    std::move(asks));
}

MarketEvent trade(qp::Price price, qp::Qty qty, Side side) {
    return qp::test::make_trade(3, 1'700'000'001'000, price, qty, side);
}

MarketEvent funding(double rate, qp::Price mark_price = 0.0) {
    return qp::test::make_funding(1, 1'700'000'002'000, rate, mark_price);
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
    EXPECT_EQ(a.mark_price, b.mark_price);
    EXPECT_EQ(a.funding_rate, b.funding_rate);
}

}  // namespace

TEST(Wire, RoundTripsBookDiffWithLevels) {
    auto ev = book_diff(101, 105, 100, {{50000.0, 1.5}, {49999.5, 0.3}}, {{50000.5, 2.0}});

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
    EXPECT_TRUE(cursor.empty());  // fully consumed
}

TEST(Wire, RoundTripsBookDiffWithNoLevels) {
    auto ev = book_diff(101, 105, 100, {}, {});

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
    EXPECT_TRUE(cursor.empty());
}

TEST(Wire, RoundTripsTrade) {
    auto ev = trade(50123.45, 0.02, Side::Sell);

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
}

TEST(Wire, RoundTripsFunding) {
    auto ev = funding(0.0001, 50123.45);

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
}

TEST(Wire, ReadsMultipleEventsBackToBackInOrder) {
    auto ev1 = book_diff(1, 5, 0, {{1.0, 1.0}}, {});
    auto ev2 = trade(2.0, 3.0, Side::Buy);
    auto ev3 = funding(-0.0002);

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev1);
    qp::wire::write_event(buf, ev2);
    qp::wire::write_event(buf, ev3);

    std::span<const std::byte> cursor{buf};

    auto d1 = qp::wire::read_event(cursor);
    ASSERT_TRUE(d1.has_value());
    expect_equal(ev1, *d1);

    auto d2 = qp::wire::read_event(cursor);
    ASSERT_TRUE(d2.has_value());
    expect_equal(ev2, *d2);

    auto d3 = qp::wire::read_event(cursor);
    ASSERT_TRUE(d3.has_value());
    expect_equal(ev3, *d3);

    EXPECT_TRUE(cursor.empty());
    EXPECT_FALSE(qp::wire::read_event(cursor).has_value());  // nothing left
}

// --- Truncated-tail tolerance: the crash-safety contract (D12) ---

TEST(Wire, TruncatedHeaderReturnsNulloptAndDoesNotAdvance) {
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}}, {});

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);
    buf.resize(3);  // chop mid-header, well before the level counts even start

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());  // untouched on failure
}

TEST(Wire, TruncatedMidPriceLevelsReturnsNulloptAndDoesNotAdvance) {
    // Levels declared but not fully present — the exact shape of a crash
    // mid-write after the count was flushed but before all levels were.
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}}, {});

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);
    buf.resize(buf.size() - sizeof(PriceLevel) - 2);  // chop into the last level's bytes

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());
}

TEST(Wire, TruncatedAfterLevelsButBeforeTrailingFieldsReturnsNullopt) {
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}}, {{2.0, 2.0}});

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);
    buf.resize(buf.size() - 1);  // chop the very last byte (of the trailing fixed-field group)

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());
}

TEST(Wire, BogusLevelCountWithInsufficientBytesReturnsNulloptWithoutOversizedAllocation) {
    // A corrupted/truncated count (here: claims 1000 levels, only room for
    // one) must not drive an allocation sized off untrusted input before
    // the byte budget is checked.
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}}, {});

    std::vector<std::byte> buf;
    qp::wire::write_event(buf, ev);

    // bid_count is the uint32 right after the 38-byte fixed header
    // (kind:1 + ts:8 + first_seq:8 + seq:8 + prev_seq:8 + symbol:4 = 37,
    // then bid_count at offset 37) — overwrite it with a huge bogus value.
    static constexpr std::size_t   kBidCountOffset = 37;
    static constexpr std::uint32_t kBogusCount     = 1000;
    std::memcpy(buf.data() + kBidCountOffset, &kBogusCount, sizeof(kBogusCount));

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());
}
