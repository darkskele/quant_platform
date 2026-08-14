#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

namespace qp {

// Lock-free single-producer single-consumer ring buffer. One thread calls
// push(), a different single thread calls pop() — no other pattern is safe.
// Ported from a prior project's `buffers::circular_buffer` (validated there
// under a threaded producer/consumer test). This is the seam between a feed
// thread (WS I/O) and a consumer thread (parse/record) so a slow consumer
// can never block the socket read.
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

    using storage_t = std::byte[sizeof(T)];
    static constexpr std::size_t INDEX_MASK = Capacity - 1;

public:
    SpscQueue() = default;

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

    // Producer side. False if the queue is full (caller decides: drop, block, count).
    template <typename... Args>
    bool push(Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);

        auto head      = head_.load(std::memory_order_relaxed);
        auto next_head = (head + 1) & INDEX_MASK;
        if (next_head == tail_.load(std::memory_order_acquire)) return false;  // full

        std::construct_at(reinterpret_cast<T*>(&storage_[head]), std::forward<Args>(args)...);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Consumer side. Empty optional if nothing available.
    std::optional<T> pop() {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return std::nullopt;  // empty

        T* elem = std::launder(reinterpret_cast<T*>(&storage_[tail]));
        std::optional<T> out{std::move(*elem)};
        std::destroy_at(elem);
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
    alignas(alignof(T)) storage_t storage_[Capacity];
    alignas(64) std::atomic<std::size_t> head_{0};  // producer-owned
    alignas(64) std::atomic<std::size_t> tail_{0};  // consumer-owned
};

}  // namespace qp
