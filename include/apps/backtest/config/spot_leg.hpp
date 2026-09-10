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

// The spot leg's own Source shape: 1 stream (klines only). Spot has no
// funding, no official mark price, so unlike the futures leg there's
// nothing else to timestamp-merge in.
using SpotSource = LegSource;

// Multi-symbol: 1 klines stream per symbol, merged by timestamp. Each row
// carries its own symbol, so identity survives the merge.
inline SpotSource make_spot_source(const std::filesystem::path& data_dir,
                                   std::span<const std::string> symbols,
                                   std::chrono::year_month_day  first_day,
                                   std::chrono::year_month_day  last_day) {
    std::vector<std::vector<std::filesystem::path>> streams;
    streams.reserve(symbols.size());
    for (std::string_view symbol : symbols)
        streams.push_back(daily_files(data_dir, symbol, "spot/klines", first_day, last_day));
    return SpotSource{std::move(streams)};
}

inline SpotSource make_spot_source(const std::filesystem::path& data_dir, std::string_view symbol,
                                   std::chrono::year_month_day first_day,
                                   std::chrono::year_month_day last_day) {
    std::array<std::string, 1> one{std::string(symbol)};
    return make_spot_source(data_dir, std::span<const std::string>{one}, first_day, last_day);
}

}  // namespace qp::backtest::config
