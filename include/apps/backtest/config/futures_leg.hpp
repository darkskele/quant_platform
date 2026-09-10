#pragma once
#include <array>
#include <chrono>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data_layout.hpp"
#include "source_config.hpp"

namespace qp::backtest::config {

// The futures leg's own Source shape: 3 streams (klines, markprice,
// funding) merged by timestamp, not simple concatenation: funding's
// monthly file and klines'/markprice's daily files interleave in time, so
// each kind needs its own CsvSource stream index for next()'s own
// earliest-wins merge to interleave them correctly (concatenating them
// into one stream would put every month's handful of funding rows
// completely out of chronological order relative to the day-by-day
// klines). Same LegParser as the spot leg today (source_config.hpp). Nothing
// requires that to stay true; a different venue backing this leg would
// pick its own Parser here without spot's leg needing to change.
using FuturesSource = LegSource;

// Multi-symbol: 3 streams per symbol, all merged by timestamp in one
// Source. Each row carries its own symbol, so identity survives the merge.
inline FuturesSource make_futures_source(const std::filesystem::path& data_dir,
                                         std::span<const std::string> symbols,
                                         std::chrono::year_month_day  first_day,
                                         std::chrono::year_month_day  last_day) {
    std::vector<std::vector<std::filesystem::path>> streams;
    streams.reserve(symbols.size() * 3);
    for (std::string_view symbol : symbols) {
        streams.push_back(daily_files(data_dir, symbol, "futures/klines", first_day, last_day));
        streams.push_back(daily_files(data_dir, symbol, "futures/markprice", first_day, last_day));
        streams.push_back(monthly_funding_files(data_dir, symbol, first_day, last_day));
    }
    return FuturesSource{std::move(streams)};
}

inline FuturesSource make_futures_source(const std::filesystem::path& data_dir,
                                         std::string_view             symbol,
                                         std::chrono::year_month_day  first_day,
                                         std::chrono::year_month_day  last_day) {
    std::array<std::string, 1> one{std::string(symbol)};
    return make_futures_source(data_dir, std::span<const std::string>{one}, first_day, last_day);
}

}  // namespace qp::backtest::config
