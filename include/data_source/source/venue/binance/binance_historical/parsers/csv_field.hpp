#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Stamps at or above this are microseconds. Spot switched from milliseconds at
/// 2025-01-01, futures did not, and both units appear inside one dataset.
inline constexpr std::int64_t kMicrosecondFloor = 100'000'000'000'000;

/// Largest stamp that survives the nanosecond scale up, well past any real date.
inline constexpr std::int64_t kStampCeiling = 100'000'000'000'000'000;

/// Digits an int64 mantissa always holds.
inline constexpr int kMaxMantissaDigits = 18;

/// Exact up to 1e22, so the scaling divide stays correctly rounded.
inline constexpr std::array<double, kMaxMantissaDigits + 1> kPow10 = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8, 1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18};

/// Binance stamps in either unit, as nanoseconds.
inline constexpr Timestamp to_nanos(std::int64_t stamp) noexcept {
    return stamp >= kMicrosecondFloor ? stamp * 1'000 : stamp * 1'000'000;
}

constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

/// Consumes the leading field of row and leaves row past its comma.
inline std::string_view take_field(std::string_view& row) noexcept {
    const auto comma = row.find(',');
    if (comma == std::string_view::npos) {
        const auto field = row;
        row              = {};
        return field;
    }
    const auto field = row.substr(0, comma);
    row.remove_prefix(comma + 1);
    return field;
}

/// Anything the digit scan will not take, including exponents and padding.
inline bool take_double_slow(std::string_view& row, double& out) noexcept {
    const auto  field = take_field(row);
    const auto* end   = field.data() + field.size();
    return std::from_chars(field.data(), end, out).ec == std::errc{};
}

/// Fixed point decimal in one pass. Every Binance price and size column is a
/// plain decimal of at most eight places, so the mantissa stays exact.
inline bool take_decimal(std::string_view& row, double& out) noexcept {
    const char* const begin = row.data();
    const char* const end   = begin + row.size();
    const char*       p     = begin;

    bool negative = false;
    if (p != end && (*p == '-' || *p == '+')) negative = (*p++ == '-');

    std::int64_t mantissa = 0;
    int          digits   = 0;
    int          scale    = 0;

    for (; p != end && is_digit(*p); ++p, ++digits) mantissa = mantissa * 10 + (*p - '0');
    if (p != end && *p == '.')
        for (++p; p != end && is_digit(*p); ++p, ++digits, ++scale)
            mantissa = mantissa * 10 + (*p - '0');

    if (digits == 0 || digits > kMaxMantissaDigits || (p != end && *p != ','))
        return take_double_slow(row, out);

    out = static_cast<double>(mantissa) / kPow10[scale];
    if (negative) out = -out;
    row.remove_prefix(p == end ? row.size() : static_cast<std::size_t>(p - begin) + 1);
    return true;
}

inline bool take_integer(std::string_view& row, std::int64_t& out) noexcept {
    const char* const begin = row.data();
    const char* const end   = begin + row.size();
    const char*       p     = begin;

    bool negative = false;
    if (p != end && (*p == '-' || *p == '+')) negative = (*p++ == '-');

    std::int64_t value  = 0;
    int          digits = 0;
    for (; p != end && is_digit(*p); ++p, ++digits) value = value * 10 + (*p - '0');

    if (digits == 0 || digits > kMaxMantissaDigits || (p != end && *p != ',')) return false;

    out = negative ? -value : value;
    row.remove_prefix(p == end ? row.size() : static_cast<std::size_t>(p - begin) + 1);
    return true;
}

inline bool take_stamp(std::string_view& row, Timestamp& out) noexcept {
    const char* const begin = row.data();
    const char* const end   = begin + row.size();
    const char*       p     = begin;

    std::int64_t raw    = 0;
    int          digits = 0;
    for (; p != end && is_digit(*p); ++p, ++digits) raw = raw * 10 + (*p - '0');

    if (digits == 0 || digits > kMaxMantissaDigits || (p != end && *p != ',')) return false;
    if (raw <= 0 || raw >= kStampCeiling) return false;

    out = to_nanos(raw);
    row.remove_prefix(p == end ? row.size() : static_cast<std::size_t>(p - begin) + 1);
    return true;
}

/// True when the row is a header, detected on a non numeric first field.
inline bool is_header_row(std::string_view row) noexcept {
    return row.empty() || !is_digit(row.front());
}

}  // namespace qp::data_source::source::venue::binance::parsers
