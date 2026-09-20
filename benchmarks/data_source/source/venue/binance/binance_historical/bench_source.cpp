#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>
#include <vector>

#include "binance_historical_source.hpp"
#include "endpoints.hpp"
#include "exchange.hpp"
#include "fetch_pool.hpp"
#include "subscription.hpp"
#include "types.hpp"

using namespace qp;
using namespace qp::data_source::source::venue::binance;

namespace {

constexpr Timestamp kFrom = 1546300800000000000LL;
constexpr Timestamp kTo   = 1924992000000000000LL;

constexpr std::string_view kRow =
    "1704067200000,42000.10,42100.00,41900.00,42050.00,10.5,1704070799999,414423568.46600,97149,"
    "1862.868,196511690.26110,0\n";

/// Delivers on submit, so the merge is never waiting on network.
class InstantPool {
   public:
    InstantPool() = default;

    explicit InstantPool(const HttpFetchPoolConfig&) {}

    bool submit(std::string, FileSlots* destination, std::size_t at) {
        const auto* bytes = reinterpret_cast<const std::byte*>(body_.data());
        return destination->place(
            at, FetchedFile{std::vector<std::byte>(bytes, bytes + body_.size()), FetchStatus::Ok});
    }

    void quiesce() noexcept {}

    static void set_rows(std::size_t rows) {
        body_.clear();
        body_.reserve(rows * kRow.size());
        for (std::size_t i = 0; i < rows; ++i) body_ += kRow;
    }

   private:
    static inline std::string body_;
};

using Source = BinanceHistoricalSource<InstantPool>;

Subscription make_universe(std::size_t symbols) {
    SubscriptionBuilder builder;
    for (std::size_t i = 0; i < symbols; ++i)
        builder.add(ExchangeId::Binance, static_cast<std::uint16_t>(BinanceMarket::UsdM),
                    "SYM" + std::to_string(i));
    return std::move(builder).build();
}

BinanceHistoricalConfig make_config() {
    return BinanceHistoricalConfig{.streams = {StreamSpec{EndpointKind::Klines, "1h"}},
                                   .pool    = {},
                                   .cadence = Cadence::Daily,
                                   .from    = kFrom,
                                   .to      = kTo};
}

std::vector<std::string> keys_for(std::size_t files) {
    std::vector<std::string> out;
    out.reserve(files);
    for (std::size_t i = 0; i < files; ++i) {
        const auto year  = 2020 + i / (12 * 28);
        const auto month = 1 + (i / 28) % 12;
        const auto day   = 1 + i % 28;

        std::string key = "x/";
        key += std::to_string(year);
        key += month < 10 ? "-0" : "-";
        key += std::to_string(month);
        key += day < 10 ? "-0" : "-";
        key += std::to_string(day);
        key += ".zip";
        out.push_back(std::move(key));
    }
    return out;
}

/// Drains a whole source. The merge is a scan over every stream per event, so
/// this is where the cost of a wide subscription shows up.
void BM_BinanceSource_Drain(benchmark::State& state) {
    const auto            symbols       = static_cast<std::size_t>(state.range(0));
    const auto            rows_per_file = static_cast<std::size_t>(state.range(1));
    constexpr std::size_t kFiles        = 4;

    InstantPool::set_rows(rows_per_file);
    const auto universe = make_universe(symbols);
    const auto keys     = keys_for(kFiles);

    std::int64_t events = 0;
    for (auto _ : state) {
        state.PauseTiming();
        Source source(make_config(), universe);
        source.plan_with([&keys](std::string_view) { return keys; });
        state.ResumeTiming();

        for (;;) {
            auto pulled = source.next();
            if (!pulled) {
                if (pulled.error() == qp::data_source::source::SourceStatus::Eof) break;
                continue;
            }
            benchmark::DoNotOptimize(*pulled);
            ++events;
        }
    }
    state.SetItemsProcessed(events);
    state.counters["streams"] = static_cast<double>(symbols);
}

// Stream count is the axis. Per event cost should grow with it, since the merge
// scans every stream to find the earliest.
BENCHMARK(BM_BinanceSource_Drain)
    ->Args({1, 1440})
    ->Args({4, 1440})
    ->Args({16, 1440})
    ->Args({64, 1440})
    ->Args({256, 1440});

}  // namespace
