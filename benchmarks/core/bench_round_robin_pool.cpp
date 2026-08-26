#include <benchmark/benchmark.h>

#include "round_robin_pool.hpp"

namespace {

struct NoopTask {
    int operator()(const int&) { return 0; }
};

// Round-trip floor: generation-counter wake + one SpscQueue push/pop.
void BM_OneTaskOneWorker(benchmark::State& state) {
    qp::RoundRobinPool<1, int, int, NoopTask> pool{NoopTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_OneTaskOneWorker);

// 4 tasks, 1 worker: one wake, tasks run sequentially inside it.
void BM_FourTasksOneWorker(benchmark::State& state) {
    qp::RoundRobinPool<1, int, int, NoopTask, NoopTask, NoopTask, NoopTask> pool{
        NoopTask{}, NoopTask{}, NoopTask{}, NoopTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_FourTasksOneWorker);

// 4 tasks, 4 workers: full parallel round trip, 4 concurrent wakes.
void BM_FourTasksFourWorkers(benchmark::State& state) {
    qp::RoundRobinPool<4, int, int, NoopTask, NoopTask, NoopTask, NoopTask> pool{
        NoopTask{}, NoopTask{}, NoopTask{}, NoopTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_FourTasksFourWorkers);

// Baseline: same 4 calls, no pool — the number the above are paying
// synchronization overhead against.
void BM_FourTasksNoPool(benchmark::State& state) {
    NoopTask a, b, c, d;
    for (auto _ : state) {
        benchmark::DoNotOptimize(a(0));
        benchmark::DoNotOptimize(b(0));
        benchmark::DoNotOptimize(c(0));
        benchmark::DoNotOptimize(d(0));
    }
}

BENCHMARK(BM_FourTasksNoPool);

}  // namespace
