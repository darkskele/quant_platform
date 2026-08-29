#include <benchmark/benchmark.h>

#include <string>
#include <vector>

#include "venue_types.hpp"

using qp::data_source::source::SymbolTable;

namespace {

// Isolation: intern() on a name it has never seen before — the one-time,
// allocation-paying insert path. intern() linearly scans every already-
// interned name before deciding a name is new, so letting the table grow
// unbounded across iterations would make later iterations within the same
// run measure an increasingly large (and increasingly unrepresentative)
// scan — this project's own symbol sets are a handful to a few dozen
// names (docs/strategy.md), not millions. Cycling through a small pool of
// 64 distinct names and resetting (untimed) every full cycle keeps the
// scan cost bounded at a realistic scale instead of growing without limit.
void BM_SymbolTable_InternNew(benchmark::State& state) {
    constexpr int            kPoolSize = 64;
    std::vector<std::string> names;
    names.reserve(kPoolSize);
    for (int i = 0; i < kPoolSize; ++i) names.push_back("SYM" + std::to_string(i));

    SymbolTable table;
    int         i = 0;
    for (auto _ : state) {
        if (i % kPoolSize == 0 && i != 0) {
            state.PauseTiming();
            table = SymbolTable{};
            state.ResumeTiming();
        }
        auto id = table.intern(names[static_cast<std::size_t>(i % kPoolSize)]);
        benchmark::DoNotOptimize(id);
        ++i;
    }
}

BENCHMARK(BM_SymbolTable_InternNew);

// Isolation: intern() on an already-known name — the steady-state,
// allocation-free path the header documents as the common case once a
// source has finished pre-interning its subscribed set ("This runs once
// per message on the I/O thread").
void BM_SymbolTable_InternExisting(benchmark::State& state) {
    SymbolTable table;
    table.intern("BTCUSDT");
    for (auto _ : state) {
        auto id = table.intern("BTCUSDT");
        benchmark::DoNotOptimize(id);
    }
}

BENCHMARK(BM_SymbolTable_InternExisting);

// Isolation: name() lookup by id.
void BM_SymbolTable_Name(benchmark::State& state) {
    SymbolTable table;
    auto        id = table.intern("BTCUSDT");
    for (auto _ : state) benchmark::DoNotOptimize(table.name(id).data());
}

BENCHMARK(BM_SymbolTable_Name);

}  // namespace
