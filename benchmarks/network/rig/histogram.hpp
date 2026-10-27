#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace qp::network::rig {

/// Counts of nanosecond values. Values below 2^PrecisionBits each get their own bucket. Above,
/// every doubling gets 2^PrecisionBits buckets, so a value reads back within 1 part in
/// 2^PrecisionBits. Values from 2^RangeBits on share the last bucket.
/// @tparam PrecisionBits buckets per doubling, as a power of two.
/// @tparam RangeBits values below 2^RangeBits nanoseconds are bucketed.
template <int PrecisionBits, int RangeBits>
class LogLinearHistogram {
    static_assert(PrecisionBits >= 1 && PrecisionBits < RangeBits && RangeBits <= 62,
                  "1 <= PrecisionBits < RangeBits <= 62");

   public:
    static constexpr std::int64_t PER_GROUP = std::int64_t{1} << PrecisionBits;
    // Group 0 holds the values below PER_GROUP.
    static constexpr std::size_t BUCKETS = (RangeBits - PrecisionBits + 1) * PER_GROUP;

    /// A negative value is counted apart and kept out of the buckets.
    void record(std::int64_t v) noexcept {
        if (v < 0) {
            ++negative_;
            return;
        }
        ++counts_[index(v)];
        ++count_;
        sum_ += static_cast<double>(v);
        min_ = std::min(min_, v);
        max_ = std::max(max_, v);
    }

    /// The bucket holding v.
    static constexpr std::size_t index(std::int64_t v) noexcept {
        // Below PER_GROUP every value is its own bucket.
        if (v < PER_GROUP) return static_cast<std::size_t>(v);
        // v's range is the doubling from 2^high_bit, cut into PER_GROUP buckets.
        const int high_bit = std::bit_width(static_cast<std::uint64_t>(v)) - 1;
        if (high_bit >= RangeBits) return BUCKETS - 1;
        const std::int64_t range_start = std::int64_t{1} << high_bit;
        const int bucket_bits = high_bit - PrecisionBits;  // a bucket is 2^bucket_bits long
        const std::int64_t range_index = std::int64_t{bucket_bits + 1} << PrecisionBits;
        return static_cast<std::size_t>(range_index + ((v - range_start) >> bucket_bits));
    }

    /// The smallest value in bucket i.
    static constexpr std::int64_t lower(std::size_t i) noexcept {
        if (i < PER_GROUP) return static_cast<std::int64_t>(i);
        // index run backwards. Which range, then how many buckets into it.
        const int          bucket_bits = static_cast<int>(i >> PrecisionBits) - 1;
        const std::int64_t range_start = std::int64_t{1} << (bucket_bits + PrecisionBits);
        const std::int64_t into_range  = static_cast<std::int64_t>(i % PER_GROUP);
        return range_start + (into_range << bucket_bits);
    }

    /// The largest value in bucket i.
    static constexpr std::int64_t upper(std::size_t i) noexcept {
        if (i < PER_GROUP) return static_cast<std::int64_t>(i);
        const int bucket_bits = static_cast<int>(i >> PrecisionBits) - 1;
        return lower(i) + (std::int64_t{1} << bucket_bits) - 1;
    }

    /// The value at or below which p percent fall, as the top of its bucket. 0 when empty.
    std::int64_t percentile(double p) const noexcept {
        if (count_ == 0) return 0;
        if (p <= 0) return min_;
        const auto rank = std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(std::ceil(p / 100.0 * static_cast<double>(count_))));
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < BUCKETS; ++i) {
            seen += counts_[i];
            if (seen >= rank) return std::min(upper(i), max_);
        }
        return max_;
    }

    /// Calls f(lower, upper, count) for every bucket that holds a value, lowest first.
    template <class F>
    void each(F&& f) const {
        for (std::size_t i = 0; i < BUCKETS; ++i)
            if (counts_[i]) f(lower(i), upper(i), counts_[i]);
    }

    std::uint64_t count() const noexcept { return count_; }

    std::uint64_t negative() const noexcept { return negative_; }

    std::int64_t min() const noexcept { return count_ ? min_ : 0; }

    std::int64_t max() const noexcept { return count_ ? max_ : 0; }

    double mean() const noexcept { return count_ ? sum_ / static_cast<double>(count_) : 0; }

   private:
    std::vector<std::uint64_t> counts_   = std::vector<std::uint64_t>(BUCKETS);
    std::uint64_t              count_    = 0;
    std::uint64_t              negative_ = 0;
    double                     sum_      = 0;
    std::int64_t               min_      = std::numeric_limits<std::int64_t>::max();
    std::int64_t               max_      = 0;
};

/// 128 buckets per doubling, within 0.8 percent, up to about 18 minutes.
using Histogram = LogLinearHistogram<7, 40>;

}  // namespace qp::network::rig
