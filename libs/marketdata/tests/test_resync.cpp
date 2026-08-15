#include "qp/marketdata/resync.hpp"

#include <gtest/gtest.h>

using qp::EventKind;
using qp::find_resync_point;
using qp::MarketEvent;

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
    auto                     idx = find_resync_point(999, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 2u);
}

TEST(FindResyncPoint, FirstEventAlreadyBrackets) {
    std::vector<MarketEvent> buffered{diff(996, 1005), diff(1006, 1010)};
    auto                     idx = find_resync_point(999, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 0u);
}

TEST(FindResyncPoint, ExactBoundaryMatch) {
    std::vector<MarketEvent> buffered{diff(1001, 1001)};
    auto                     idx = find_resync_point(1000, buffered);
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(*idx, 0u);
}

TEST(FindResyncPoint, HoleBetweenSnapshotAndBufferReturnsNullopt) {
    // Snapshot's lastUpdateId+1 = 1000, but the buffer picks up at 1005 —
    // nothing brackets 1000. Must retry with a fresh snapshot.
    std::vector<MarketEvent> buffered{diff(900, 950), diff(1005, 1010)};
    EXPECT_FALSE(find_resync_point(999, buffered).has_value());
}

TEST(FindResyncPoint, EveryBufferedEventPredatesSnapshot) {
    std::vector<MarketEvent> buffered{diff(100, 150), diff(151, 200)};
    EXPECT_FALSE(find_resync_point(1000, buffered).has_value());
}

TEST(FindResyncPoint, EmptyBufferReturnsNullopt) {
    std::vector<MarketEvent> buffered;
    EXPECT_FALSE(find_resync_point(1000, buffered).has_value());
}
