#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>

#include "types.hpp"

namespace qp::data_source::source::exchange::binance::parsers {

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

/// Text before the first comma, without consuming it. Rows of one group share
/// it byte for byte, so comparing it finds a group's end without parsing.
inline std::string_view leading_field(std::string_view row) noexcept {
    return row.substr(0, row.find(','));
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

    for (; p != end && is_digit(*p); ++p, ++digits)
        if (digits < kMaxMantissaDigits) mantissa = mantissa * 10 + (*p - '0');
    if (p != end && *p == '.')
        for (++p; p != end && is_digit(*p); ++p, ++digits, ++scale)
            if (digits < kMaxMantissaDigits) mantissa = mantissa * 10 + (*p - '0');

    if (digits == 0 || digits > kMaxMantissaDigits || (p != end && *p != ','))
        return take_double_slow(row, out);

    out = static_cast<double>(mantissa) / kPow10[scale];
    if (negative) out = -out;
    row.remove_prefix(p == end ? row.size() : static_cast<std::size_t>(p - begin) + 1);
    return true;
}

/// A blank field is a figure the venue did not publish.
/// Coin-M metrics leaves its three long short ratios empty
/// on every row. NaN insteadX.
inline bool take_optional_decimal(std::string_view& row, double& out) noexcept {
    if (row.empty() || row.front() == ',') {
        if (!row.empty()) row.remove_prefix(1);
        out = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    return take_decimal(row, out);
}

/// true or false in either case. Futures write lowercase and spot capitalises.
inline bool take_bool(std::string_view& row, bool& out) noexcept {
    const auto comma = row.find(',');
    const auto field = row.substr(0, comma);
    if (field == "true" || field == "True")
        out = true;
    else if (field == "false" || field == "False")
        out = false;
    else
        return false;
    row.remove_prefix(comma == std::string_view::npos ? row.size() : comma + 1);
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
    for (; p != end && is_digit(*p); ++p, ++digits)
        if (digits < kMaxMantissaDigits) value = value * 10 + (*p - '0');

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
    for (; p != end && is_digit(*p); ++p, ++digits)
        if (digits < kMaxMantissaDigits) raw = raw * 10 + (*p - '0');

    if (digits == 0 || digits > kMaxMantissaDigits || (p != end && *p != ',')) return false;
    if (raw <= 0 || raw >= kStampCeiling) return false;

    out = to_nanos(raw);
    row.remove_prefix(p == end ? row.size() : static_cast<std::size_t>(p - begin) + 1);
    return true;
}

/// Days since the unix epoch, Howard Hinnant's civil calendar algorithm.
inline constexpr std::int64_t days_from_civil(int y, int m, int d) noexcept {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto         yoe = static_cast<unsigned>(y - era * 400);
    const auto         doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const auto         doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146'097 + static_cast<std::int64_t>(doe) - 719'468;
}

inline constexpr int days_in_month(int year, int month) noexcept {
    constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : kDays[month - 1];
}

/// Fixed width digit run, the only shape the datetime columns ever take.
inline constexpr bool fixed_digits(std::string_view field, std::size_t pos, std::size_t count,
                                   int& out) noexcept {
    int value = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const char c = field[pos + i];
        if (!is_digit(c)) return false;
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

/// YYYY-MM-DD HH:MM:SS, the stamp the metrics and bookDepth datasets write.
/// Every other dataset is epoch millis or micros, which take_stamp handles.
/// UTC, since that is the only zone the bucket publishes in.
inline bool take_datetime(std::string_view& row, Timestamp& out) noexcept {
    constexpr std::size_t kWidth = 19;
    if (row.size() < kWidth) return false;

    const auto field = row.substr(0, kWidth);
    if (field[4] != '-' || field[7] != '-' || field[10] != ' ' || field[13] != ':' ||
        field[16] != ':')
        return false;

    int year{}, month{}, day{}, hour{}, minute{}, second{};
    if (!fixed_digits(field, 0, 4, year) || !fixed_digits(field, 5, 2, month) ||
        !fixed_digits(field, 8, 2, day) || !fixed_digits(field, 11, 2, hour) ||
        !fixed_digits(field, 14, 2, minute) || !fixed_digits(field, 17, 2, second))
        return false;

    // Leap seconds are not in these files, so 60 is a malformed row and not a
    // real instant.
    if (day < 1 || day > days_in_month(year, month)) return false;
    if (hour > 23 || minute > 59 || second > 59) return false;

    // A longer field is a different format, not this one with a tail.
    if (row.size() > kWidth && row[kWidth] != ',') return false;

    const std::int64_t days = days_from_civil(year, month, day);
    const std::int64_t secs = days * 86'400LL + hour * 3'600LL + minute * 60LL + second;
    out                     = secs * 1'000'000'000LL;
    row.remove_prefix(row.size() > kWidth ? kWidth + 1 : kWidth);
    return true;
}

/// True when the row is a header, detected on a non numeric first field.
inline bool is_header_row(std::string_view row) noexcept {
    return row.empty() || !is_digit(row.front());
}

}  // namespace qp::data_source::source::exchange::binance::parsers
