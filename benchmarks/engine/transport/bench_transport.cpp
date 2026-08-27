#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>

#include "backtest_in_process_transport.hpp"
#include "combined_transport.hpp"
#include "control_channel.hpp"
#include "spmc_ring.hpp"
#include "types.hpp"

namespace {

qp::MarketEvent make_trade(qp::Timestamp ts = 0) {
    qp::MarketEvent ev;
    ev.kind  = qp::EventKind::Trade;
    ev.ts    = ts;
    ev.price = 100.0;
    ev.qty   = 1.0;
    return ev;
}

// A plain Source-shaped fake (no ring involved) — isolates what
// CombinedTransport's round-robin dispatch layer costs on its own.
struct FakeSource {
    std::optional<qp::MarketEvent> next() { return make_trade(); }
};

void BM_CombinedTransportSingleSource(benchmark::State& state) {
    qp::transport::CombinedTransport<FakeSource> combined{FakeSource{}};
    for (auto _ : state) {
        auto event = combined.next();
        benchmark::DoNotOptimize(event);
    }
}
BENCHMARK(BM_CombinedTransportSingleSource);

// Two sources merged round-robin — the shape CombinedTransport was built for.
void BM_CombinedTransportTwoSources(benchmark::State& state) {
    qp::transport::CombinedTransport<FakeSource, FakeSource> combined{FakeSource{}, FakeSource{}};
    for (auto _ : state) {
        auto event = combined.next();
        benchmark::DoNotOptimize(event);
    }
}
BENCHMARK(BM_CombinedTransportTwoSources);

using Ring = qp::SpmcRing<std::shared_ptr<const qp::MarketEvent>, 1024, 1>;

// The motivating shape: two real rings (a carry strategy's spot + perp
// legs), merged in timestamp order rather than round-robin. Monotonically
// increasing timestamps so every next() call finds a winner immediately —
// isolates the merge's own steady-state cost, not the empty-ring stall path.
void BM_BacktestInProcessTransportTwoRings(benchmark::State& state) {
    Ring ring_a, ring_b;
    qp::ControlChannel<1> control;
    auto                   idx = control.attach();
    qp::transport::BacktestInProcessTransport<Ring, 2, 1> transport({&ring_a, &ring_b}, {0, 0}, control, idx);

    qp::Timestamp ts = 0;
    for (auto _ : state) {
        ring_a.push(std::make_shared<const qp::MarketEvent>(make_trade(ts++)));
        ring_b.push(std::make_shared<const qp::MarketEvent>(make_trade(ts++)));
        auto first  = transport.next();
        auto second = transport.next();
        benchmark::DoNotOptimize(first);
        benchmark::DoNotOptimize(second);
    }
}
BENCHMARK(BM_BacktestInProcessTransportTwoRings);

}  // namespace
