#pragma once
#include <chrono>
#include <filesystem>
#include <string_view>

#include "data_layout.hpp"
#include "source_config.hpp"

namespace qp::backtest::config {

// The spot leg's own Source shape: 1 stream (klines only). Spot has no
// funding, no official mark price, so unlike the futures leg there's
// nothing else to timestamp-merge in.
using SpotSource = LegSource;

inline SpotSource make_spot_source(const std::filesystem::path& data_dir, std::string_view symbol,
                                   std::chrono::year_month_day first_day,
                                   std::chrono::year_month_day last_day) {
    return SpotSource{{daily_files(data_dir, symbol, "spot/klines", first_day, last_day)}};
}

}  // namespace qp::backtest::config
