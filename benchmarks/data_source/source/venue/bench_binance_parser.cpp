#include <benchmark/benchmark.h>

#include "binance.hpp"

using namespace qp;
using namespace qp::venue::binance;

namespace {

// Small synthetic diff — a couple of levels, close to a quiet-book update.
constexpr const char* kSmallDepthMsg =
    R"({"stream":"btcusdt@depth@100ms","data":{)"
    R"("e":"depthUpdate","E":1723660800123,"T":1723660800120,"s":"BTCUSDT",)"
    R"("U":157,"u":160,"pu":149,)"
    R"("b":[["61000.10","1.500"],["60999.90","0.020"]],)"
    R"("a":[["61000.20","2.300"]]}})";

// Real capture (same message as BinanceParser.DepthUpdateRealCapture in
// tests/test_binance_parser.cpp) — 16 bids, 8 asks, a busier book update.
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

constexpr const char* kAggTradeMsg =
    R"({"stream":"btcusdt@aggTrade","data":{)"
    R"("e":"aggTrade","E":1786742457242,"s":"BTCUSDT","a":3408644882,)"
    R"("p":"62859.00","q":"0.938","f":7972465064,"l":7972465066,)"
    R"("T":1786742457242,"m":false}})";

// `ev` is reused across iterations rather than constructed fresh per call —
// parse_message() is written around clear()-then-refill (see binance.cpp),
// so this matches its intended calling convention and avoids charging the
// benchmark for an allocation pattern the API isn't actually optimized for.

void BM_ParseSmallDepthUpdate(benchmark::State& state) {
    SymbolTable symbols;
    MarketEvent ev;
    for (auto _ : state) {
        bool ok = parse_message(kSmallDepthMsg, symbols, ev);
        benchmark::DoNotOptimize(ok);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ParseSmallDepthUpdate);

void BM_ParseRealDepthUpdate(benchmark::State& state) {
    SymbolTable symbols;
    MarketEvent ev;
    for (auto _ : state) {
        bool ok = parse_message(kRealDepthMsg, symbols, ev);
        benchmark::DoNotOptimize(ok);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ParseRealDepthUpdate);

void BM_ParseAggTrade(benchmark::State& state) {
    SymbolTable symbols;
    MarketEvent ev;
    for (auto _ : state) {
        bool ok = parse_message(kAggTradeMsg, symbols, ev);
        benchmark::DoNotOptimize(ok);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ParseAggTrade);

}  // namespace
