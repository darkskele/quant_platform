#include <benchmark/benchmark.h>

#include <cstdint>

#include "round_robin_pool.hpp"

namespace {

struct NoopTask {
    int operator()(const int&) { return 0; }
};

// Round-trip floor: generation-counter wake + one SpscQueue push/pop.
void BM_RoundRobinPool_OneTaskOneWorker(benchmark::State& state) {
    qp::RoundRobinPool<1, int, int, NoopTask> pool{NoopTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_RoundRobinPool_OneTaskOneWorker);

// 4 tasks, 1 worker: one wake, tasks run sequentially inside it.
void BM_RoundRobinPool_FourTasksOneWorker(benchmark::State& state) {
    qp::RoundRobinPool<1, int, int, NoopTask, NoopTask, NoopTask, NoopTask> pool{
        NoopTask{}, NoopTask{}, NoopTask{}, NoopTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_RoundRobinPool_FourTasksOneWorker);

// 4 tasks, 4 workers: full parallel round trip, 4 concurrent wakes.
void BM_RoundRobinPool_FourTasksFourWorkers(benchmark::State& state) {
    qp::RoundRobinPool<4, int, int, NoopTask, NoopTask, NoopTask, NoopTask> pool{
        NoopTask{}, NoopTask{}, NoopTask{}, NoopTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_RoundRobinPool_FourTasksFourWorkers);

// Baseline: same 4 calls, no pool — the number the above are paying
// synchronization overhead against.
void BM_RoundRobinPool_FourTasksNoPool(benchmark::State& state) {
    NoopTask a, b, c, d;
    for (auto _ : state) {
        benchmark::DoNotOptimize(a(0));
        benchmark::DoNotOptimize(b(0));
        benchmark::DoNotOptimize(c(0));
        benchmark::DoNotOptimize(d(0));
    }
}

BENCHMARK(BM_RoundRobinPool_FourTasksNoPool);

// NoopTask isolates the pool's own dispatch/sync cost cleanly (task cost
// ~0, so the pool-vs-no-pool difference *is* the overhead) — but it can't
// show the other side of the question: how much that overhead matters
// once a task does actual work, or whether 4-way parallelism ever nets a
// real win once per-task cost is large enough to amortize the
// synchronization cost against. WorkTask below answers that: a small,
// fixed, deterministic amount of real computation (not a Strategy — this
// file's job is the pool mechanism, not FundingCarryStrategy's semantics;
// bench_engine.cpp already covers a real Strategy running through the
// pool inside the rest of Engine::step()).
struct WorkTask {
    int operator()(const int& x) const {
        std::uint32_t acc = static_cast<std::uint32_t>(x);
        for (int i = 0; i < 64; ++i) acc = acc * 2654435761u + static_cast<std::uint32_t>(i);
        return static_cast<int>(acc);
    }
};

// Solo baseline: WorkTask's own cost, no pool, no other tasks — what the
// "OneWorker"/"FourWorkers" numbers below are actually paying pool
// overhead on top of.
void BM_RoundRobinPool_WorkTaskSolo(benchmark::State& state) {
    WorkTask task;
    for (auto _ : state) benchmark::DoNotOptimize(task(0));
}

BENCHMARK(BM_RoundRobinPool_WorkTaskSolo);

// One real task, one worker, through the pool — same shape as
// BM_RoundRobinPool_OneTaskOneWorker, WorkTask instead of NoopTask.
void BM_RoundRobinPool_WorkTaskOneWorker(benchmark::State& state) {
    qp::RoundRobinPool<1, int, int, WorkTask> pool{WorkTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_RoundRobinPool_WorkTaskOneWorker);

// Four real tasks, one worker: sequential, same wake.
void BM_RoundRobinPool_FourWorkTasksOneWorker(benchmark::State& state) {
    qp::RoundRobinPool<1, int, int, WorkTask, WorkTask, WorkTask, WorkTask> pool{
        WorkTask{}, WorkTask{}, WorkTask{}, WorkTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_RoundRobinPool_FourWorkTasksOneWorker);

// Four real tasks, four workers: genuine parallel execution — the case
// where, unlike NoopTask, there's enough real work per task that 4-way
// concurrency has something to actually amortize its synchronization cost
// against.
void BM_RoundRobinPool_FourWorkTasksFourWorkers(benchmark::State& state) {
    qp::RoundRobinPool<4, int, int, WorkTask, WorkTask, WorkTask, WorkTask> pool{
        WorkTask{}, WorkTask{}, WorkTask{}, WorkTask{}};
    for (auto _ : state) pool.run_round(0, [](std::size_t, int) {});
}

BENCHMARK(BM_RoundRobinPool_FourWorkTasksFourWorkers);

// Baseline: same 4 WorkTask calls, no pool — the number the two "through
// the pool" WorkTask benchmarks above are paying synchronization overhead
// against.
void BM_RoundRobinPool_FourWorkTasksNoPool(benchmark::State& state) {
    WorkTask a, b, c, d;
    for (auto _ : state) {
        benchmark::DoNotOptimize(a(0));
        benchmark::DoNotOptimize(b(0));
        benchmark::DoNotOptimize(c(0));
        benchmark::DoNotOptimize(d(0));
    }
}

BENCHMARK(BM_RoundRobinPool_FourWorkTasksNoPool);

}  // namespace
