#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "lent_slot_pool.hpp"

namespace {

using Pool = qp::LentSlotPool<2048, 4096>;

// One slot out and back. The same slot comes back every time.
void BM_LentSlotPool_AcquireLendDrop(benchmark::State& state) {
    Pool pool;
    for (auto _ : state) {
        auto l = pool.lend(*pool.acquire(), 0, 1472);
        benchmark::DoNotOptimize(l.bytes().data());
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}

BENCHMARK(BM_LentSlotPool_AcquireLendDrop);

// A batch of 32 lent at once, dropped newest first.
void BM_LentSlotPool_Batch32(benchmark::State& state) {
    Pool                         pool;
    std::array<qp::LentSlot, 32> held{};
    for (auto _ : state) {
        for (auto& l : held) l = pool.lend(*pool.acquire(), 0, 1472);
        for (auto it = held.rbegin(); it != held.rend(); ++it) *it = {};
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * held.size()));
}

BENCHMARK(BM_LentSlotPool_Batch32);

// Reads every byte of a datagram while range(0) slots stay held, so reads walk that many slots.
void BM_LentSlotPool_SumHeld(benchmark::State& state) {
    constexpr std::size_t                   kMsg = 1472;
    const auto                              held = static_cast<std::size_t>(state.range(0));
    Pool                                    pool;
    std::array<qp::LentSlot, Pool::count()> out{};
    for (std::size_t i = 0; i < held; ++i) out[i] = pool.lend(*pool.acquire(), 0, kMsg);

    std::size_t next = 0;
    for (auto _ : state) {
        std::uint32_t sum = 0;
        for (auto b : out[next].bytes()) sum += static_cast<std::uint8_t>(b);
        benchmark::DoNotOptimize(sum);
        // Handed back and lent again, so the same slots stay held.
        out[next] = {};
        out[next] = pool.lend(*pool.acquire(), 0, kMsg);
        next      = next + 1 == held ? 0 : next + 1;
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations() * kMsg));
}

BENCHMARK(BM_LentSlotPool_SumHeld)->Arg(1)->Arg(64)->Arg(4095);

}  // namespace
