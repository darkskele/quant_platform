#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <span>
#include <variant>
#include <vector>

#include "support/market_event_builders.hpp"
#include "wire.hpp"

using qp::BookDiffEvent;
using qp::BookLevels;
using qp::EventKind;
using qp::FundingEvent;
using qp::MarketEvent;
using qp::PriceLevel;
using qp::Side;
using qp::TradeEvent;

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

bool levels_equal(const BookLevels& a, const BookLevels& b) {
    if (a.bids.size() != b.bids.size() || a.asks.size() != b.asks.size()) return false;
    for (std::size_t i = 0; i < a.bids.size(); ++i) {
        if (a.bids[i].price != b.bids[i].price || a.bids[i].qty != b.bids[i].qty) return false;
    }
    for (std::size_t i = 0; i < a.asks.size(); ++i) {
        if (a.asks[i].price != b.asks[i].price || a.asks[i].qty != b.asks[i].qty) return false;
    }
    return true;
}

// Value comparison, not the variant's own operator== (which would compare
// BookDiffEvent/BookSnapshotEvent's shared_ptr<const BookLevels> by
// pointer identity — always false after a round trip, since read_event
// always allocates a fresh BookLevels, even when the bytes are identical).
void expect_equal(const MarketEvent& a, const MarketEvent& b) {
    ASSERT_EQ(a.index(), b.index());
    std::visit(
        [&b](const auto& ae) {
            using E        = std::decay_t<decltype(ae)>;
            const auto& be = std::get<E>(b);
            EXPECT_EQ(ae.kind, be.kind);
            EXPECT_EQ(ae.venue, be.venue);
            EXPECT_EQ(ae.symbol, be.symbol);
            EXPECT_EQ(ae.ts, be.ts);
            if constexpr (std::is_same_v<E, TradeEvent>) {
                EXPECT_EQ(ae.side, be.side);
                EXPECT_EQ(ae.price, be.price);
                EXPECT_EQ(ae.qty, be.qty);
            } else if constexpr (std::is_same_v<E, FundingEvent>) {
                EXPECT_EQ(ae.mark_price, be.mark_price);
                EXPECT_EQ(ae.funding_rate, be.funding_rate);
            } else if constexpr (std::is_same_v<E, BookDiffEvent>) {
                EXPECT_EQ(ae.first_seq, be.first_seq);
                EXPECT_EQ(ae.seq, be.seq);
                EXPECT_EQ(ae.prev_seq, be.prev_seq);
                ASSERT_TRUE(ae.levels && be.levels);
                EXPECT_TRUE(levels_equal(*ae.levels, *be.levels));
            }
        },
        a);
}

}  // namespace

TEST(Wire, RoundTripsBookDiffWithLevels) {
    auto ev = book_diff(101, 105, 100, {{50000.0, 1.5}, {49999.5, 0.3}}, {{50000.5, 2.0}});

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
    EXPECT_TRUE(cursor.empty());  // fully consumed
}

TEST(Wire, RoundTripsBookDiffWithNoLevels) {
    auto ev = book_diff(101, 105, 100, {}, {});

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
    EXPECT_TRUE(cursor.empty());
}

TEST(Wire, RoundTripsNonZeroVenue) {
    // Every other round-trip test uses venue's default (0) — real proof the
    // byte actually gets read back, not just written and silently defaulted
    // to 0 on both sides (D43: FanoutSink stamps this per-leg).
    auto ev                           = book_diff(101, 105, 100, {}, {});
    std::get<BookDiffEvent>(ev).venue = 1;

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(qp::header_of(*decoded).venue, 1);
    expect_equal(ev, *decoded);
}

TEST(Wire, RoundTripsTrade) {
    auto ev = trade(50123.45, 0.02, Side::Sell);

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
}

TEST(Wire, RoundTripsFunding) {
    auto ev = funding(0.0001, 50123.45);

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(decoded.has_value());
    expect_equal(ev, *decoded);
}

TEST(Wire, ReadsMultipleEventsBackToBackInOrder) {
    auto ev1 = book_diff(1, 5, 0, {{1.0, 1.0}}, {});
    auto ev2 = trade(2.0, 3.0, Side::Buy);
    auto ev3 = funding(-0.0002);

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev1);
    qp::data_source::wire::write_event(buf, ev2);
    qp::data_source::wire::write_event(buf, ev3);

    std::span<const std::byte> cursor{buf};

    auto d1 = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(d1.has_value());
    expect_equal(ev1, *d1);

    auto d2 = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(d2.has_value());
    expect_equal(ev2, *d2);

    auto d3 = qp::data_source::wire::read_event(cursor);
    ASSERT_TRUE(d3.has_value());
    expect_equal(ev3, *d3);

    EXPECT_TRUE(cursor.empty());
    EXPECT_FALSE(qp::data_source::wire::read_event(cursor).has_value());  // nothing left
}

// --- Truncated-tail tolerance: the crash-safety contract (D12) ---

TEST(Wire, TruncatedHeaderReturnsNulloptAndDoesNotAdvance) {
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}}, {});

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);
    buf.resize(3);  // chop mid-header, well before the level counts even start

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());  // untouched on failure
}

TEST(Wire, TruncatedMidPriceLevelsReturnsNulloptAndDoesNotAdvance) {
    // Levels declared but not fully present — the exact shape of a crash
    // mid-write after the count was flushed but before all levels were.
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}}, {});

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);
    buf.resize(buf.size() - sizeof(PriceLevel) - 2);  // chop into the last level's bytes

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());
}

TEST(Wire, TruncatedAfterLevelsButBeforeTrailingFieldsReturnsNullopt) {
    // book_diff's own trailing field is the ask-levels block itself (unlike
    // the old fixed layout, BookDiff has no fields after bids/asks) —
    // chopping the last byte still lands mid-decode, just inside the ask
    // level array rather than a separate trailing-field group.
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}}, {{2.0, 2.0}});

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);
    buf.resize(buf.size() - 1);  // chop the very last byte

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());
}

TEST(Wire, BogusLevelCountWithInsufficientBytesReturnsNulloptWithoutOversizedAllocation) {
    // A corrupted/truncated count (here: claims 1000 levels, only room for
    // one) must not drive an allocation sized off untrusted input before
    // the byte budget is checked.
    auto ev = book_diff(1, 5, 0, {{1.0, 1.0}}, {});

    std::vector<std::byte> buf;
    qp::data_source::wire::write_event(buf, ev);

    // bid_count is the uint32 right after the 14-byte shared header
    // (kind:1 + venue:1 + symbol:4 + ts:8 = 14) plus BookDiff's own
    // first_seq:8 + seq:8 + prev_seq:8 = 24, so bid_count sits at offset
    // 14+24 = 38 — overwrite it with a huge bogus value.
    static constexpr std::size_t   kBidCountOffset = 38;
    static constexpr std::uint32_t kBogusCount     = 1000;
    std::memcpy(buf.data() + kBidCountOffset, &kBogusCount, sizeof(kBogusCount));

    std::span<const std::byte> cursor{buf};
    auto                       decoded = qp::data_source::wire::read_event(cursor);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(cursor.size(), buf.size());
}
