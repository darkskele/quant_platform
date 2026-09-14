#include <benchmark/benchmark.h>

#include <utility>

#include "types.hpp"
#include "viewable_pool.hpp"

namespace {

constexpr std::size_t kCapacity = 1024;

template <bool UseHeap>
using Pool = qp::ViewablePool<qp::Intent, kCapacity, UseHeap>;

// Push alone: pool reset (untimed) whenever it'd otherwise overflow, so
// push() never runs past capacity.
template <bool UseHeap>
void BM_ViewablePool_Push(benchmark::State& state) {
    Pool<UseHeap> pool;
    qp::Intent    intent{.symbol = 1, .market = 0, .target_position = 2.0};
    for (auto _ : state) {
        if (pool.size() == pool.capacity()) {
            state.PauseTiming();
            pool.reset();
            state.ResumeTiming();
        }
        pool.push(intent);
        benchmark::DoNotOptimize(pool);
    }
}

BENCHMARK(BM_ViewablePool_Push<false>);
BENCHMARK(BM_ViewablePool_Push<true>);

// Mirror of the push case above, via emplace instead of a pre-built Intent.
template <bool UseHeap>
void BM_ViewablePool_Emplace(benchmark::State& state) {
    Pool<UseHeap> pool;
    for (auto _ : state) {
        if (pool.size() == pool.capacity()) {
            state.PauseTiming();
            pool.reset();
            state.ResumeTiming();
        }
        pool.emplace(qp::SymbolId{1}, qp::MarketId{0}, qp::Qty{2.0});
        benchmark::DoNotOptimize(pool);
    }
}

BENCHMARK(BM_ViewablePool_Emplace<false>);
BENCHMARK(BM_ViewablePool_Emplace<true>);

// Access via view(): pool filled once outside the timed loop, the span
// cycled over — isolates read cost through the span (stack: direct; heap:
// one pointer indirection already resolved when the span was taken).
template <bool UseHeap>
void BM_ViewablePool_ViewAccess(benchmark::State& state) {
    Pool<UseHeap> pool;
    for (std::size_t i = 0; i < kCapacity; ++i)
        pool.push(qp::Intent{.symbol = 1, .market = 0, .target_position = double(i)});

    auto        view = pool.view();
    std::size_t i    = 0;
    for (auto _ : state) {
        qp::Intent intent = view[i];
        benchmark::DoNotOptimize(intent);
        i = (i + 1) % kCapacity;
    }
}

BENCHMARK(BM_ViewablePool_ViewAccess<false>);
BENCHMARK(BM_ViewablePool_ViewAccess<true>);

// Reset alone: cost doesn't depend on how many elements are live (it's just
// count_ = 0), so no fill/drain needed around it.
template <bool UseHeap>
void BM_ViewablePool_Reset(benchmark::State& state) {
    Pool<UseHeap> pool;
    for (auto _ : state) {
        pool.reset();
        benchmark::DoNotOptimize(pool);
    }
}

BENCHMARK(BM_ViewablePool_Reset<false>);
BENCHMARK(BM_ViewablePool_Reset<true>);

// Move construct out, move assign back — a round trip, tandem-style like
// push+pop elsewhere in this file, since isolating just one direction would
// need a PauseTiming rebuild every iteration (measured elsewhere in this
// repo at ~270ns/call, likely swamping the heap-mode move it's isolating).
// The two storage modes are the actual point: UseHeap=true moves a
// unique_ptr (O(1), pointer repoint), UseHeap=false relocates the whole
// std::array<Intent, kCapacity> (O(Capacity), a real copy).
template <bool UseHeap>
void BM_ViewablePool_MoveRoundTrip(benchmark::State& state) {
    Pool<UseHeap> source;
    for (std::size_t i = 0; i < kCapacity; ++i)
        source.push(qp::Intent{.symbol = 1, .market = 0, .target_position = double(i)});

    for (auto _ : state) {
        Pool<UseHeap> dest{std::move(source)};
        benchmark::DoNotOptimize(dest);
        source = std::move(dest);
    }
}

BENCHMARK(BM_ViewablePool_MoveRoundTrip<false>);
BENCHMARK(BM_ViewablePool_MoveRoundTrip<true>);

}  // namespace
