#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

namespace qp {

/// Single-producer, multi-consumer queue over a fixed, compile-time set of
/// consumers.
template <typename T, std::size_t Capacity, std::size_t NumConsumers, bool UseHeap = false>
class SpmcQueue {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert(std::has_single_bit(Capacity), "Capacity must be a power of 2");
    static_assert(NumConsumers > 0, "at least one consumer");

    // std::byte[sizeof(T)] alone only guarantees byte alignment when
    // heap-allocated via new[], not alignof(T)
    struct alignas(T) storage_t {
        std::byte data[sizeof(T)];
    };

    using Storage =
        std::conditional_t<UseHeap, std::unique_ptr<storage_t[]>, std::array<storage_t, Capacity>>;
    static constexpr std::size_t INDEX_MASK = Capacity - 1;

    // Own cache line per cursor.
    struct alignas(64) Cursor {
        std::atomic<std::size_t> value{0};
    };

   public:
    SpmcQueue() {
        if constexpr (UseHeap) storage_ = std::make_unique<storage_t[]>(Capacity);
    }

    ~SpmcQueue() {
        // [published_ - Capacity, published_) all hold live objects,
        // push() only ever destroys a slot right before reconstructing it.
        auto head = published_.load(std::memory_order_relaxed);
        auto tail = (head >= Capacity) ? head - Capacity : std::size_t{0};
        while (tail != head) {
            std::destroy_at(std::launder(reinterpret_cast<T*>(&storage_[tail & INDEX_MASK])));
            ++tail;
        }
    }

    SpmcQueue(const SpmcQueue&)            = delete;
    SpmcQueue& operator=(const SpmcQueue&) = delete;

    /// Reads the next slot `consumer` hasn't seen yet. Never blocks.
    /// @param consumer index in [0, NumConsumers), fixed at wiring time.
    /// @return nullopt if `consumer` is caught up to the producer.
    std::optional<T> try_pop(std::size_t consumer) {
        auto& cursor = cursors_[consumer].value;
        auto  cur    = cursor.load(std::memory_order_relaxed);
        if (cur == published_.load(std::memory_order_acquire)) return std::nullopt;

        T* elem = std::launder(reinterpret_cast<T*>(&storage_[cur & INDEX_MASK]));
        // Copy, not move, other consumers may
        // still need this exact slot, so it must stay intact.
        std::optional<T> out{*elem};
        cursor.store(cur + 1, std::memory_order_release);
        return out;
    }

    /// Compile-time-indexed sibling of try_pop(std::size_t).
    template <std::size_t Consumer>
    std::optional<T> try_pop() {
        static_assert(Consumer < NumConsumers);
        return try_pop(Consumer);
    }

    /// Publishes one event constructed from `args`.
    template <typename... Args>
    bool push(Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);
        auto next = published_.load(std::memory_order_relaxed);
        if (!slot_free(next)) return false;

        auto* slot = reinterpret_cast<T*>(&storage_[next & INDEX_MASK]);
        if (next >= Capacity) [[likely]]  // false only for the queue's first Capacity pushes ever
            std::destroy_at(std::launder(slot));
        std::construct_at(slot, std::forward<Args>(args)...);
        published_.store(next + 1, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    std::size_t min_cursor() const {
        std::size_t min = cursors_[0].value.load(std::memory_order_acquire);
        for (std::size_t i = 1; i < NumConsumers; ++i) {
            auto v = cursors_[i].value.load(std::memory_order_acquire);
            if (v < min) min = v;
        }
        return min;
    }

    // True if the slot `next` wants to reuse has been read by every
    // consumer already.
    bool slot_free(std::size_t next) const {
        if (next < Capacity) return true;  // queue hasn't wrapped yet; slot is virgin storage
        return min_cursor() > next - Capacity;
    }

    Storage storage_;
    alignas(64) std::atomic<std::size_t> published_{0};  // producer-owned
    Cursor cursors_[NumConsumers];
};

}  // namespace qp
