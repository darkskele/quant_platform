#pragma once
#include <cstddef>

// One macro-selected combo per build — QP_SOURCE_* picks which Source +
// Parser this binary is compiled against. Adding a real combo means adding
// a branch here, never touching backtest.cpp: it only ever names
// config::LegSource<N>.
#if defined(QP_SOURCE_CSV_BINHIST)
#include "bin_hist_venue.hpp"
#include "csv_source.hpp"

namespace qp::backtest::config {
using LegParser = data_source::source::venue::binance::binance_historical::BinHistVenue;
template <std::size_t N>
using LegSource = data_source::source::CsvSource<LegParser, N>;
}  // namespace qp::backtest::config
#else
#error "define exactly one QP_SOURCE_* macro (see cmake/apps/backtest/CMakeLists.txt)"
#endif
