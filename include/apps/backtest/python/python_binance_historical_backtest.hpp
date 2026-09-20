#pragma once
#include <tuple>
#include <utility>

#include "binance_historical_source.hpp"
#include "http_fetch_pool.hpp"
#include "python_backtest.hpp"
#include "subscription.hpp"

namespace qp::backtest::python {

/// A python backtest fed by Binance historical files.
///
/// @tparam Matcher the fill model.
template <class Matcher>
class PythonBinanceHistoricalBacktest
    : public PythonBacktestBase<PythonBinanceHistoricalBacktest<Matcher>, Matcher> {
    using Base   = PythonBacktestBase<PythonBinanceHistoricalBacktest<Matcher>, Matcher>;
    using Source = data_source::source::exchange::binance::BinanceHistoricalSource<
        data_source::source::exchange::binance::HttpFetchPool>;

   public:
    using Config = data_source::source::exchange::binance::BinanceHistoricalConfig;

    PythonBinanceHistoricalBacktest(Subscription subscription, Config config,
                                    typename Base::MakeMatcher make_matcher)
        : Base(subscription, std::move(make_matcher)), source_(std::move(config), subscription) {}

    /// Discovers every stream's files.
    void plan() { source_.plan(); }

    std::size_t stream_count() const noexcept { return source_.stream_count(); }

    /// What each stream managed to read.
    auto reports() const { return source_.reports(); }

    auto fetch_stats() const { return source_.pool().stats(); }

    /// The recent failure ring, so a run can say which files went wrong.
    auto fetch_failures() const { return source_.pool().recent_failures(); }

    auto sources() { return std::tie(source_); }

   private:
    Source source_;
};

}  // namespace qp::backtest::python
