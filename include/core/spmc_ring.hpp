#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

namespace qp {

/// Single-producer, multi-consumer ring buffer over a fixed, compile-time
/// set of consumers (indices 0..NumConsumers-1, assigned once by whoever
/// wires it up). 
template <typename T, std::size_t Capacity, std::size_t NumConsumers, bool UseHeap = false>
class SpmcRing {
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

    // Own cache line per cursor: the producer's gating scan and
    // NumConsumers independent consumer threads all touch different
    // cursors concurrently — unpadded, they'd false-share.
    struct alignas(64) Cursor {
        std::atomic<std::size_t> value{0};
    };

   public:
    SpmcRing() {
        if constexpr (UseHeap) storage_ = std::make_unique<storage_t[]>(Capacity);
    }

    ~SpmcRing() {
        // [published_ - Capacity, published_) all hold live objects,
        // push() only ever destroys a slot right before reconstructing it.
        auto head = published_.load(std::memory_order_relaxed);
        auto tail = (head >= Capacity) ? head - Capacity : std::size_t{0};
        while (tail != head) {
            std::destroy_at(std::launder(reinterpret_cast<T*>(&storage_[tail & INDEX_MASK])));
            ++tail;
        }
    }

    SpmcRing(const SpmcRing&)            = delete;
    SpmcRing& operator=(const SpmcRing&) = delete;

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

    /// Publishes one event constructed from `args`. Single producer only.
    /// Blocks until every consumer has read past the slot being reused.
    template <typename... Args>
    void push(Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);

        auto next = published_.load(std::memory_order_relaxed);
        wait_for_slot(next);

        auto* slot = reinterpret_cast<T*>(&storage_[next & INDEX_MASK]);
        if (next >= Capacity) [[likely]]  // false only for the ring's first Capacity pushes ever
            std::destroy_at(std::launder(slot));
        std::construct_at(slot, std::forward<Args>(args)...);
        published_.store(next + 1, std::memory_order_release);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

    /// Number of push() calls that had to wait because the slot being
    /// reused wasn't free yet (some consumer hadn't read it).
    std::uint64_t full_count() const noexcept {
        return full_count_.load(std::memory_order_relaxed);
    }

   private:
    std::size_t min_cursor() const {
        std::size_t min = cursors_[0].value.load(std::memory_order_acquire);
        for (std::size_t i = 1; i < NumConsumers; ++i) {
            auto v = cursors_[i].value.load(std::memory_order_acquire);
            if (v < min) min = v;
        }
        return min;
    }

    // Spin -> yield -> capped exponential backoff while the slot `next`
    // wants to reuse is still needed by a consumer. Untuned — placeholders
    // for the right shape of backoff, not validated values.
    static constexpr int                       kSpinAttempts  = 1000;
    static constexpr int                       kYieldAttempts = 100;
    static constexpr std::chrono::microseconds kBackoffStart{50};
    static constexpr std::chrono::microseconds kBackoffCap{5000};

    void wait_for_slot(std::size_t next) {
        if (next < Capacity) return;  // ring hasn't wrapped yet; slot is virgin storage
        const auto reuse_after = next - Capacity;

        // Fast path, checked unconditionally before any loop machinery:
        // the overwhelmingly common case is consumers already caught up,
        // and this runs on every push() past the first lap.
        if (min_cursor() > reuse_after) return;

        full_count_.fetch_add(1, std::memory_order_relaxed);  // genuinely had to wait

        for (int i = 0; i < kSpinAttempts; ++i) {
            if (min_cursor() > reuse_after) return;
        }
        for (int i = 0; i < kYieldAttempts; ++i) {
            std::this_thread::yield();
            if (min_cursor() > reuse_after) return;
        }
        auto backoff = kBackoffStart;
        while (min_cursor() <= reuse_after) {
            std::this_thread::sleep_for(backoff);
            backoff = std::min(backoff * 2, kBackoffCap);
        }
    }

    Storage storage_;
    alignas(64) std::atomic<std::size_t> published_{0};  // producer-owned
    alignas(64) std::atomic<std::uint64_t> full_count_{0};
    Cursor cursors_[NumConsumers];
};

}  // namespace qp
