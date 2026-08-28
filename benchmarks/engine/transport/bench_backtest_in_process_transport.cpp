#include <benchmark/benchmark.h>

#include <memory>

#include "backtest_in_process_transport.hpp"
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

using Ring = qp::SpmcRing<std::shared_ptr<const qp::MarketEvent>, 1024, 1>;

// The motivating shape: two real rings (a carry strategy's spot + perp
// legs), merged in timestamp order — the only merge policy anything real
// actually uses (apps/backtest; a live composition would use the same).
// Monotonically increasing timestamps so every next() call finds a winner
// immediately — isolates the merge's own steady-state cost, not the
// empty-ring stall path.
void BM_BacktestInProcessTransport_TwoRings(benchmark::State& state) {
    Ring                                                  ring_a, ring_b;
    qp::ControlChannel<1>                                 control;
    auto                                                  idx = control.attach();
    qp::transport::BacktestInProcessTransport<Ring, 2, 1> transport({&ring_a, &ring_b}, {0, 0},
                                                                    control, idx);

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

BENCHMARK(BM_BacktestInProcessTransport_TwoRings);

}  // namespace
