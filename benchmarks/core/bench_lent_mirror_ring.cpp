#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <span>

#include "lent_mirror_ring.hpp"

namespace {

using Ring = qp::LentMirrorRing<64 * 1024, 64>;

// One message in and out. Sizes that do not divide the ring wrap at a moving point.
void BM_LentMirrorRing_WriteLendRelease(benchmark::State& state) {
    const auto n = static_cast<std::size_t>(state.range(0));
    Ring       ring;
    for (auto _ : state) {
        ring.write([&](std::span<std::byte> space) {
            benchmark::DoNotOptimize(space.data());
            return n;
        });
        auto l = ring.lend(n);
        benchmark::DoNotOptimize(l.bytes().data());
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

BENCHMARK(BM_LentMirrorRing_WriteLendRelease)->Arg(64)->Arg(1500)->Arg(16384);

// Eight ranges held at once, handed back newest first.
void BM_LentMirrorRing_ReverseRelease8(benchmark::State& state) {
    constexpr std::size_t        kChunk = 512;
    Ring                         ring;
    std::array<qp::LentBytes, 8> held{};
    for (auto _ : state) {
        ring.write([&](std::span<std::byte>) { return kChunk * held.size(); });
        for (auto& l : held) l = ring.lend(kChunk);
        for (auto it = held.rbegin(); it != held.rend(); ++it) *it = {};
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * held.size()));
}

BENCHMARK(BM_LentMirrorRing_ReverseRelease8);

// Reads every lent byte, so a wrapped span's second half is touched through the mirror.
void BM_LentMirrorRing_SumLentBytes(benchmark::State& state) {
    constexpr std::size_t kMsg = 1500;
    Ring                  ring;
    for (auto _ : state) {
        ring.write([](std::span<std::byte>) { return kMsg; });
        auto          l   = ring.lend(kMsg);
        std::uint32_t sum = 0;
        for (auto b : l.bytes()) sum += static_cast<std::uint8_t>(b);
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations() * kMsg));
}

BENCHMARK(BM_LentMirrorRing_SumLentBytes);

}  // namespace
