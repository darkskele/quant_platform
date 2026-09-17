#pragma once
#include "source.hpp"

namespace qp::data_source::source {

// Placeholder Source that reports Eof immediately. Backtest variants that
// have no live Source wired up yet (binary+zstd BinanceHistoricalSource not
// landed) use this to satisfy BacktestBase's sources() contract.
struct EofSource {
    PullResult next() { return std::unexpected(SourceStatus::Eof); }
};

}  // namespace qp::data_source::source
