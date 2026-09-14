#pragma once
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cost_row.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear {

/// Loads a cost table CSV into `vector<CostRow>`. The CSV is expected to
/// carry a header row naming at least these columns (extras are ignored):
///   symbol, market, week_start, half_spread_bps, impact_bps_per_unit,
///   taker_fee_bps
template <class SymbolTable>
std::vector<CostRow> read_cost_rows_csv(const std::filesystem::path& path);

namespace detail {

inline std::optional<Timestamp> parse_iso_date_to_ns(std::string_view s) noexcept {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
    auto digits = [](std::string_view t) -> std::optional<int> {
        int v = 0;
        for (char c : t) {
            if (c < '0' || c > '9') return std::nullopt;
            v = v * 10 + (c - '0');
        }
        return v;
    };
    auto y = digits(s.substr(0, 4));
    auto m = digits(s.substr(5, 2));
    auto d = digits(s.substr(8, 2));
    if (!y || !m || !d) return std::nullopt;
    std::chrono::year_month_day ymd{std::chrono::year{*y}, std::chrono::month{unsigned(*m)},
                                    std::chrono::day{unsigned(*d)}};
    if (!ymd.ok()) return std::nullopt;
    // Cast to nanoseconds since epoch.
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::sys_days{ymd})
        .time_since_epoch()
        .count();
}

inline std::optional<double> parse_optional_double(std::string_view s) noexcept {
    if (s.empty()) return std::nullopt;
    double v{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    if (std::isnan(v)) return std::nullopt;
    return v;
}

inline std::vector<std::string_view> split_csv(std::string_view line) {
    std::vector<std::string_view> out;
    out.reserve(12);
    std::size_t start = 0;
    while (start <= line.size()) {
        std::size_t comma = line.find(',', start);
        if (comma == std::string_view::npos) {
            out.emplace_back(line.substr(start));
            break;
        }
        out.emplace_back(line.substr(start, comma - start));
        start = comma + 1;
    }
    return out;
}

struct HeaderIndex {
    int symbol{-1}, market{-1}, week_start{-1};
    int half_spread_bps{-1}, impact_bps_per_unit{-1}, taker_fee_bps{-1};
};

inline HeaderIndex parse_header(std::string_view header_line) {
    HeaderIndex h{};
    auto        fields = split_csv(header_line);
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        auto f = fields[i];
        if (f == "symbol")
            h.symbol = i;
        else if (f == "market")
            h.market = i;
        else if (f == "week_start")
            h.week_start = i;
        else if (f == "half_spread_bps")
            h.half_spread_bps = i;
        else if (f == "impact_bps_per_unit")
            h.impact_bps_per_unit = i;
        else if (f == "taker_fee_bps")
            h.taker_fee_bps = i;
    }
    return h;
}

inline bool header_complete(const HeaderIndex& h) noexcept {
    return h.symbol >= 0 && h.market >= 0 && h.week_start >= 0 && h.half_spread_bps >= 0 &&
           h.impact_bps_per_unit >= 0 && h.taker_fee_bps >= 0;
}

}  // namespace detail

template <class SymbolTable>
std::vector<CostRow> read_cost_rows_csv(const std::filesystem::path& path) {
    std::ifstream in{path};
    if (!in) throw std::runtime_error("cost_table csv not readable: " + path.string());

    std::string header;
    if (!std::getline(in, header)) {
        throw std::runtime_error("cost_table csv empty: " + path.string());
    }
    // Strip trailing CR from CRLF endings.
    if (!header.empty() && header.back() == '\r') header.pop_back();

    auto hdr = detail::parse_header(header);
    if (!detail::header_complete(hdr)) {
        throw std::runtime_error("cost_table csv header missing required columns: " +
                                 path.string());
    }

    std::vector<CostRow> rows;
    std::string          line;
    std::size_t          line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto fields = detail::split_csv(line);

        auto need       = [&](int idx) -> std::string_view { return fields[idx]; };
        auto max_needed = std::max({hdr.symbol, hdr.market, hdr.week_start, hdr.half_spread_bps,
                                    hdr.impact_bps_per_unit, hdr.taker_fee_bps});
        if (static_cast<int>(fields.size()) <= max_needed) {
            std::ostringstream oss;
            oss << "cost_table csv line " << line_no << " has too few fields";
            throw std::runtime_error(oss.str());
        }

        auto sym_id = SymbolTable::id_of(need(hdr.symbol));
        if (!sym_id) continue;  // symbol not in this build's universe, skip cleanly

        auto ven_str = need(hdr.market);
        int  ven_i{};
        if (std::from_chars(ven_str.data(), ven_str.data() + ven_str.size(), ven_i).ec !=
            std::errc{}) {
            std::ostringstream oss;
            oss << "cost_table csv line " << line_no << " has bad market '" << ven_str << "'";
            throw std::runtime_error(oss.str());
        }

        auto ws_ns = detail::parse_iso_date_to_ns(need(hdr.week_start));
        if (!ws_ns) {
            std::ostringstream oss;
            oss << "cost_table csv line " << line_no << " has bad week_start '"
                << need(hdr.week_start) << "'";
            throw std::runtime_error(oss.str());
        }

        auto hs = detail::parse_optional_double(need(hdr.half_spread_bps));
        auto tf = detail::parse_optional_double(need(hdr.taker_fee_bps));
        if (!hs || !tf) {
            std::ostringstream oss;
            oss << "cost_table csv line " << line_no
                << " has missing or NaN half_spread_bps or taker_fee_bps";
            throw std::runtime_error(oss.str());
        }

        // Empty impact is treated as 0 (for weeks with no aggTrades summary).
        // Non-empty but non-numeric is a genuine bad value and throws.
        auto   im_field = need(hdr.impact_bps_per_unit);
        double im_val   = 0.0;
        if (!im_field.empty()) {
            auto parsed = detail::parse_optional_double(im_field);
            if (!parsed) {
                std::ostringstream oss;
                oss << "cost_table csv line " << line_no << " has bad impact_bps_per_unit '"
                    << im_field << "'";
                throw std::runtime_error(oss.str());
            }
            im_val = *parsed;
        }

        rows.push_back(CostRow{
            .symbol              = *sym_id,
            .market               = static_cast<MarketId>(ven_i),
            .week_start_ns       = *ws_ns,
            .half_spread_bps     = *hs,
            .impact_bps_per_unit = im_val,
            .taker_fee_bps       = *tf,
        });
    }
    return rows;
}

}  // namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear
