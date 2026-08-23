#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>

#include "combined_transport.hpp"
#include "in_process_transport.hpp"
#include "spmc_ring.hpp"
#include "types.hpp"

namespace {

qp::MarketEvent make_trade() {
    qp::MarketEvent ev;
    ev.kind  = qp::EventKind::Trade;
    ev.price = 100.0;
    ev.qty   = 1.0;
    return ev;
}

using Ring      = qp::SpmcRing<std::shared_ptr<const qp::MarketEvent>, 1024, 1>;
using OneReader = qp::transport::InProcessTransport<Ring, 0>;

// The floor this lib sits on top of: bench_spmc_ring.cpp's
// BM_PushTryPopSharedMarketEvent measures the bare ring; this adds
// InProcessTransport::next()'s own cost (deref + MarketEvent copy out of
// the shared_ptr slot, required to satisfy Transport's by-value contract).
void BM_InProcessTransportNext(benchmark::State& state) {
    Ring      ring;
    OneReader transport(ring);
    auto      seed = make_trade();

    for (auto _ : state) {
        ring.push(std::make_shared<const qp::MarketEvent>(seed));
        auto event = transport.next();
        benchmark::DoNotOptimize(event);
    }
}

BENCHMARK(BM_InProcessTransportNext);

// Same ring, wrapped in CombinedTransport<OneReader> with exactly one
// source: isolates what the round-robin dispatch layer (function-pointer
// table + modulo bookkeeping) costs on top of BM_InProcessTransportNext
// when there's nothing to actually merge.
void BM_CombinedTransportSingleSource(benchmark::State& state) {
    Ring                                        ring;
    qp::transport::CombinedTransport<OneReader> combined{OneReader{ring}};
    auto                                        seed = make_trade();

    for (auto _ : state) {
        ring.push(std::make_shared<const qp::MarketEvent>(seed));
        auto event = combined.next();
        benchmark::DoNotOptimize(event);
    }
}

BENCHMARK(BM_CombinedTransportSingleSource);

// Two real rings merged round-robin — the motivating shape (a carry
// strategy's spot leg + perp leg as one Engine::Tx). Both pushed every
// iteration so next() always has something to hand back on each call;
// measures the realistic per-event cost with two legs actually in play,
// not an artificially-starved rotation.
void BM_CombinedTransportTwoSources(benchmark::State& state) {
    Ring                                                   ring_a, ring_b;
    qp::transport::CombinedTransport<OneReader, OneReader> combined{OneReader{ring_a},
                                                                    OneReader{ring_b}};
    auto                                                   seed = make_trade();

    for (auto _ : state) {
        ring_a.push(std::make_shared<const qp::MarketEvent>(seed));
        ring_b.push(std::make_shared<const qp::MarketEvent>(seed));
        auto first  = combined.next();
        auto second = combined.next();
        benchmark::DoNotOptimize(first);
        benchmark::DoNotOptimize(second);
    }
}

BENCHMARK(BM_CombinedTransportTwoSources);

}  // namespace
