#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace qp {

/// Fixed-capacity, single-producer buffer: push/emplace append, reset()
/// rewinds the write position without touching storage. `view()` hands out
/// a non-owning span with no synchronization of its own. Movable, not
/// copyable.
template <typename T, std::size_t Capacity, bool UseHeap = false>
class ViewablePool {
    static_assert(Capacity > 0);
    static_assert(std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>);

    using Storage = std::conditional_t<UseHeap, std::unique_ptr<T[]>, std::array<T, Capacity>>;

   public:
    ViewablePool() {
        if constexpr (UseHeap) storage_ = std::make_unique<T[]>(Capacity);
    }

    ViewablePool(const ViewablePool&)            = delete;
    ViewablePool& operator=(const ViewablePool&) = delete;
    ViewablePool(ViewablePool&&)                 = default;
    ViewablePool& operator=(ViewablePool&&)      = default;

    void push(T value) {
        assert(count_ < Capacity);
        data()[count_++] = value;
    }

    template <class... Args>
    void emplace(Args&&... args) {
        assert(count_ < Capacity);
        data()[count_++] = T{std::forward<Args>(args)...};
    }

    T* begin() noexcept { return data(); }

    T* end() noexcept { return data() + count_; }

    const T* begin() const noexcept { return data(); }

    const T* end() const noexcept { return data() + count_; }

    std::span<const T> view() const noexcept { return {data(), count_}; }

    void reset() noexcept { count_ = 0; }

    std::size_t size() const noexcept { return count_; }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

   private:
    T* data() noexcept {
        if constexpr (UseHeap)
            return storage_.get();
        else
            return storage_.data();
    }

    const T* data() const noexcept {
        if constexpr (UseHeap)
            return storage_.get();
        else
            return storage_.data();
    }

    Storage     storage_{};
    std::size_t count_ = 0;
};

}  // namespace qp
