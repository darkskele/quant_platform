#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

namespace qp {

/// Single-producer, multi-consumer bounded queue. Each element goes to exactly
/// one consumer, whichever claims it first, in push order.
/// @tparam Capacity slot count, a power of two, all of them usable.
/// @tparam UseHeap storage off the stack, for a capacity too large to hold there.
template <typename T, std::size_t Capacity, bool UseHeap = false>
class WorkQueue {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert(std::has_single_bit(Capacity), "Capacity must be a power of 2");

    static constexpr std::size_t INDEX_MASK = Capacity - 1;

    // Free retries before yielding on CAS failure.
    // @todo: Tune this for hardware.
    static constexpr int kSpinAttempts = 100;

    // seq sits with the slot it guards, so a consumer takes one cache line to
    // claim an element and read it.
    struct Cell {
        std::atomic<std::size_t> seq;
        alignas(T) std::byte storage[sizeof(T)];
    };

    using Cells = std::conditional_t<UseHeap, std::unique_ptr<Cell[]>, std::array<Cell, Capacity>>;

   public:
    WorkQueue() {
        if constexpr (UseHeap) cells_ = std::make_unique<Cell[]>(Capacity);
        // Slot i awaits the producer at position i.
        for (std::size_t i = 0; i < Capacity; ++i)
            cells_[i].seq.store(i, std::memory_order_relaxed);
    }

    ~WorkQueue() {
        // Everything claimed has been drained by now, so the live elements are
        // exactly the positions between the two cursors.
        auto pos = dequeue_pos_.load(std::memory_order_relaxed);
        auto end = enqueue_pos_.load(std::memory_order_relaxed);
        for (; pos != end; ++pos)
            std::destroy_at(std::launder(reinterpret_cast<T*>(&cells_[pos & INDEX_MASK].storage)));
    }

    WorkQueue(const WorkQueue&)            = delete;
    WorkQueue& operator=(const WorkQueue&) = delete;

    /// False if the queue is full.
    template <typename... Args>
    bool push(Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);

        const std::size_t pos  = enqueue_pos_.load(std::memory_order_relaxed);
        Cell&             cell = cells_[pos & INDEX_MASK];

        // A slot is free only on the lap it is stamped for. acquire pairs with
        // try_pop's release of pos + Capacity, so a claimed but undrained slot
        // still reads as full.
        if (cell.seq.load(std::memory_order_acquire) != pos) return false;

        std::construct_at(reinterpret_cast<T*>(&cell.storage), std::forward<Args>(args)...);
        // release publishes the element to the consumer that claims pos.
        cell.seq.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    /// Consumer side, any thread. Empty optional if nothing available.
    std::optional<T> try_pop() {
        std::size_t pos     = dequeue_pos_.load(std::memory_order_relaxed);
        Cell*       cell    = nullptr;
        int         retries = 0;
        for (;;) {
            cell = &cells_[pos & INDEX_MASK];
            // Signed, so the wraparound of both counters stays well defined.
            const auto seq = cell->seq.load(std::memory_order_acquire);
            const auto diff =
                static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos + 1);

            if (diff == 0) {
                // The claim happens after the slot is known ready, so no
                // consumer can take a position it then cannot service.
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return std::nullopt;  // empty, or the producer is mid-construct
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);  // lost the race, retry
            }
            if (++retries > kSpinAttempts) std::this_thread::yield();
        }

        T*               elem = std::launder(reinterpret_cast<T*>(&cell->storage));
        std::optional<T> out{std::move(*elem)};
        std::destroy_at(elem);
        // release frees the slot for the producer one lap on.
        cell->seq.store(pos + Capacity, std::memory_order_release);
        return out;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    Cells cells_;
    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};  // producer-owned
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};  // consumer, CAS-raced
};

}  // namespace qp
