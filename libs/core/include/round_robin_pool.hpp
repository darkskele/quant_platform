#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <thread>
#include <tuple>
#include <utility>

#include "spsc_queue.hpp"

namespace qp {

/// Runs a fixed, compile-time set of Tasks — each `Result operator()(const
/// Context&)` — round-robin across NumWorkers persistent threads
/// (NumWorkers <= sizeof...(Tasks); a worker runs several tasks per round
/// if there are more tasks than workers). Domain-agnostic: knows nothing
/// about MarketEvent/Strategy — Engine supplies Context/Result/Tasks.
///
/// run_round() collects results via one SpscQueue<Result> per task,
/// drained in strict task-index order — draining is simultaneously the
/// wait for that task's worker and the deterministic ordering, no separate
/// barrier (see docs/decisions.md D33).
template <std::size_t NumWorkers, class Context, class Result, class... Tasks>
class RoundRobinPool {
    static constexpr std::size_t kNumTasks = sizeof...(Tasks);
    static_assert(NumWorkers >= 1 && NumWorkers <= kNumTasks);

    using ResultQueue = SpscQueue<Result, 2>;  // 2: only ever one result in flight per task

   public:
    explicit RoundRobinPool(Tasks... tasks) : tasks_{std::move(tasks)...} {
        spawn_workers(std::make_index_sequence<NumWorkers>{});
    }

    // Worker threads capture `this`; a moved-to pool would leave them
    // pointing at the old address.
    RoundRobinPool(const RoundRobinPool&)            = delete;
    RoundRobinPool& operator=(const RoundRobinPool&) = delete;
    RoundRobinPool(RoundRobinPool&&)                 = delete;
    RoundRobinPool& operator=(RoundRobinPool&&)      = delete;

    ~RoundRobinPool() {
        stop_.store(true, std::memory_order_relaxed);
        generation_.fetch_add(1, std::memory_order_release);  // release every spinning worker
        for (auto& worker : workers_)
            if (worker.joinable()) worker.join();
    }

    template <class OnResult>
    void run_round(const Context& context, OnResult&& on_result) {
        current_context_ = &context;  // shared read-only input every task() gets this round
        generation_.fetch_add(1, std::memory_order_release);  // wake all workers

        for (std::size_t i = 0; i < kNumTasks; ++i) {
            std::optional<Result> result;
            while (!(result = result_queues_[i].pop())) {
            }
            on_result(i, std::move(*result));
        }
    }

   private:
    // Round-robin ownership, checked at compile time: false branch isn't
    // compiled into worker W's code at all, not just skipped at runtime.
    template <std::size_t W, std::size_t I>
    void run_if_mine() {
        if constexpr (I % NumWorkers == W)
            result_queues_[I].push(std::get<I>(tasks_)(*current_context_));
    }

    // Worker W asks every task index "mine?" (Is = 0..kNumTasks-1) —
    // expands to one inline call per task, all in this one function. If W
    // owns >1 task they run here sequentially, same call, before return.
    template <std::size_t W, std::size_t... Is>
    void run_assigned(std::index_sequence<Is...>) {
        (run_if_mine<W, Is>(), ...);
    }

    template <std::size_t W>
    void worker_loop() {
        std::size_t seen = 0;
        for (;;) {
            std::size_t gen;
            while ((gen = generation_.load(std::memory_order_acquire)) == seen) {
            }
            seen = gen;
            if (stop_.load(std::memory_order_relaxed)) return;
            run_assigned<W>(std::index_sequence_for<Tasks...>{});
        }
    }

    // Ws = 0..NumWorkers-1, compile-time (worker_loop<W> needs W as a
    // template arg) — expands to NumWorkers separate thread-launch lines.
    template <std::size_t... Ws>
    void spawn_workers(std::index_sequence<Ws...>) {
        ((workers_[Ws] = std::thread([this] { worker_loop<Ws>(); })), ...);
    }

    std::tuple<Tasks...>                tasks_;
    const Context*                      current_context_ = nullptr;
    std::array<ResultQueue, kNumTasks>  result_queues_;
    std::array<std::thread, NumWorkers> workers_;
    std::atomic<std::size_t>            generation_{0};
    std::atomic<bool>                   stop_{false};
};

}  // namespace qp
