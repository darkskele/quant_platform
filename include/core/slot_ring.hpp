#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

namespace qp {

/// Bounded ring addressed by absolute position rather than by a shared cursor.
/// Producers fill positions in any order, one producer per position, and the
/// single consumer takes them strictly in position order.
/// @tparam Capacity slot count, a power of two, all of them usable.
template <typename T, std::size_t Capacity>
class SlotRing {
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert(std::has_single_bit(Capacity), "Capacity must be a power of 2");

    static constexpr std::size_t INDEX_MASK = Capacity - 1;

    // seq sits with the slot it guards, so filling one touches a single line.
    struct Cell {
        std::atomic<std::size_t> seq;
        alignas(T) std::byte storage[sizeof(T)];
    };

   public:
    SlotRing() {
        // Slot i awaits the producer at position i.
        for (std::size_t i = 0; i < Capacity; ++i)
            cells_[i].seq.store(i, std::memory_order_relaxed);
    }

    ~SlotRing() {
        for (std::size_t i = 0; i < Capacity; ++i)
            if (filled(i)) std::destroy_at(elem(i));
    }

    SlotRing(const SlotRing&)            = delete;
    SlotRing& operator=(const SlotRing&) = delete;

    /// Producer side, one thread per position. False when the slot has not been
    /// freed yet, which means more positions were handed out than fit.
    template <typename... Args>
    bool place(std::size_t position, Args&&... args) {
        static_assert(std::is_constructible_v<T, Args...>);

        Cell& cell = cells_[position & INDEX_MASK];
        // A slot is free only for the lap it is stamped for. acquire pairs with
        // take's release of position + Capacity.
        if (cell.seq.load(std::memory_order_acquire) != position) return false;

        std::construct_at(reinterpret_cast<T*>(&cell.storage), std::forward<Args>(args)...);
        // release publishes the value to the consumer at this position.
        cell.seq.store(position + 1, std::memory_order_release);
        return true;
    }

    /// True when the consumer at this position would get a value.
    bool ready(std::size_t position) const noexcept {
        return cells_[position & INDEX_MASK].seq.load(std::memory_order_acquire) == position + 1;
    }

    /// Consumer side, one thread, positions taken in ascending order.
    std::optional<T> take(std::size_t position) {
        Cell& cell = cells_[position & INDEX_MASK];
        if (cell.seq.load(std::memory_order_acquire) != position + 1) return std::nullopt;

        T*               slot = std::launder(reinterpret_cast<T*>(&cell.storage));
        std::optional<T> out{std::move(*slot)};
        std::destroy_at(slot);
        // release frees the slot for the producer one lap on.
        cell.seq.store(position + Capacity, std::memory_order_release);
        return out;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    // Slot i only ever holds a sequence congruent to i when free and to i + 1
    // when filled, so the residue is what distinguishes them at destruction.
    bool filled(std::size_t index) const noexcept {
        const auto seq = cells_[index].seq.load(std::memory_order_relaxed);
        return ((seq - index) & INDEX_MASK) == 1;
    }

    T* elem(std::size_t index) noexcept {
        return std::launder(reinterpret_cast<T*>(&cells_[index].storage));
    }

    std::array<Cell, Capacity> cells_;
};

}  // namespace qp
