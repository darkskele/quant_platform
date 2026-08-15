#include "qp/marketdata/resync_coordinator.hpp"

#include <gtest/gtest.h>

using qp::EventKind;
using qp::MarketEvent;
using qp::ResyncCoordinator;
using Action = qp::ResyncCoordinator::Action;

namespace {

constexpr qp::SymbolId kBtc = 0;
constexpr qp::SymbolId kEth = 1;

MarketEvent diff(qp::SymbolId symbol, std::uint64_t first_seq, std::uint64_t seq, std::uint64_t prev_seq) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.symbol    = symbol;
    ev.first_seq = first_seq;
    ev.seq       = seq;
    ev.prev_seq  = prev_seq;
    return ev;
}

qp::venue::binance::DepthSnapshot snapshot(std::uint64_t last_update_id) {
    return {last_update_id, {}, {}};
}

}  // namespace

// --- Fresh connect: buffering starts immediately, one request per round ---

TEST(ResyncCoordinator, FirstEventForASymbolBuffersAndRequests) {
    ResyncCoordinator coord;
    auto              v = coord.on_event(diff(kBtc, 1, 100, 0));
    EXPECT_EQ(v.action, Action::BufferAndRequest);
    EXPECT_EQ(v.symbol, kBtc);
    EXPECT_FALSE(v.event.has_value());
}

TEST(ResyncCoordinator, SubsequentEventsWhileBufferingDontRequestAgain) {
    ResyncCoordinator coord;
    ASSERT_EQ(coord.on_event(diff(kBtc, 1, 100, 0)).action, Action::BufferAndRequest);
    EXPECT_EQ(coord.on_event(diff(kBtc, 101, 105, 100)).action, Action::Buffer);
    EXPECT_EQ(coord.on_event(diff(kBtc, 106, 110, 105)).action, Action::Buffer);
}

// --- The actual ask: snapshot -> correct delta -> correct replayed sequence ---

TEST(ResyncCoordinator, SnapshotThatBracketsFirstBufferedEventReplaysEverythingInOrder) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_event(diff(kBtc, 101, 105, 100));
    coord.on_event(diff(kBtc, 106, 110, 105));

    auto outcome = coord.on_snapshot(kBtc, snapshot(50));  // 1 <= 51 <= 100: brackets the first event
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 3u);
    EXPECT_EQ(outcome.to_replay[0].seq, 100u);
    EXPECT_EQ(outcome.to_replay[1].seq, 105u);
    EXPECT_EQ(outcome.to_replay[2].seq, 110u);
    EXPECT_EQ(outcome.internal_gaps, 0u);
}

TEST(ResyncCoordinator, SnapshotThatBracketsAMiddleEventDropsTheStaleOnesBeforeIt) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_event(diff(kBtc, 101, 105, 100));
    coord.on_event(diff(kBtc, 106, 110, 105));

    // last_update_id=102: event 1 (seq=100) is stale (100 < 102), event 2
    // (U=101,u=105) brackets 103 -> replay starts from event 2, event 1 is
    // correctly dropped as already-reflected-in-the-snapshot.
    auto outcome = coord.on_snapshot(kBtc, snapshot(102));
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 2u);
    EXPECT_EQ(outcome.to_replay[0].seq, 105u);
    EXPECT_EQ(outcome.to_replay[1].seq, 110u);
}

TEST(ResyncCoordinator, StreamingAfterResyncForwardsDirectlyWithoutBuffering) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    auto outcome = coord.on_snapshot(kBtc, snapshot(50));
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
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_snapshot(kBtc, snapshot(50));  // now Streaming, anchored at seq=100

    // Claims prev_seq=150, but the last recorded seq is 100 -> a gap.
    auto v = coord.on_event(diff(kBtc, 151, 200, 150));
    EXPECT_EQ(v.action, Action::BufferAndRequest);
    EXPECT_TRUE(v.gap_detected);
    EXPECT_FALSE(v.event.has_value());  // held, not forwarded
}

TEST(ResyncCoordinator, RecoversFullyAfterAGap) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_snapshot(kBtc, snapshot(50));  // Streaming, anchored at 100

    coord.on_event(diff(kBtc, 151, 200, 150));  // gap -> buffering again
    auto outcome = coord.on_snapshot(kBtc, snapshot(170));  // 151 <= 171 <= 200
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 1u);
    EXPECT_EQ(outcome.to_replay[0].seq, 200u);

    // And it's genuinely re-anchored: the next continuous event forwards
    // (this would spuriously re-flag as a gap if gap_detector_ hadn't been
    // reset to the new anchor).
    auto v = coord.on_event(diff(kBtc, 201, 205, 200));
    EXPECT_EQ(v.action, Action::Forward);
    EXPECT_FALSE(v.gap_detected);
}

// --- Snapshot that doesn't align: the retry path ---

TEST(ResyncCoordinator, SnapshotAheadOfEverythingBufferedNeedsRetry) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));

    auto outcome = coord.on_snapshot(kBtc, snapshot(500));  // every buffered event predates this
    EXPECT_TRUE(outcome.need_retry);
    EXPECT_TRUE(outcome.to_replay.empty());
}

TEST(ResyncCoordinator, SnapshotInAHoleNeedsRetry) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_event(diff(kBtc, 200, 210, 199));  // a gap inside the buffer itself, contrived on purpose

    // last_update_id+1 = 150 falls in the hole between the two events.
    auto outcome = coord.on_snapshot(kBtc, snapshot(149));
    EXPECT_TRUE(outcome.need_retry);
}

TEST(ResyncCoordinator, RetryClearsTheBufferSoOnlyFreshEventsCountTowardTheNextAlignment) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    ASSERT_TRUE(coord.on_snapshot(kBtc, snapshot(500)).need_retry);  // discarded, doesn't align

    // Feeding a fresh event after a failed retry starts a new buffering
    // round from empty — proves the retry path doesn't leave the buffer in
    // some half-cleared state, and confirms the (documented, deliberately
    // accepted) behavior: a quiet connection with a failed align keeps
    // buffering only what arrives *after* the retry, not what was lost.
    auto v = coord.on_event(diff(kBtc, 101, 105, 100));
    EXPECT_EQ(v.action, Action::BufferAndRequest);  // buffer was empty again -> requests again

    auto outcome = coord.on_snapshot(kBtc, snapshot(102));  // 101 <= 103 <= 105
    ASSERT_FALSE(outcome.need_retry);
    ASSERT_EQ(outcome.to_replay.size(), 1u);
    EXPECT_EQ(outcome.to_replay[0].seq, 105u);
}

// --- Buffer overflow: the other retry trigger ---

TEST(ResyncCoordinator, BufferOverflowClearsAndKeepsBufferingRatherThanGrowingUnbounded) {
    ResyncCoordinator coord;
    std::uint64_t     seq = 0;

    // Fill to exactly capacity. Only the very first event (i == 0) should
    // request; the rest are ordinary Buffer actions accumulating.
    for (std::size_t i = 0; i < ResyncCoordinator::kBufferCapacity; ++i) {
        auto v = coord.on_event(diff(kBtc, seq + 1, seq + 5, seq));
        EXPECT_EQ(v.action, i == 0 ? Action::BufferAndRequest : Action::Buffer);
        seq += 5;
    }

    // The event that pushes past capacity forces a clear-and-restart, which
    // — same as the very first event ever — requests again, rather than
    // growing the buffer without bound.
    auto overflow_v = coord.on_event(diff(kBtc, seq + 1, seq + 5, seq));
    EXPECT_EQ(overflow_v.action, Action::BufferAndRequest);
    seq += 5;

    // A snapshot aligning against what's left (just the one event pushed
    // right after the clear) produces a small, coherent replay — proving
    // the overflow didn't leave stale pre-clear entries lying around.
    auto outcome = coord.on_snapshot(kBtc, snapshot(seq - 4));
    ASSERT_FALSE(outcome.need_retry);
    EXPECT_EQ(outcome.to_replay.size(), 1u);
}

// --- Symbols are independent ---

TEST(ResyncCoordinator, OneSymbolResyncingDoesNotAffectAnother) {
    ResyncCoordinator coord;
    coord.on_event(diff(kBtc, 1, 100, 0));
    coord.on_snapshot(kBtc, snapshot(50));  // BTC now Streaming

    // ETH starts completely independently.
    auto v = coord.on_event(diff(kEth, 1, 50, 0));
    EXPECT_EQ(v.action, Action::BufferAndRequest);

    // BTC keeps streaming normally throughout.
    auto btc_v = coord.on_event(diff(kBtc, 101, 105, 100));
    EXPECT_EQ(btc_v.action, Action::Forward);
}
