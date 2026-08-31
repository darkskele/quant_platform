#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string_view>

#include "bin_hist_venue.hpp"

using qp::data_source::source::venue::binance::binance_historical::BinHistVenue;

namespace {

std::span<const std::byte> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

// Real captured lines, post-tagging (the SYMBOL,KIND, prefix
// BinHistParserPolicy::parse() now expects — see its own comment for why:
// klines/markPriceKlines are byte-identical CSV shapes, so the tag is what
// makes dispatch possible at all). Pulled directly from the downloaded
// data/binance/BTCUSDT files, not fabricated.
constexpr std::string_view kRealKlineLine =
    "BTCUSDT,K,1717200000000,67577.90,67680.70,67572.00,67680.40,464.748,1717200299999,"
    "31428593.42840,6933,289.996,19611103.65640,0";

constexpr std::string_view kRealMarkLine =
    "BTCUSDT,M,1717200000000,67570.93117730,67680.70000000,67570.93117730,67678.20000000,0,"
    "1717200299999,0,300,0,0,0";

constexpr std::string_view kRealFundingLine = "BTCUSDT,F,1577836800000,8,-0.00012359";

constexpr std::string_view kUnknownSymbolKline =
    "NOTASYMBOL,K,1717200000000,67577.90,67680.70,67572.00,67680.40,464.748,1717200299999,"
    "31428593.42840,6933,289.996,19611103.65640,0";

// Isolation: the kline path's success case.
void BM_BinHistParser_ParsesKline(benchmark::State& state) {
    auto raw = as_bytes(kRealKlineLine);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_ParsesKline);

// Isolation: the markPriceKline path's success case — same 7-field shape
// as kline, one field fewer actually stored (volume dropped). Comparison
// point for whether the extra kind-tag split costs anything measurable
// over the kline path above.
void BM_BinHistParser_ParsesMark(benchmark::State& state) {
    auto raw = as_bytes(kRealMarkLine);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_ParsesMark);

// Isolation: the funding path's success case — now genuinely CSV (3
// fields), not the old JSON/simdjson path this used to measure. Baseline
// for the rewrite, not the ~400ns/9ns-reject numbers from the JSON era.
void BM_BinHistParser_ParsesFunding(benchmark::State& state) {
    auto raw = as_bytes(kRealFundingLine);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_ParsesFunding);

// Isolation: symbol miss on the kline path — pays the full field split
// (parse() reads the symbol field first now, before any kind dispatch, so
// this actually rejects earlier than before: no comma-splitting of the
// remaining 7 fields happens at all once Table::id_of fails).
void BM_BinHistParser_RejectsUnknownSymbol(benchmark::State& state) {
    auto raw = as_bytes(kUnknownSymbolKline);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_RejectsUnknownSymbol);

}  // namespace
