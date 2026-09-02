#include <benchmark/benchmark.h>

#include <cstddef>
#include <optional>

#include "fanout_sink.hpp"
#include "types.hpp"

using qp::data_source::sink::fanout::FanoutSink;

namespace {

qp::MarketEvent make_trade() {
    qp::TradeEvent ev;
    ev.price = 100.0;
    ev.qty   = 1.0;
    return ev;
}

// Isolation: record() alone, one consumer draining periodically.
void BM_FanoutSink_Record(benchmark::State& state) {
    FanoutSink<1024, 1> sink;
    std::size_t         consumer = 0;  // sole consumer, compile-time known
    int                 i        = 0;
    for (auto _ : state) {
        if (i % 1000 == 0 && i != 0) {
            state.PauseTiming();
            while (sink.queue().try_pop(consumer)) {
            }
            state.ResumeTiming();
        }
        bool ok = sink.record(make_trade());
        benchmark::DoNotOptimize(ok);
        ++i;
    }
}

BENCHMARK(BM_FanoutSink_Record);

// Tandem: record() then drain via the queue directly, single thread,
// always caught up.
void BM_FanoutSink_RecordAndDrain(benchmark::State& state) {
    FanoutSink<1024, 1> sink;
    std::size_t         consumer = 0;  // sole consumer, compile-time known
    for (auto _ : state) {
        sink.record(make_trade());
        auto v = sink.queue().try_pop(consumer);
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_FanoutSink_RecordAndDrain);

template <std::size_t NumConsumers>
void BM_FanoutSink_RecordContended(benchmark::State& state) {
    static FanoutSink<1024, NumConsumers> sink;

    if (state.thread_index() == 0) {
        for (std::size_t c = 0; c < NumConsumers; ++c) {
            while (sink.queue().try_pop(c)) {
            }
        }
    }

    if (state.thread_index() == 0) {
        for (auto _ : state) {
            while (!sink.record(make_trade())) {
            }
        }
    } else {
        auto consumer = static_cast<std::size_t>(state.thread_index() - 1);
        for (auto _ : state) {
            std::optional<qp::MarketEvent> v;
            while (!(v = sink.queue().try_pop(consumer))) {
            }
            benchmark::DoNotOptimize(v);
        }
    }
}

BENCHMARK(BM_FanoutSink_RecordContended<1>)->Threads(2);
BENCHMARK(BM_FanoutSink_RecordContended<4>)->Threads(5);

}  // namespace
