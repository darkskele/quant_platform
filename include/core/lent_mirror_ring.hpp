#pragma once
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

namespace qp {

template <std::size_t Size, std::size_t MaxLent>
class LentMirrorRing;

/// Bytes lent by a ring. Destroying it hands them back.
class [[nodiscard]] LentBytes {
   public:
    LentBytes() = default;

    ~LentBytes() { release(); }

    LentBytes(LentBytes&& other) noexcept
        : bytes_(other.bytes_), done_(std::exchange(other.done_, nullptr)) {}

    LentBytes& operator=(LentBytes&& other) noexcept {
        if (this != &other) {
            release();
            bytes_ = other.bytes_;
            done_  = std::exchange(other.done_, nullptr);
        }
        return *this;
    }

    LentBytes(const LentBytes&)            = delete;
    LentBytes& operator=(const LentBytes&) = delete;

    std::span<std::byte> bytes() const& noexcept { return bytes_; }

    std::span<std::byte> bytes() && = delete;

   private:
    template <std::size_t Size, std::size_t MaxLent>
    friend class LentMirrorRing;

    LentBytes(std::span<std::byte> bytes, bool* done) noexcept : bytes_(bytes), done_(done) {}

    void release() noexcept {
        if (done_) *done_ = true;
        done_ = nullptr;
    }

    std::span<std::byte> bytes_;
    bool*                done_ = nullptr;  // the ring's flag for this range
};

/// Single-thread byte ring mapped twice in a row, so every span over it is contiguous across the
/// end. Bytes are written in, then lent out in order and handed back in any order.
/// @tparam Size ring bytes, a power of two and a page multiple.
/// @tparam MaxLent ranges lent at once, a power of two.
template <std::size_t Size, std::size_t MaxLent>
class LentMirrorRing {
    static_assert(std::has_single_bit(Size) && Size >= 4096,
                  "Size must be a power of two of at least a page");
    static_assert(std::has_single_bit(MaxLent), "MaxLent must be a power of 2");

    static constexpr std::size_t BYTE_MASK  = Size - 1;
    static constexpr std::size_t RANGE_MASK = MaxLent - 1;

    struct Range {
        std::uint64_t end  = 0;
        bool          done = true;
    };

   public:
    /// Throws on failure.
    LentMirrorRing() : ranges_(std::make_unique<Range[]>(MaxLent)) {
        if (Size % static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)) != 0)
            throw std::invalid_argument("Size must be a page multiple");
        map();
    }

    ~LentMirrorRing() {
        if (!base_) return;
        reclaim();
        assert(oldest_ == next_ && "a lent range outlived its ring");
        ::munmap(base_, 2 * Size);
    }

    LentMirrorRing(LentMirrorRing&& other) noexcept
        : base_(std::exchange(other.base_, nullptr)),
          ranges_(std::move(other.ranges_)),
          freed_(other.freed_),
          lent_(other.lent_),
          filled_(other.filled_),
          oldest_(other.oldest_),
          next_(other.next_) {}

    LentMirrorRing& operator=(LentMirrorRing&&)      = delete;
    LentMirrorRing(const LentMirrorRing&)            = delete;
    LentMirrorRing& operator=(const LentMirrorRing&) = delete;

    /// fill writes into the free space, one run that may cross the end, and returns how many
    /// bytes it wrote.
    template <class F>
        requires std::is_invocable_r_v<std::size_t, F&, std::span<std::byte>>
    std::size_t write(F&& fill) noexcept(std::is_nothrow_invocable_v<F&, std::span<std::byte>>) {
        reclaim();
        const std::span<std::byte> space{base_ + (filled_ & BYTE_MASK),
                                         Size - static_cast<std::size_t>(filled_ - freed_)};
        const std::size_t          n = fill(space);
        assert(n <= space.size());
        filled_ += n;
        return n;
    }

    /// Empty when nothing is unlent or MaxLent ranges are already out.
    LentBytes lend(std::size_t n) noexcept {
        reclaim();
        const auto len = std::min(n, unlent());
        if (len == 0 || next_ - oldest_ == MaxLent) return {};
        Range& r = ranges_[next_++ & RANGE_MASK];
        r        = {lent_ + len, false};
        LentBytes out{{base_ + (lent_ & BYTE_MASK), len}, &r.done};
        lent_ += len;
        return out;
    }

    std::size_t unlent() const noexcept { return static_cast<std::size_t>(filled_ - lent_); }

    static constexpr std::size_t size() noexcept { return Size; }

   private:
    void map() {
        const int fd = ::memfd_create("lent_mirror_ring", MFD_CLOEXEC);
        if (fd < 0) throw std::system_error(errno, std::system_category(), "memfd_create");
        int   err  = 0;
        void* addr = MAP_FAILED;
        // Reserve both halves first, so nothing else lands between them.
        if (::ftruncate(fd, static_cast<off_t>(Size)) != 0 ||
            (addr = ::mmap(nullptr, 2 * Size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) ==
                MAP_FAILED)
            err = errno;
        auto* base = static_cast<std::byte*>(addr);
        if (err == 0)
            for (std::byte* half : {base, base + Size})
                if (::mmap(half, Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) ==
                    MAP_FAILED) {
                    err = errno;
                    break;
                }
        ::close(fd);
        if (err != 0) {
            if (addr != MAP_FAILED) ::munmap(addr, 2 * Size);
            throw std::system_error(err, std::system_category(), "mirror mapping");
        }
        base_ = base;
    }

    // Frees from the oldest range forward while each is done.
    void reclaim() noexcept {
        while (oldest_ != next_ && ranges_[oldest_ & RANGE_MASK].done)
            freed_ = ranges_[oldest_++ & RANGE_MASK].end;
    }

    std::byte*               base_ = nullptr;
    std::unique_ptr<Range[]> ranges_;

    // Byte positions only grow. freed_ <= lent_ <= filled_.
    std::uint64_t freed_  = 0;
    std::uint64_t lent_   = 0;
    std::uint64_t filled_ = 0;

    // Range positions, oldest_ is the first still open.
    std::uint64_t oldest_ = 0;
    std::uint64_t next_   = 0;
};

}  // namespace qp
