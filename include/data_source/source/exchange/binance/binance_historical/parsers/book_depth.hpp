#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "csv_field.hpp"
#include "endpoints.hpp"
#include "types.hpp"

namespace qp::data_source::source::exchange::binance::parsers {

/// Bands on each side of mid, one percent apart.
inline constexpr std::size_t kDepthBands = 5;

/// Builds one sample from its block of rows, one row per band. Every band from
/// -5 to 5 has to appear exactly once, since the bucket has never shipped a
/// short or padded sample. False leaves both outputs untouched from the
/// caller's view.
inline bool parse_book_depth_group(std::string_view block, Timestamp& ts,
                                   BookDepthBands& out) noexcept {
    BookDepthBands bands{};
    Timestamp      stamp{};
    unsigned       seen  = 0;
    std::size_t    count = 0;
    while (!block.empty()) {
        const auto newline = block.find('\n');
        auto       row     = block.substr(0, newline);
        block.remove_prefix(newline == std::string_view::npos ? block.size() : newline + 1);
        if (!row.empty() && row.back() == '\r') row.remove_suffix(1);
        if (++count > 2 * kDepthBands) return false;

        Timestamp    row_ts{};
        std::int64_t percent{};
        double       depth{};
        double       notional{};
        if (!take_datetime(row, row_ts)) return false;
        if (!take_integer(row, percent)) return false;
        if (!take_decimal(row, depth)) return false;
        if (!take_decimal(row, notional)) return false;
        if (!row.empty()) return false;

        if (count == 1)
            stamp = row_ts;
        else if (row_ts != stamp)
            return false;

        const auto limit = static_cast<std::int64_t>(kDepthBands);
        if (percent == 0 || percent < -limit || percent > limit) return false;
        const unsigned bit = 1u << static_cast<unsigned>(percent + limit);
        if (seen & bit) return false;
        seen |= bit;

        auto&      side = percent < 0 ? bands.bids : bands.asks;
        const auto band = static_cast<std::size_t>(percent < 0 ? -percent : percent) - 1;
        side[band]      = DepthBand{.depth = depth, .notional = notional};
    }
    if (count != 2 * kDepthBands) return false;

    ts  = stamp;
    out = bands;
    return true;
}

struct BookDepthParser {
    using Event = BookDepthEvent;

    static constexpr EventKind    event_kind    = EventKind::BookDepth;
    static constexpr EndpointKind endpoint_kind = EndpointKind::BookDepth;
    static constexpr bool         intervalled   = false;

    /// Ten rows make one sample, so the stream hands over the whole run.
    static constexpr bool grouped = true;

    static bool parse(const Endpoint&, std::string_view block, Timestamp& ts, Event& out) noexcept {
        BookDepthBands bands{};
        if (!parse_book_depth_group(block, ts, bands)) return false;
        out.bands = std::make_shared<const BookDepthBands>(bands);
        return true;
    }
};

}  // namespace qp::data_source::source::exchange::binance::parsers
