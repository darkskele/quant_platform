#include "qp/core/spsc_queue.hpp"
#include "qp/marketdata/gap_detector.hpp"
#include "qp/venue/binance.hpp"

#include <benchmark/benchmark.h>

using namespace qp;
using namespace qp::venue::binance;

namespace {

// Real capture (same message as BinanceParser.DepthUpdateRealCapture in
// libs/venue/tests/test_binance_parser.cpp) — 16 bids, 8 asks.
constexpr const char* kRealDepthMsg =
    R"({"stream":"ethusdt@depth@100ms","data":{"e":"depthUpdate","E":1786742884159,)"
    R"("T":1786742884158,"s":"ETHUSDT","ps":"ETHUSDT","U":11289273841677,)"
    R"("u":11289273848795,"pu":11289273841549,)"
    R"("b":[["200.00","57.006"],["1353.67","4.685"],["1353.77","0.000"],)"
    R"(["1373.67","4.133"],["1690.80","0.040"],["1690.81","3.261"],)"
    R"(["1778.67","0.032"],["1803.50","0.186"],["1849.55","14.613"],)"
    R"(["1875.11","0.982"],["1876.76","0.978"],["1877.74","12.794"],)"
    R"(["1877.87","44.287"],["1878.10","0.335"],["1878.42","20.618"],)"
    R"(["1878.49","4.477"]],)"
    R"("a":[["1879.10","0.035"],["1897.68","10.462"],["1897.72","34.050"],)"
    R"(["1909.41","0.550"],["1917.19","13.544"],["1978.68","0.173"],)"
    R"(["2866.35","0.001"],["2866.44","0.000"]],"st":1}})";

// End-to-end cost of one message on the I/O thread's hot path: parse, gap
// check, queue push. NOT the socket read itself — that's the OS/network
// stack, not ours to optimize (see MISSION.md — not chasing tick-to-trade
// speed). This is the part that has to keep up with arrival rate
// regardless of how fast the network delivers.
void BM_ProcessMessage(benchmark::State& state) {
    SymbolTable                  symbols;
    SequenceGapDetector           gaps;
    SpscQueue<MarketEvent, 1024> queue;

    for (auto _ : state) {
        MarketEvent ev;
        bool        ok = parse_message(kRealDepthMsg, symbols, ev);
        benchmark::DoNotOptimize(ok);

        if (ev.kind == EventKind::BookDiff) {
            auto gap = gaps.check_and_record(ev.symbol, ev.prev_seq, ev.seq);
            benchmark::DoNotOptimize(gap);
        }

        bool pushed = queue.push(std::move(ev));
        benchmark::DoNotOptimize(pushed);
        auto popped = queue.pop();  // drain so the queue doesn't fill across iterations
        benchmark::DoNotOptimize(popped);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ProcessMessage);

// Isolated steady-state cost of check_and_record itself — same symbol,
// always-continuous sequence, so this measures exactly the try_emplace +
// [[likely]] path, not parse/queue overhead mixed in. The number this
// should justify: the single-lookup + branch-hint rewrite over the
// original find()-then-operator[] version.
void BM_GapDetectorSteadyState(benchmark::State& state) {
    SequenceGapDetector gaps;
    gaps.check_and_record(1, 0, 100);  // seed — first event, establishes the tracked baseline
    std::uint64_t seq = 100;
    for (auto _ : state) {
        auto gap = gaps.check_and_record(1, seq, seq + 5);
        benchmark::DoNotOptimize(gap);
        seq += 5;
    }
}
BENCHMARK(BM_GapDetectorSteadyState);

}  // namespace

BENCHMARK_MAIN();
