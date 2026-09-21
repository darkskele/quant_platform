#include <benchmark/benchmark.h>

#include <optional>
#include <utility>

#include "spsc_queue.hpp"
#include "types.hpp"

namespace {

// Three tiers, in order: each op in isolation (push alone, pop alone) ->
// both together on one thread ("tandem", no real concurrency) -> both
// together on two real threads ("contended", see BM_Spsc_PushPopContended
// below). Isolation and tandem give different numbers because a queue's
// push and pop touch different atomics in opposite directions (push:
// relaxed-load head_, acquire-load tail_, release-store head_; pop: mirror
// image) — measuring them together on one thread hides whichever one the
// branch predictor / store buffer happens to favor when they alternate.

// Push alone. Periodically (untimed, via PauseTiming) drains the queue so
// push() never spends timed iterations returning false because the queue
// filled up — that would measure "how fast does push() detect full", not
// push()'s real cost. Deliberately NOT draining every iteration (tempting:
// "then we're only timing one push and nothing else") — PauseTiming/
// ResumeTiming are themselves slow (measured: ~270ns/call on this box, a
// real syscall under the hood, not free bookkeeping), so pausing every
// iteration would make the "isolated" push cost mostly Pause/Resume
// overhead leaking around the boundary — the exact clock-overhead-swamps-
// the-signal mistake D51 already burned time on, just via a different
// clock call. Draining only once every ~1023 pushes amortizes that cost
// to a small fraction of a nanosecond per push instead.
void BM_Spsc_PushInt(benchmark::State& state) {
    qp::SpscQueue<int, 1024> q;
    int                      i = 0;
    for (auto _ : state) {
        bool pushed = q.push(i);
        if (!pushed) {
            state.PauseTiming();
            while (q.pop()) {
            }
            state.ResumeTiming();
            pushed = q.push(i);
        }
        // DoNotOptimize on the actual return value — matches
        // BM_Spsc_PopInt's own methodology. Verified this isn't a
        // measurement artifact two ways (this, and a plain ClobberMemory()
        // beforehand) — neither changed the number. Push genuinely does
        // cost roughly 1ns here, well under pop's ~7-8ns; both isolated
        // benchmarks touch the same shape of work (two atomics + one
        // storage access), so the gap is real, not unmeasured work —
        // exact root cause (store-to-load forwarding, std::launder's
        // effect on pop's aliasing, something else) not pinned down
        // further than that.
        benchmark::DoNotOptimize(pushed);
        ++i;
    }
}

BENCHMARK(BM_Spsc_PushInt);

// Pop alone. Mirror image of BM_Spsc_PushInt: periodically (untimed) refills so
// pop() never spends timed iterations against an empty queue.
void BM_Spsc_PopInt(benchmark::State& state) {
    qp::SpscQueue<int, 1024> q;
    for (int j = 0; j < 1023; ++j) q.push(j);
    for (auto _ : state) {
        auto v = q.pop();
        if (!v) {
            state.PauseTiming();
            for (int j = 0; j < 1023; ++j) q.push(j);
            state.ResumeTiming();
            v = q.pop();
        }
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Spsc_PopInt);

// Tandem: single-threaded push-then-pop round trip — no real cross-thread
// contention, so this is the per-operation overhead floor (ring-buffer
// bookkeeping + atomics + object construction), not a measure of
// throughput under actual producer/consumer contention.

// Templated on UseHeap, both instantiated below, to actually measure
// SpscQueue's own "costs the same either way" UseHeap doc comment instead
// of just asserting it.
template <bool UseHeap>
void BM_Spsc_PushPopInt(benchmark::State& state) {
    qp::SpscQueue<int, 1024, UseHeap> q;
    int                               i = 0;
    for (auto _ : state) {
        bool pushed = q.push(i);
        benchmark::DoNotOptimize(pushed);
        auto v = q.pop();
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_Spsc_PushPopInt<false>);
BENCHMARK(BM_Spsc_PushPopInt<true>);

template <bool UseHeap>
void BM_Spsc_PushPopMarketEvent(benchmark::State& state) {
    qp::SpscQueue<qp::MarketEvent, 1024, UseHeap> q;

    qp::BookDepthEvent seed;
    seed.bands = std::make_shared<const qp::BookDepthBands>();

    for (auto _ : state) {
        qp::MarketEvent fresh{.base = {.kind = qp::EventKind::BookDepth}, .payload = seed};
        bool            pushed = q.push(std::move(fresh));
        benchmark::DoNotOptimize(pushed);
        auto v = q.pop();
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Spsc_PushPopMarketEvent<false>);
BENCHMARK(BM_Spsc_PushPopMarketEvent<true>);

// Real contention.
void BM_Spsc_PushPopContended(benchmark::State& state) {
    static qp::SpscQueue<int, 1024> q;
    if (state.thread_index() == 0) {
        while (q.pop()) {
        }
    }

    if (state.thread_index() == 0) {
        int i = 0;
        for (auto _ : state) {
            while (!q.push(i)) {
            }
            ++i;
        }
    } else {
        for (auto _ : state) {
            std::optional<int> v;
            while (!(v = q.pop())) {
            }
            benchmark::DoNotOptimize(v);
        }
    }
}

BENCHMARK(BM_Spsc_PushPopContended)->Threads(2);

}  // namespace
