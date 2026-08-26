#include <gtest/gtest.h>

#include "resync.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::source::find_resync_point;
using qp::source::FuturesAlignment;
using qp::source::SpotAlignment;

namespace {
MarketEvent diff(std::uint64_t first_seq, std::uint64_t seq) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.first_seq = first_seq;
    ev.seq       = seq;
    return ev;
}
}  // namespace

TEST(FindResyncPoint, SkipsStaleEventsThenFindsBracketingOne) {
    std::vector<MarketEvent> buffered{diff(900, 950), diff(951, 995), diff(996, 1005)};
    auto                     idx = find_resync_point<FuturesAlignment>(999, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 2u);
}

TEST(FindResyncPoint, FirstEventAlreadyBrackets) {
    std::vector<MarketEvent> buffered{diff(996, 1005), diff(1006, 1010)};
    auto                     idx = find_resync_point<FuturesAlignment>(999, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 0u);
}

// The real boundary case (D13): last_update_id lands exactly on a buffered
// event's seq (u) — no +1 slack. This is the exact scenario a real captured
// snapshot+diff sequence hit during collector smoke-testing, which is what
// surfaced the original (+1) rule as wrong for USD-M futures.
TEST(FindResyncPoint, ExactBoundaryAtEventsFinalSeq) {
    std::vector<MarketEvent> buffered{diff(996, 1000)};
    auto                     idx = find_resync_point<FuturesAlignment>(1000, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 0u);
}

// Same boundary, the other side: last_update_id lands exactly on an event's
// first_seq (U).
TEST(FindResyncPoint, ExactBoundaryAtEventsFirstSeq) {
    std::vector<MarketEvent> buffered{diff(1000, 1005)};
    auto                     idx = find_resync_point<FuturesAlignment>(1000, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 0u);
}

TEST(FindResyncPoint, HoleBetweenSnapshotAndBufferReturnsNullopt) {
    // Snapshot's lastUpdateId = 999, but the buffer picks up at 1005 —
    // nothing brackets 999. Must retry with a fresh snapshot.
    std::vector<MarketEvent> buffered{diff(900, 950), diff(1005, 1010)};
    EXPECT_FALSE(find_resync_point<FuturesAlignment>(999, buffered).has_value());
}

TEST(FindResyncPoint, EveryBufferedEventPredatesSnapshot) {
    std::vector<MarketEvent> buffered{diff(100, 150), diff(151, 200)};
    EXPECT_FALSE(find_resync_point<FuturesAlignment>(1000, buffered).has_value());
}

TEST(FindResyncPoint, EmptyBufferReturnsNullopt) {
    std::vector<MarketEvent> buffered;
    EXPECT_FALSE(find_resync_point<FuturesAlignment>(1000, buffered).has_value());
}

// --- SpotAlignment, through find_resync_point itself, not just the raw
// AlignmentRule::brackets() comparison (test_resync_policy.cpp) — proves the
// +1 slack actually changes find_resync_point's real output on the exact
// D13 boundary, not just the isolated comparison function.

TEST(FindResyncPoint, SpotRejectsTheExactBoundaryFuturesAccepts) {
    // Same buffered event, same last_update_id as
    // FindResyncPoint.ExactBoundaryAtEventsFinalSeq — futures finds it (idx
    // 0), spot's +1 rule pushes past `u` and finds nothing here.
    std::vector<MarketEvent> buffered{diff(996, 1000)};
    EXPECT_FALSE(find_resync_point<SpotAlignment>(1000, buffered).has_value());
}

TEST(FindResyncPoint, SpotBracketsOneBelowWhereFuturesWould) {
    std::vector<MarketEvent> buffered{diff(996, 1000)};
    auto                     idx = find_resync_point<SpotAlignment>(999, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 0u);
}
