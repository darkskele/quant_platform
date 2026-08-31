#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

namespace qp {

// Lock-free single-producer single-consumer ring buffer. One thread calls
// push(), a different single thread calls pop().
template <typename T, std::size_t Capacity, bool UseHeap = false>
class SpscQueue {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

    // std::byte[sizeof(T)] alone only guarantees byte alignment when
    // heap-allocated via new[], not alignof(T).
    struct alignas(T) storage_t {
        std::byte data[sizeof(T)];
    };

    using Storage =
        std::conditional_t<UseHeap, std::unique_ptr<storage_t[]>, std::array<storage_t, Capacity>>;
    static constexpr std::size_t INDEX_MASK = Capacity - 1;

   public:
    SpscQueue() {
        if constexpr (UseHeap) storage_ = std::make_unique<storage_t[]>(Capacity);
    }

    ~SpscQueue() {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto head = head_.load(std::memory_order_acquire);
        while (tail != head) {
            std::destroy_at(std::launder(reinterpret_cast<T*>(&storage_[tail])));
            tail = (tail + 1) & INDEX_MASK;
        }
    }

    SpscQueue(const SpscQueue&)            = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    // Producer side. False if the queue is full.
    template <typename... Args>
    bool push(Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);

        // head_ is only ever written by this thread. acquire on
        // tail_, must observe the consumer's release-store of tail_ before
        // reading storage_[head] is safe to overwrit.
        auto head      = head_.load(std::memory_order_relaxed);
        auto next_head = (head + 1) & INDEX_MASK;
        if (next_head == tail_.load(std::memory_order_acquire)) return false;  // full

        std::construct_at(reinterpret_cast<T*>(&storage_[head]), std::forward<Args>(args)...);
        // release: publishes both the new head_ and the just-constructed
        // element to the consumer's acquire-load of head_ below.
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Consumer side. Empty optional if nothing available.
    std::optional<T> pop() {
        // relaxed: tail_ is only ever written by this thread. acquire on
        // head_: must observe the producer's release-store.
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return std::nullopt;  // empty

        T*               elem = std::launder(reinterpret_cast<T*>(&storage_[tail]));
        std::optional<T> out{std::move(*elem)};
        std::destroy_at(elem);
        // release: publishes the freed slot to the producer's acquire-load
        // of tail_ above.
        tail_.store((tail + 1) & INDEX_MASK, std::memory_order_release);
        return out;
    }

    std::size_t size() const noexcept {
        auto head = head_.load(std::memory_order_acquire);
        auto tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & INDEX_MASK;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    Storage storage_;
    // alignas(64): own cache line per cursor, so the producer writing
    // head_ never invalidates the consumer's cached tail_ line.
    alignas(64) std::atomic<std::size_t> head_{0};  // producer-owned
    alignas(64) std::atomic<std::size_t> tail_{0};  // consumer-owned
};

}  // namespace qp
