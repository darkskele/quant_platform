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

// Real shapes, same captures test_bin_hist_venue.cpp verifies against —
// see bin_hist_parser_policy.hpp's own comments for the live source of each.
constexpr std::string_view kRealFundingEntry =
    R"({"symbol":"BTCUSDT","fundingTime":1787990400000,"fundingRate":"0.00010000",)"
    R"("markPrice":"77597.93110145","rateType":"Regular"})";

constexpr std::string_view kRealKlineRow =
    "BTCUSDT,1717200000000,67577.90,67729.90,67535.40,67690.00,3025.451,1717203599999,"
    "204628990.86440,49741,1569.014,106127949.20210,0";

constexpr std::string_view kUnknownSymbolFunding =
    R"({"symbol":"NOTASYMBOL","fundingTime":1787990400000,"fundingRate":"0.00010000",)"
    R"("markPrice":"77597.93110145","rateType":"Regular"})";

constexpr std::string_view kUnknownSymbolKline =
    "NOTASYMBOL,1717200000000,67577.90,67729.90,67535.40,67690.00,3025.451,1717203599999,"
    "204628990.86440,49741,1569.014,106127949.20210,0";

// Isolation: the JSON path's success case — one simdjson::ondemand::parser
// construction + a padded_string copy (bin_hist_parser_policy.hpp's own
// comments flag both as candidates for removal) per call, plus the
// object's four field lookups. Baseline for that rewrite, not a number to
// leave standing.
void BM_BinHistParser_ParsesFunding(benchmark::State& state) {
    auto raw = as_bytes(kRealFundingEntry);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_ParsesFunding);

// Isolation: the CSV path's success case — no simdjson, no allocation,
// just field-of-view slicing + from_chars. The comparison point for how
// much the JSON path's overhead above is actually costing relative to a
// plain-text format doing the same job.
void BM_BinHistParser_ParsesKline(benchmark::State& state) {
    auto raw = as_bytes(kRealKlineRow);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_ParsesKline);

// Isolation: JSON path, symbol miss — still pays the full simdjson parse
// (symbol is read before the table lookup can reject it), unlike the CSV
// path below where the reject can only happen after the same parse cost
// too (field[0] is read first). Same shape as
// BinHistSymbolTable_IdOfUnknown, one level up the stack.
void BM_BinHistParser_RejectsUnknownSymbolFunding(benchmark::State& state) {
    auto raw = as_bytes(kUnknownSymbolFunding);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_RejectsUnknownSymbolFunding);

// Isolation: CSV path, symbol miss — pays the full 8-field comma split
// (parse_kline collects every field before checking the symbol), then the
// table lookup's own worst case.
void BM_BinHistParser_RejectsUnknownSymbolKline(benchmark::State& state) {
    auto raw = as_bytes(kUnknownSymbolKline);
    for (auto _ : state) {
        benchmark::DoNotOptimize(raw);
        auto ev = BinHistVenue::parse(raw);
        benchmark::DoNotOptimize(ev);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinHistParser_RejectsUnknownSymbolKline);

}  // namespace
