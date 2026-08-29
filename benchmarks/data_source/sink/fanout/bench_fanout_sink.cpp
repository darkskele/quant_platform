#include <benchmark/benchmark.h>

#include <cstddef>
#include <optional>

#include "fanout_sink.hpp"
#include "types.hpp"

using qp::data_source::sink::fanout::FanoutSink;

namespace {

qp::MarketEvent make_trade() {
    qp::MarketEvent ev;
    ev.kind  = qp::EventKind::Trade;
    ev.price = 100.0;
    ev.qty   = 1.0;
    return ev;
}

// Isolation: record() alone, one consumer draining periodically. record()
// wraps SpmcRing::push (gated, blocks if the slowest consumer hasn't
// caught up) — same reasoning as bench_spmc_ring.cpp's BM_Spmc_PushInt:
// drain proactively, comfortably under the 1024 capacity, so record()
// never actually enters the wait path during a timed iteration.
void BM_FanoutSink_Record(benchmark::State& state) {
    FanoutSink<1024, 1> sink;
    auto                consumer = sink.attach();
    int                 i        = 0;
    for (auto _ : state) {
        if (i % 1000 == 0 && i != 0) {
            state.PauseTiming();
            while (sink.ring().try_pop(consumer)) {
            }
            state.ResumeTiming();
        }
        sink.record(make_trade());
        ++i;
    }
}

BENCHMARK(BM_FanoutSink_Record);

// Tandem: record() then drain via the ring directly, single thread, always
// caught up — the per-call overhead floor (make_shared allocation + ring
// bookkeeping), same shape as SpmcRing's own tandem bench.
void BM_FanoutSink_RecordAndDrain(benchmark::State& state) {
    FanoutSink<1024, 1> sink;
    auto                consumer = sink.attach();
    for (auto _ : state) {
        sink.record(make_trade());
        auto v = sink.ring().try_pop(consumer);
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_FanoutSink_RecordAndDrain);

// Contention: one shared sink, NumConsumers+1 real OS threads — thread 0
// is the producer (record()), threads 1..NumConsumers are independent real
// consumers reading the ring directly, each spinning against the
// producer's pace on its own cursor. Same pattern as bench_spmc_ring.cpp's
// BM_Spmc_PushTryPopContended, through FanoutSink's actual public API
// (record(), the make_shared allocation included) instead of the bare ring.
template <std::size_t NumConsumers>
void BM_FanoutSink_RecordContended(benchmark::State& state) {
    static FanoutSink<1024, NumConsumers> sink;

    if (state.thread_index() == 0) {
        for (std::size_t c = 0; c < NumConsumers; ++c) {
            while (sink.ring().try_pop(c)) {
            }
        }
    }

    if (state.thread_index() == 0) {
        for (auto _ : state) sink.record(make_trade());
    } else {
        auto consumer = static_cast<std::size_t>(state.thread_index() - 1);
        for (auto _ : state) {
            std::optional<std::shared_ptr<const qp::MarketEvent>> v;
            while (!(v = sink.ring().try_pop(consumer))) {
            }
            benchmark::DoNotOptimize(v);
        }
    }
}

BENCHMARK(BM_FanoutSink_RecordContended<1>)->Threads(2);
BENCHMARK(BM_FanoutSink_RecordContended<4>)->Threads(5);

}  // namespace
