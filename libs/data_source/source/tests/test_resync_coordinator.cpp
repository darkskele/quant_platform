#include <gtest/gtest.h>

#include "resync_coordinator.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::PriceLevel;
using qp::source::FuturesAlignment;
using qp::source::ResyncCoordinator;
using qp::source::SpotAlignment;
using Action = qp::source::ResyncCoordinator<qp::source::FuturesAlignment>::Action;
// Action is a nested type of the class template, not a free enum — each
// Rule instantiation has its own distinct Action type (same enumerator
// names, not the same type), so a SpotAlignment-coordinator's .action needs
// its own alias rather than comparing against FuturesAlignment's Action.
using SpotAction = qp::source::ResyncCoordinator<qp::source::SpotAlignment>::Action;

namespace {

constexpr qp::SymbolId kBtc = 0;
constexpr qp::SymbolId kEth = 1;

MarketEvent diff(qp::SymbolId symbol, std::uint64_t first_seq, std::uint64_t seq,
                 std::uint64_t prev_seq) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.symbol    = symbol;
    ev.first_seq = first_seq;
    ev.seq       = seq;
    ev.prev_seq  = prev_seq;
    return ev;
}

}  // namespace

// --- Fresh connect: buffering starts immediately, one request per round ---

TEST(ResyncCoordinator, FirstEventForASymbolBuffersAndRequests) {
    ResyncCoordinator<FuturesAlignment> coord;
    auto                                v = coord.on_event(diff(kBtc, 1, 100, 0));
    EXPECT_EQ(v.action, Action::BufferAndRequest);
    EXPECT_EQ(v.symbol, kBtc);
    EXPECT_FALSE(v.event.has_value());
    EXPECT_FALSE(v.buffer_overflowed);  // fresh start, not a capacity-driven restart
}

TEST(ResyncCoordinator, SubsequentEventsWhileBufferingDontRequestAgain) {
    ResyncCoordinator<FuturesAlignment> coord;
    ASSERT_EQ(coord.on_event(diff(kBtc, 1, 100, 0)).action, Action::BufferAndRequest);
    EXPECT_EQ(coord.on_event(diff(kBtc, 101, 105, 100)).action, Action::Buffer);
    EXPECT_EQ(coord.on_event(diff(kBtc, 106, 110, 105)).action, Action::Buffer);
}

// --- The actual ask: snapshot -> correct delta -> correct replayed sequence ---

TEST(ResyncCoordinator, SnapshotThatBracketsFirstBufferedEventReplaysEverythingInOrder) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_event(diff(kBtc, 101, 105, 100));
    coord.on_event(diff(kBtc, 106, 110, 105));

    auto outcome = coord.on_snapshot(kBtc, 50);  // 1 <= 50 <= 100: brackets the first event
    ASSERT_FALSE(outcome.need_retry);
    // +1 for the BookSnapshot anchor, always first.
    ASSERT_EQ(outcome.to_replay.size(), 4u);
    EXPECT_EQ(outcome.to_replay[0].kind, EventKind::BookSnapshot);
    EXPECT_EQ(outcome.to_replay[0].seq, 50u);
    EXPECT_EQ(outcome.to_replay[1].seq, 100u);
    EXPECT_EQ(outcome.to_replay[2].seq, 105u);
    EXPECT_EQ(outcome.to_replay[3].seq, 110u);
    EXPECT_EQ(outcome.internal_gaps, 0u);
}

TEST(ResyncCoordinator, SnapshotEventCarriesTheForwardedLevels) {
    ResyncCoordinator<FuturesAlignment> coord;
    auto                                first = diff(kBtc, 1, 100, 0);
    first.ts = 1786742884159;  // non-zero: proves .ts is actually copied, not just
                               // left at its (also zero) default
    coord.on_event(first);

    auto outcome = coord.on_snapshot(kBtc, 50, {{100.0, 1.5}, {99.5, 2.0}}, {{100.5, 3.0}});
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_FALSE(outcome.to_replay.empty());

    const MarketEvent& snapshot_event = outcome.to_replay[0];
    EXPECT_EQ(snapshot_event.kind, EventKind::BookSnapshot);
    EXPECT_EQ(snapshot_event.symbol, kBtc);
    EXPECT_EQ(snapshot_event.seq, 50u);  // the anchor point (last_update_id), not a diff seq
    // FileRecorder partitions by event.ts — a snapshot with its own default
    // (0) ts would land in a spurious 1970-01-01 partition instead of
    // alongside the diffs it anchors. Must match the first replayed diff's.
    EXPECT_EQ(snapshot_event.ts, first.ts);
    ASSERT_EQ(snapshot_event.bids.size(), 2u);
    EXPECT_EQ(snapshot_event.bids[0].price, 100.0);
    EXPECT_EQ(snapshot_event.bids[1].price, 99.5);
    ASSERT_EQ(snapshot_event.asks.size(), 1u);
    EXPECT_EQ(snapshot_event.asks[0].price, 100.5);
}

TEST(ResyncCoordinator, SnapshotThatBracketsAMiddleEventDropsTheStaleOnesBeforeIt) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_event(diff(kBtc, 101, 105, 100));
    coord.on_event(diff(kBtc, 106, 110, 105));

    // last_update_id=102: event 1 (seq=100) is stale (100 < 102), event 2
    // (U=101,u=105) brackets 102 -> replay starts from event 2, event 1 is
    // correctly dropped as already-reflected-in-the-snapshot.
    auto outcome = coord.on_snapshot(kBtc, 102);
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 3u);  // +1 for the BookSnapshot anchor
    EXPECT_EQ(outcome.to_replay[0].kind, EventKind::BookSnapshot);
    EXPECT_EQ(outcome.to_replay[1].seq, 105u);
    EXPECT_EQ(outcome.to_replay[2].seq, 110u);
}

TEST(ResyncCoordinator, StreamingAfterResyncForwardsDirectlyWithoutBuffering) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    auto outcome = coord.on_snapshot(kBtc, 50);
    ASSERT_FALSE(outcome.need_retry);

    // Now Streaming: a continuous event forwards immediately, no buffering.
    auto v = coord.on_event(diff(kBtc, 101, 105, 100));
    ASSERT_EQ(v.action, Action::Forward);
    ASSERT_TRUE(v.event.has_value());
    EXPECT_EQ(v.event->seq, 105u);
    EXPECT_FALSE(v.gap_detected);
}

// --- Recovering from a gap while already Streaming ---

TEST(ResyncCoordinator, GapWhileStreamingTriggersANewBufferingRound) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_snapshot(kBtc, 50);  // now Streaming, anchored at seq=100

    // Claims prev_seq=150, but the last recorded seq is 100 -> a gap.
    auto v = coord.on_event(diff(kBtc, 151, 200, 150));
    EXPECT_EQ(v.action, Action::BufferAndRequest);
    EXPECT_TRUE(v.gap_detected);
    EXPECT_FALSE(v.buffer_overflowed);  // a gap, not a capacity-driven restart
    EXPECT_FALSE(v.event.has_value());  // held, not forwarded
}

TEST(ResyncCoordinator, RecoversFullyAfterAGap) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_snapshot(kBtc, 50);  // Streaming, anchored at 100

    coord.on_event(diff(kBtc, 151, 200, 150));    // gap -> buffering again
    auto outcome = coord.on_snapshot(kBtc, 170);  // 151 <= 170 <= 200
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 2u);  // +1 for the BookSnapshot anchor
    EXPECT_EQ(outcome.to_replay[0].kind, EventKind::BookSnapshot);
    EXPECT_EQ(outcome.to_replay[1].seq, 200u);

    // And it's genuinely re-anchored: the next continuous event forwards
    // (this would spuriously re-flag as a gap if gap_detector_ hadn't been
    // reset to the new anchor).
    auto v = coord.on_event(diff(kBtc, 201, 205, 200));
    EXPECT_EQ(v.action, Action::Forward);
    EXPECT_FALSE(v.gap_detected);
}

// --- Snapshot that doesn't align: the retry path ---

TEST(ResyncCoordinator, SnapshotAheadOfEverythingBufferedNeedsRetry) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));

    auto outcome = coord.on_snapshot(kBtc, 500);  // every buffered event predates this
    EXPECT_TRUE(outcome.need_retry);
    EXPECT_TRUE(outcome.to_replay.empty());
}

TEST(ResyncCoordinator, SnapshotInAHoleNeedsRetry) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_event(
        diff(kBtc, 200, 210, 199));  // a gap inside the buffer itself, contrived on purpose

    // last_update_id = 149 falls in the hole between the two events (event 2 starts at U=200,
    // well past it).
    auto outcome = coord.on_snapshot(kBtc, 149);
    EXPECT_TRUE(outcome.need_retry);
}

TEST(ResyncCoordinator, RetryClearsTheBufferSoOnlyFreshEventsCountTowardTheNextAlignment) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    ASSERT_TRUE(coord.on_snapshot(kBtc, 500).need_retry);  // discarded, doesn't align

    // Feeding a fresh event after a failed retry starts a new buffering
    // round from empty — proves the retry path doesn't leave the buffer in
    // some half-cleared state, and confirms the (documented, deliberately
    // accepted) behavior: a quiet connection with a failed align keeps
    // buffering only what arrives *after* the retry, not what was lost.
    auto v = coord.on_event(diff(kBtc, 101, 105, 100));
    EXPECT_EQ(v.action, Action::BufferAndRequest);  // buffer was empty again -> requests again

    auto outcome = coord.on_snapshot(kBtc, 102);  // 101 <= 102 <= 105
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 2u);  // +1 for the BookSnapshot anchor
    EXPECT_EQ(outcome.to_replay[0].kind, EventKind::BookSnapshot);
    EXPECT_EQ(outcome.to_replay[1].seq, 105u);
}

// --- Buffer overflow: the other retry trigger ---

TEST(ResyncCoordinator, BufferOverflowClearsAndKeepsBufferingRatherThanGrowingUnbounded) {
    ResyncCoordinator<FuturesAlignment> coord;
    std::uint64_t                       seq = 0;

    // Fill to exactly capacity. Only the very first event (i == 0) should
    // request; the rest are ordinary Buffer actions accumulating.
    for (std::size_t i = 0; i < ResyncCoordinator<FuturesAlignment>::kBufferCapacity; ++i) {
        auto v = coord.on_event(diff(kBtc, seq + 1, seq + 5, seq));
        EXPECT_EQ(v.action, i == 0 ? Action::BufferAndRequest : Action::Buffer);
        EXPECT_FALSE(v.buffer_overflowed);  // none of these are the capacity-driven restart
        seq += 5;
    }

    // The event that pushes past capacity forces a clear-and-restart, which
    // — same as the very first event ever — requests again, rather than
    // growing the buffer without bound. This one specifically IS the
    // overflow case, distinguishable from a fresh/gap-triggered start.
    auto overflow_v = coord.on_event(diff(kBtc, seq + 1, seq + 5, seq));
    EXPECT_EQ(overflow_v.action, Action::BufferAndRequest);
    EXPECT_TRUE(overflow_v.buffer_overflowed);
    seq += 5;

    // A snapshot aligning against what's left (just the one event pushed
    // right after the clear) produces a small, coherent replay — proving
    // the overflow didn't leave stale pre-clear entries lying around.
    auto outcome = coord.on_snapshot(kBtc, seq - 4);
    ASSERT_FALSE(outcome.need_retry);
    EXPECT_EQ(outcome.to_replay.size(), 2u);  // +1 for the BookSnapshot anchor
}

// --- SpotAlignment, through the full coordinator, not just
// find_resync_point in isolation (test_resync.cpp) — proves the policy
// parameter actually changes ResyncCoordinator's real behavior end to end.

TEST(ResyncCoordinator, SpotPolicyRejectsTheExactBoundaryFuturesAccepts) {
    ResyncCoordinator<SpotAlignment> coord;
    coord.on_event(diff(kBtc, 996, 1000, 995));

    // Same buffered event + last_update_id as the futures
    // SnapshotThatBracketsFirstBufferedEventReplaysEverythingInOrder-style
    // case would accept; spot's +1 rule pushes past `u` here and finds a
    // hole instead.
    auto outcome = coord.on_snapshot(kBtc, 1000);
    EXPECT_TRUE(outcome.need_retry);
}

TEST(ResyncCoordinator, SpotPolicyResyncsSuccessfullyOneBelowThatBoundary) {
    ResyncCoordinator<SpotAlignment> coord;
    coord.on_event(diff(kBtc, 996, 1000, 995));

    auto outcome = coord.on_snapshot(kBtc, 999);  // 999+1 == 1000 == u: spot brackets
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 2u);  // +1 for the BookSnapshot anchor
    EXPECT_EQ(outcome.to_replay[1].seq, 1000u);
}

// --- D40: continuity checking (gap_detector, not find_resync_point) is
// also market-specific — spot diffs carry no `pu`, so continuity has to
// read off `U` chaining instead of a `pu` comparison.

TEST(ResyncCoordinator, SpotStreamingEventWithNoPuForwardsInsteadOfFalsePositivingAGap) {
    ResyncCoordinator<SpotAlignment> coord;
    coord.on_event(diff(kBtc, 996, 1000, /*prev_seq=*/0));  // spot: pu always 0
    ASSERT_FALSE(coord.on_snapshot(kBtc, 999).need_retry);  // now Streaming, anchored at seq=1000

    // Continuous by spot's rule (U == last seq + 1 == 1001), despite
    // prev_seq still being 0 — a real spot stream never sets it.
    auto v = coord.on_event(diff(kBtc, 1001, 1010, /*prev_seq=*/0));
    EXPECT_EQ(v.action, SpotAction::Forward);
    EXPECT_FALSE(v.gap_detected);
}

TEST(ResyncCoordinator, SameSpotShapedEventWouldFalsePositiveUnderFuturesAlignment) {
    // Same U/seq/prev_seq sequence as above, but through FuturesAlignment's
    // pu-based check — demonstrates why the rule has to be a real parameter,
    // not just defaulted: this would silently misfire if spot were ever
    // wired to the wrong rule.
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 996, 1000, /*prev_seq=*/0));
    ASSERT_FALSE(coord.on_snapshot(kBtc, 1000).need_retry);  // futures: no +1 offset

    auto v = coord.on_event(diff(kBtc, 1001, 1010, /*prev_seq=*/0));
    EXPECT_EQ(v.action, Action::BufferAndRequest);  // pu (0) != last seq (1000): false gap
    EXPECT_TRUE(v.gap_detected);
}

// --- Symbols are independent ---

TEST(ResyncCoordinator, OneSymbolResyncingDoesNotAffectAnother) {
    ResyncCoordinator<FuturesAlignment> coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_snapshot(kBtc, 50);  // BTC now Streaming

    // ETH starts completely independently.
    auto v = coord.on_event(diff(kEth, 1, 50, 0));
    EXPECT_EQ(v.action, Action::BufferAndRequest);

    // BTC keeps streaming normally throughout.
    auto btc_v = coord.on_event(diff(kBtc, 101, 105, 100));
    EXPECT_EQ(btc_v.action, Action::Forward);
}
