#include <benchmark/benchmark.h>

#include <atomic>
#include <optional>
#include <tuple>

#include "run_data_source.hpp"
#include "types.hpp"

using namespace qp;

namespace {

// run_data_source() owns a while(running) loop itself — there's no single-
// call granularity to put inside a plain `for (auto _ : state)` body the
// way every other benchmark in this suite works. Instead, each timed
// iteration runs it to completion over a fixed round count (the source
// flips `running` false itself after kRounds calls), and
// SetItemsProcessed() reports the per-round throughput — the honest unit
// for a function whose public shape is "drive to completion," not
// "compute one thing and return."
constexpr int kRounds = 1000;

struct CountingSource {
    int                remaining;
    std::atomic<bool>* running;

    std::optional<MarketEvent> next() {
        if (--remaining <= 0) running->store(false, std::memory_order_release);
        MarketEvent ev;
        ev.kind = EventKind::Trade;
        return ev;
    }
};

// Counts, doesn't discard: a no-op record() would give the optimizer
// nothing to observe from the whole run except the final running->store,
// and everything in between (predictable, closed-form decrements) is
// exactly the shape an aggressive optimizer can prove equivalent to a
// closed-form answer and shortcut entirely, skipping the loop altogether —
// measured 486ns for a nominal 1000 rounds (0.5ns/round) before this fix,
// an unrealistic number that was really "the compiler proved this loop's
// only effect and computed it directly." Accumulating into `count` and
// forcing both it and the round count through DoNotOptimize (making the
// trip count opaque, not a compile-time constant to fold against) closes
// both ends of that shortcut.
struct CountingSink {
    long count = 0;

    void record(MarketEvent) { ++count; }
};

// One source/sink pair — the floor: fold-expansion dispatch overhead for
// N=1.
void BM_RunDataSource_OnePair(benchmark::State& state) {
    for (auto _ : state) {
        int rounds = kRounds;
        benchmark::DoNotOptimize(rounds);
        std::atomic<bool>          running{true};
        std::tuple<CountingSource> sources{CountingSource{rounds, &running}};
        std::tuple<CountingSink>   sinks{CountingSink{}};
        run_data_source(sources, sinks, running);
        benchmark::DoNotOptimize(std::get<0>(sinks).count);
    }
    state.SetItemsProcessed(state.iterations() * kRounds);
}

BENCHMARK(BM_RunDataSource_OnePair);

// Four source/sink pairs — how the fold-expansion dispatch cost actually
// scales with pack size (the real shape: a carry strategy's spot + perp
// legs is N=2 today, run_data_source.hpp's own doc comment).
void BM_RunDataSource_FourPairs(benchmark::State& state) {
    for (auto _ : state) {
        int rounds = kRounds;
        benchmark::DoNotOptimize(rounds);
        std::atomic<bool>                                                          running{true};
        std::tuple<CountingSource, CountingSource, CountingSource, CountingSource> sources{
            CountingSource{rounds, &running}, CountingSource{rounds, &running},
            CountingSource{rounds, &running}, CountingSource{rounds, &running}};
        std::tuple<CountingSink, CountingSink, CountingSink, CountingSink> sinks{};
        run_data_source(sources, sinks, running);
        benchmark::DoNotOptimize(std::get<0>(sinks).count + std::get<1>(sinks).count +
                                 std::get<2>(sinks).count + std::get<3>(sinks).count);
    }
    state.SetItemsProcessed(state.iterations() * kRounds * 4);
}

BENCHMARK(BM_RunDataSource_FourPairs);

}  // namespace
