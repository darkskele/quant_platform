#pragma once
#include <atomic>
#include <bit>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

namespace qp {

/// Lock-free multi-producer, single-consumer bounded queue. Any number of
/// producer threads may push() concurrently, racing a CAS on enqueue_pos_
/// to claim a position; only one thread may call try_pop(). Fills the
/// missing "many writers, one reader" cell of core's producer/consumer
/// primitive matrix (SpscQueue: 1-writer/1-reader, SpmcRing:
/// 1-writer/N-readers).
///
/// Per-slot readiness is a plain bool, not Vyukov's usual per-cell
/// sequence number: fullness is decided directly from enqueue_pos_ -
/// dequeue_pos_ (same shape as SpscQueue's head/tail check) *before* a
/// producer ever attempts its CAS, which bounds how far enqueue_pos_ can
/// run ahead of dequeue_pos_ to under Capacity — so at most one lap's
/// claim can ever be outstanding per slot at a time, and a bool can never
/// be ambiguous about which lap it belongs to. That's specific to having
/// exactly one consumer enforcing strict order; it wouldn't hold for a
/// multi-consumer version of this queue.
///
/// Per-slot readiness flags aren't cache-line-padded: unlike SpmcRing's
/// NumConsumers cursors (typically a handful, so padding each one costs
/// little), this queue's Capacity is sized for a low-frequency control
/// channel, not a hot data feed — false-sharing between adjacent slots
/// under rare, low-concurrency pushes isn't worth the memory blow-up
/// padding every slot to a cache line would cost.
template <typename T, std::size_t Capacity>
class MpscQueue {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert(std::has_single_bit(Capacity), "Capacity must be a power of 2");

    using storage_t                         = std::byte[sizeof(T)];
    static constexpr std::size_t INDEX_MASK = Capacity - 1;

    // Free retries before yielding on CAS failure — most contended CAS
    // races resolve within one or two retries, so this only ever engages
    // under genuine sustained contention. Untuned (no measurement behind
    // this specific number, same "placeholder for the right shape, not a
    // validated value" caveat SpmcRing's own backoff constants carry) — a
    // few retries is a reasonable default, not a claim that 8 is optimal.
    static constexpr int kSpinAttempts = 8;

   public:
    MpscQueue() {
        for (std::size_t i = 0; i < Capacity; ++i)
            ready_[i].store(false, std::memory_order_relaxed);
    }

    ~MpscQueue() {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (ready_[i].load(std::memory_order_relaxed)) {
                std::destroy_at(std::launder(reinterpret_cast<T*>(&storage_[i])));
            }
        }
    }

    MpscQueue(const MpscQueue&)            = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    // Producer side, any thread. False if the queue is full (caller decides: drop, retry, count).
    template <typename... Args>
    bool push(Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);

        std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        std::size_t slot;
        int         retries = 0;
        for (;;) {
            // acquire: must observe try_pop()'s release-store of dequeue_pos_
            // before trusting this slot is actually free.
            if (pos - dequeue_pos_.load(std::memory_order_acquire) >= Capacity)
                return false;  // full

            slot = pos & INDEX_MASK;
            if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
            // CAS failed: compare_exchange_weak refreshed `pos` to the current value — retry,
            // re-checking fullness against it (another producer may have raced ahead of us).
            // Bare retry (no backoff at all) meant every contending producer
            // hammered the same enqueue_pos_ cache line as fast as possible —
            // measured (bench_mpsc_queue.cpp's contended cases) as severely
            // supralinear: ~8x cost for 2.5x threads, ~23x for 4.5x. A few
            // free retries first (the common case resolves in 1-2), then
            // yield instead of spinning tighter — cheap insurance against
            // contention this queue's only real caller (ControlChannel)
            // doesn't hit today, but costs nothing when uncontended since it
            // only ever triggers after a CAS actually fails.
            if (++retries > kSpinAttempts) std::this_thread::yield();
        }

        std::construct_at(reinterpret_cast<T*>(&storage_[slot]), std::forward<Args>(args)...);
        // release: publishes the constructed element to try_pop()'s acquire-load of ready_.
        ready_[slot].store(true, std::memory_order_release);
        return true;
    }

    // Consumer side, single thread only. Empty optional if nothing available.
    std::optional<T> try_pop() {
        // relaxed: dequeue_pos_ is only ever written by this thread.
        std::size_t pos  = dequeue_pos_.load(std::memory_order_relaxed);
        std::size_t slot = pos & INDEX_MASK;
        // acquire: must observe push()'s release-store before reading storage_[slot].
        if (!ready_[slot].load(std::memory_order_acquire))
            return std::nullopt;  // not published yet

        T*               elem = std::launder(reinterpret_cast<T*>(&storage_[slot]));
        std::optional<T> out{std::move(*elem)};
        std::destroy_at(elem);
        // release: frees the slot for a producer's next-lap claim.
        ready_[slot].store(false, std::memory_order_release);
        // release: publishes the freed slot to push()'s acquire-load of dequeue_pos_.
        dequeue_pos_.store(pos + 1, std::memory_order_release);
        return out;
    }

    /// Approximate — a live snapshot under concurrent producers, not a
    /// guaranteed-exact count (same "diagnostic only" caveat as
    /// SpmcRing::full_count()).
    std::size_t size() const noexcept {
        return enqueue_pos_.load(std::memory_order_relaxed) -
               dequeue_pos_.load(std::memory_order_relaxed);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    alignas(alignof(T)) storage_t storage_[Capacity];
    std::atomic<bool> ready_[Capacity];
    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};  // multi-producer, CAS-raced
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};  // consumer-owned
};

}  // namespace qp
