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
/// to claim a position.
template <typename T, std::size_t Capacity>
class MpscQueue {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert(std::has_single_bit(Capacity), "Capacity must be a power of 2");

    using storage_t                         = std::byte[sizeof(T)];
    static constexpr std::size_t INDEX_MASK = Capacity - 1;

    // Own cache line per slot.
    struct alignas(64) Ready {
        std::atomic<bool> flag{false};
    };

    // Free retries before yielding on CAS failure.
    // @todo: Tune this for hardware.
    static constexpr int kSpinAttempts = 100;

   public:
    MpscQueue() = default;

    ~MpscQueue() {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (ready_[i].flag.load(std::memory_order_relaxed)) {
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
            // CAS failed: pos was refreshed to the current value, retry.
            // Bare retry alone measured severely supralinear under
            // contention (~8x cost for 2.5x threads), hence the backoff.
            if (++retries > kSpinAttempts) std::this_thread::yield();
        }

        std::construct_at(reinterpret_cast<T*>(&storage_[slot]), std::forward<Args>(args)...);
        // release: publishes the constructed element to try_pop()'s acquire-load of ready_.
        ready_[slot].flag.store(true, std::memory_order_release);
        return true;
    }

    // Consumer side, single thread only. Empty optional if nothing available.
    std::optional<T> try_pop() {
        // relaxed: dequeue_pos_ is only ever written by this thread.
        std::size_t pos  = dequeue_pos_.load(std::memory_order_relaxed);
        std::size_t slot = pos & INDEX_MASK;
        // acquire: must observe push()'s release-store before reading storage_[slot].
        if (!ready_[slot].flag.load(std::memory_order_acquire))
            return std::nullopt;  // not published yet

        T*               elem = std::launder(reinterpret_cast<T*>(&storage_[slot]));
        std::optional<T> out{std::move(*elem)};
        std::destroy_at(elem);
        // release: frees the slot for a producer's next-lap claim.
        ready_[slot].flag.store(false, std::memory_order_release);
        // release: publishes the freed slot to push()'s acquire-load of dequeue_pos_.
        dequeue_pos_.store(pos + 1, std::memory_order_release);
        return out;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    alignas(alignof(T)) storage_t storage_[Capacity];
    Ready ready_[Capacity];
    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};  // multi-producer, CAS-raced
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};  // consumer-owned
};

}  // namespace qp
