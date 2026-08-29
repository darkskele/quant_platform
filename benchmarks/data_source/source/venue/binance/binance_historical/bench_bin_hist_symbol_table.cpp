#include <benchmark/benchmark.h>

#include <string_view>

#include "bin_hist_symbol_table.hpp"

using qp::data_source::source::venue::binance::binance_historical::BinHistSymbolTable;

namespace {

// DoNotOptimize on `name` itself, not just the result: id_of("BTCUSDT")
// called with a literal is provably foldable at compile time (the whole
// scan, not just the answer), and the optimizer will happily do that
// despite DoNotOptimize(id) below — same class of trap as this session's
// earlier Engine::step()/LastTradeMatcher benchmarks (BENCHMARKS.md).
// Marking the input opaque forces the actual runtime scan to happen.

// Isolation: id_of() on the first entry — the linear scan's best case,
// one comparison.
void BM_BinHistSymbolTable_IdOfFirstMatch(benchmark::State& state) {
    std::string_view name = "BTCUSDT";
    for (auto _ : state) {
        benchmark::DoNotOptimize(name);
        auto id = BinHistSymbolTable::id_of(name);
        benchmark::DoNotOptimize(id);
    }
}

BENCHMARK(BM_BinHistSymbolTable_IdOfFirstMatch);

// Isolation: id_of() on the last entry — the linear scan's worst case
// among known symbols, kSymbols.size() comparisons.
void BM_BinHistSymbolTable_IdOfLastMatch(benchmark::State& state) {
    std::string_view name = "LTCUSDT";
    for (auto _ : state) {
        benchmark::DoNotOptimize(name);
        auto id = BinHistSymbolTable::id_of(name);
        benchmark::DoNotOptimize(id);
    }
}

BENCHMARK(BM_BinHistSymbolTable_IdOfLastMatch);

// Isolation: id_of() on a symbol not in the table — has to scan every
// entry before returning nullopt. The actual worst case: an unexpected
// symbol on the wire (typo, a listing not in kSymbols) pays this every
// time, not just once, since nothing gets cached the way the old runtime
// table's intern() would have. 7 chars, deliberately: std::ranges::equal-
// style short-circuiting on a length that collides with nothing would
// give a misleadingly cheap "not found" (a 10-char miss never touches a
// character); 7 chars collides with 6 of the 10 real entries' length,
// forcing genuine string comparisons against those before failing.
void BM_BinHistSymbolTable_IdOfUnknown(benchmark::State& state) {
    std::string_view name = "ZZZUSDT";
    for (auto _ : state) {
        benchmark::DoNotOptimize(name);
        auto id = BinHistSymbolTable::id_of(name);
        benchmark::DoNotOptimize(id);
    }
}

BENCHMARK(BM_BinHistSymbolTable_IdOfUnknown);

// Isolation: name_of() — direct array index, no scan.
void BM_BinHistSymbolTable_NameOf(benchmark::State& state) {
    qp::SymbolId id = 9;
    for (auto _ : state) {
        benchmark::DoNotOptimize(id);
        benchmark::DoNotOptimize(BinHistSymbolTable::name_of(id).data());
    }
}

BENCHMARK(BM_BinHistSymbolTable_NameOf);

}  // namespace
