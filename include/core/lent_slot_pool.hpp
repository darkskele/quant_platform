#pragma once
#include <sys/mman.h>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

namespace qp {

template <std::size_t SlotSize, std::size_t Count>
class LentSlotPool;

/// Indices of a pool's free slots.
class FreeSlots {
   public:
    /// Frees a slot.
    void push(std::uint32_t index) noexcept { items_[size_++] = index; }

    /// Takes the most recently freed slot, nothing when none are free.
    std::optional<std::uint32_t> pop() noexcept {
        if (size_ == 0) return std::nullopt;
        return items_[--size_];
    }

    std::size_t size() const noexcept { return size_; }

   protected:
    explicit FreeSlots(std::uint32_t* items) noexcept : items_(items) {}

   private:
    std::uint32_t* items_;
    std::size_t    size_ = 0;
};

/// FreeSlots holding its indices in place, starting with all Count free.
/// @tparam Count slot count.
template <std::size_t Count>
class FreeSlotsOf : public FreeSlots {
   public:
    FreeSlotsOf() noexcept : FreeSlots(storage_) {
        // Pushed highest first, so slot 0 is taken first.
        for (std::size_t i = Count; i > 0; --i) push(static_cast<std::uint32_t>(i - 1));
    }

   private:
    std::uint32_t storage_[Count];
};

/// A view into one slot of a pool. Destroying it frees the slot.
class [[nodiscard]] LentSlot {
   public:
    LentSlot() = default;

    ~LentSlot() { release(); }

    LentSlot(LentSlot&& other) noexcept
        : bytes_(other.bytes_), free_(std::exchange(other.free_, nullptr)), index_(other.index_) {}

    LentSlot& operator=(LentSlot&& other) noexcept {
        if (this != &other) {
            release();
            bytes_ = other.bytes_;
            free_  = std::exchange(other.free_, nullptr);
            index_ = other.index_;
        }
        return *this;
    }

    LentSlot(const LentSlot&)            = delete;
    LentSlot& operator=(const LentSlot&) = delete;

    /// The lent bytes, writable in place. Valid while this view lives.
    std::span<std::byte> bytes() const& noexcept { return bytes_; }

    std::span<std::byte> bytes() && = delete;

    /// Whether this view holds a slot. A zero length view still does.
    explicit operator bool() const noexcept { return free_ != nullptr; }

   private:
    template <std::size_t SlotSize, std::size_t Count>
    friend class LentSlotPool;

    LentSlot(std::span<std::byte> bytes, FreeSlots* free, std::uint32_t index) noexcept
        : bytes_(bytes), free_(free), index_(index) {}

    void release() noexcept {
        if (free_) free_->push(index_);
        free_ = nullptr;
    }

    std::span<std::byte> bytes_;
    FreeSlots*           free_  = nullptr;  // null once released
    std::uint32_t        index_ = 0;
};

/// Single-thread pool of equal slots in one contiguous mapping. A slot is acquired empty, filled
/// in place, then lent out and freed in any order.
/// @tparam SlotSize slot bytes, a multiple of the cache line.
/// @tparam Count slot count.
template <std::size_t SlotSize, std::size_t Count>
class LentSlotPool {
    static_assert(SlotSize > 0 && SlotSize % 64 == 0, "SlotSize must be a multiple of 64");
    static_assert(Count > 0 && Count <= std::numeric_limits<std::uint32_t>::max(),
                  "Count must fit a 32 bit index");

    static constexpr std::size_t BYTES = SlotSize * Count;

   public:
    /// Throws on failure.
    LentSlotPool() : free_(std::make_unique<FreeSlotsOf<Count>>()) {
        // Populated now, so no slot takes a page fault on first use.
        void* addr = ::mmap(nullptr, BYTES, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        if (addr == MAP_FAILED)
            throw std::system_error(errno, std::system_category(), "slot pool mapping");
        base_ = static_cast<std::byte*>(addr);
    }

    ~LentSlotPool() {
        if (!base_) return;
        assert(free_->size() == Count && "a slot outlived its pool");
        ::munmap(base_, BYTES);
    }

    LentSlotPool(LentSlotPool&& other) noexcept
        : base_(std::exchange(other.base_, nullptr)), free_(std::move(other.free_)) {}

    LentSlotPool& operator=(LentSlotPool&&)      = delete;
    LentSlotPool(const LentSlotPool&)            = delete;
    LentSlotPool& operator=(const LentSlotPool&) = delete;

    /// Takes an empty slot to fill through slot(), then lend or release. Nothing when every slot
    /// is out.
    std::optional<std::uint32_t> acquire() noexcept { return free_->pop(); }

    /// The whole of an acquired slot, for its filler to write in place.
    std::span<std::byte> slot(std::uint32_t index) const noexcept {
        assert(index < Count);
        return {base_ + std::size_t{index} * SlotSize, SlotSize};
    }

    /// Ends filling and lends len bytes of the slot from offset. Destroying the view frees the
    /// slot.
    LentSlot lend(std::uint32_t index, std::size_t offset, std::size_t len) noexcept {
        assert(offset <= SlotSize && len <= SlotSize - offset);
        return {slot(index).subspan(offset, len), free_.get(), index};
    }

    /// Frees an acquired slot that was never filled.
    void release(std::uint32_t index) noexcept {
        assert(index < Count && free_->size() < Count);
        free_->push(index);
    }

    /// Slots ready to acquire.
    std::size_t free() const noexcept { return free_->size(); }

    static constexpr std::size_t slot_size() noexcept { return SlotSize; }

    static constexpr std::size_t count() noexcept { return Count; }

   private:
    std::byte* base_ = nullptr;
    // On the heap, so lent slots still reach it after the pool moves.
    std::unique_ptr<FreeSlotsOf<Count>> free_;
};

}  // namespace qp
