#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

#include "resync_coordinator.hpp"
#include "resync_policy.hpp"
#include "types.hpp"

using namespace qp;
using qp::source::FuturesAlignment;
using qp::source::ResyncCoordinator;

namespace {

constexpr SymbolId kSymbol = 1;

MarketEvent make_diff(std::uint64_t first_seq, std::uint64_t seq) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.symbol    = kSymbol;
    ev.first_seq = first_seq;
    ev.seq       = seq;
    return ev;
}

// Isolation: on_event(), Forward path — the steady-state case once a
// symbol has completed resync and is Streaming, continuous sequence, no
// gap. Every symbol starts Buffering by default (resync_coordinator.hpp),
// so getting here needs one buffered event plus one on_snapshot() call
// that successfully aligns against it, same as a real resync completing.
void BM_ResyncCoordinator_OnEventForward(benchmark::State& state) {
    ResyncCoordinator<FuturesAlignment> coordinator;
    coordinator.on_event(make_diff(100, 110));  // buffers (still Buffering)
    coordinator.on_snapshot(kSymbol, 105);      // brackets(100,105,110) -> Streaming

    std::uint64_t seq = 110;
    for (auto _ : state) {
        auto verdict = coordinator.on_event(make_diff(seq, seq + 5));
        benchmark::DoNotOptimize(verdict);
        seq += 5;
    }
}

BENCHMARK(BM_ResyncCoordinator_OnEventForward);

// Isolation: on_event(), Buffering path — a symbol that hasn't resynced
// yet (or is mid-resync). Self-bounding: buffer() clears itself at
// kBufferCapacity (256), so no periodic external reset is needed the way
// the core queues' isolation benches need one.
void BM_ResyncCoordinator_OnEventBuffering(benchmark::State& state) {
    ResyncCoordinator<FuturesAlignment> coordinator;
    std::uint64_t                       seq = 0;
    for (auto _ : state) {
        auto verdict = coordinator.on_event(make_diff(seq, seq + 1));
        benchmark::DoNotOptimize(verdict);
        ++seq;
    }
}

BENCHMARK(BM_ResyncCoordinator_OnEventBuffering);

// Isolation: find_resync_point() alone — the free function on_snapshot()
// calls internally. A small buffered vector (4 events), resume point in
// the middle, matching a realistic post-gap buffer depth better than an
// empty or single-element one.
void BM_ResyncCoordinator_FindResyncPoint(benchmark::State& state) {
    std::vector<MarketEvent> buffered;
    for (std::uint64_t i = 0; i < 4; ++i) buffered.push_back(make_diff(100 + i * 10, 109 + i * 10));

    for (auto _ : state) {
        auto idx = source::find_resync_point<FuturesAlignment>(115, buffered);
        benchmark::DoNotOptimize(idx);
    }
}

BENCHMARK(BM_ResyncCoordinator_FindResyncPoint);

}  // namespace
