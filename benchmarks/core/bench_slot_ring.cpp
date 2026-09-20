#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "slot_ring.hpp"

namespace {

// Isolation: place alone, freeing the slot untimed so it never runs against an
// occupied one.
void BM_SlotRing_PlaceInt(benchmark::State& state) {
    qp::SlotRing<int, 1024> ring;
    std::size_t             position = 0;
    for (auto _ : state) {
        if (!ring.place(position, 1)) {
            state.PauseTiming();
            while (ring.take(position - 1024)) {
            }
            state.ResumeTiming();
            ring.place(position, 1);
        }
        benchmark::ClobberMemory();
        ++position;
    }
}

BENCHMARK(BM_SlotRing_PlaceInt);

// Tandem: the whole cycle one position goes through, which is the per file cost
// the fetch path pays.
void BM_SlotRing_PlaceTakeInt(benchmark::State& state) {
    qp::SlotRing<int, 1024> ring;
    std::size_t             position = 0;
    for (auto _ : state) {
        bool placed = ring.place(position, 1);
        benchmark::DoNotOptimize(placed);
        auto value = ring.take(position);
        benchmark::DoNotOptimize(value);
        ++position;
    }
}

BENCHMARK(BM_SlotRing_PlaceTakeInt);

// A full window placed before any is read, which is the steady state while a
// stream has several fetches outstanding.
template <std::size_t Window>
void BM_SlotRing_WindowRoundTrip(benchmark::State& state) {
    qp::SlotRing<int, 1024> ring;
    std::size_t             position = 0;
    for (auto _ : state) {
        for (std::size_t i = 0; i < Window; ++i) ring.place(position + i, static_cast<int>(i));
        for (std::size_t i = 0; i < Window; ++i) {
            auto value = ring.take(position + i);
            benchmark::DoNotOptimize(value);
        }
        position += Window;
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(Window));
}

BENCHMARK(BM_SlotRing_WindowRoundTrip<8>);
BENCHMARK(BM_SlotRing_WindowRoundTrip<32>);

// Out of order placement, worst case for the sequence check, since every slot
// is written on a different line from the one read next.
void BM_SlotRing_ReversedWindow(benchmark::State& state) {
    constexpr std::size_t   kWindow = 8;
    qp::SlotRing<int, 1024> ring;
    std::size_t             position = 0;
    for (auto _ : state) {
        for (std::size_t i = kWindow; i-- > 0;) ring.place(position + i, static_cast<int>(i));
        for (std::size_t i = 0; i < kWindow; ++i) {
            auto value = ring.take(position + i);
            benchmark::DoNotOptimize(value);
        }
        position += kWindow;
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kWindow));
}

BENCHMARK(BM_SlotRing_ReversedWindow);

}  // namespace
