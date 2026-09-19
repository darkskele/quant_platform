#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "endpoints.hpp"
#include "fetch_pool.hpp"
#include "parsers/klines.hpp"
#include "stream.hpp"
#include "subscription.hpp"
#include "types.hpp"

using namespace qp;
using namespace qp::data_source::source::venue::binance;

namespace {

constexpr Subscription::Instrument kInstrument{.exchange = 0, .market = 0, .symbol = 1};

// 2019 and 2031, wide enough that every generated path is inside the span.
constexpr Timestamp kFrom = 1546300800000000000LL;
constexpr Timestamp kTo   = 1924992000000000000LL;

constexpr std::string_view kRow =
    "1704067200000,42000.10,42100.00,41900.00,42050.00,10.5,1704070799999,414423568.46600,97149,"
    "1862.868,196511690.26110,0\n";

/// Delivers straight into the destination, so next() never waits on network.
class InstantPool {
   public:
    explicit InstantPool(const std::string& body) : body_(&body) {}

    bool submit(std::string, FileQueue* destination) {
        const auto* bytes = reinterpret_cast<const std::byte*>(body_->data());
        return destination->push(
            FetchedFile{std::vector<std::byte>(bytes, bytes + body_->size()), FetchStatus::Ok});
    }

   private:
    const std::string* body_;
};

/// Always saturated, so the stream keeps asking and never gets anything.
class RefusingPool {
   public:
    bool submit(std::string, FileQueue*) { return false; }
};

/// Accepts and never delivers, so the stream sits with a fetch outstanding.
/// This is what most of a sweep looks like once the pool is busy.
class SilentPool {
   public:
    bool submit(std::string, FileQueue*) { return true; }
};

/// Splits rows and stamps events but does no field work, isolating the stream
/// from the cost of parsing.
struct NullParser {
    using Event = KlineEvent;

    static constexpr EventKind    event_kind    = EventKind::Kline;
    static constexpr EndpointKind endpoint_kind = EndpointKind::Klines;
    static constexpr bool         intervalled   = true;

    static bool parse(std::string_view row, Timestamp& ts, Event& out) noexcept {
        ts       = static_cast<Timestamp>(row.size());
        out.open = static_cast<double>(row.size());
        return true;
    }
};

std::string make_file(std::size_t rows) {
    std::string out;
    out.reserve(rows * kRow.size());
    for (std::size_t i = 0; i < rows; ++i) out += kRow;
    return out;
}

/// Distinct, well formed daily keys. Twenty eight days a month keeps every one
/// of them valid without a calendar.
std::vector<std::string> make_paths(std::size_t count) {
    std::vector<std::string> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto year  = 2020 + i / (12 * 28);
        const auto month = 1 + (i / 28) % 12;
        const auto day   = 1 + i % 28;

        std::string key = "data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-";
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

template <class P, class Pool>
std::unique_ptr<BinanceHistoricalStream<P, Pool>> make_stream(Pool& pool) {
    return std::make_unique<BinanceHistoricalStream<P, Pool>>(
        pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "1h", kInstrument);
}

/// Builds a fresh stream, drains it, and counts the events. Timing covers the
/// drain only, since construction is per file not per event.
template <class P>
void drain_bench(benchmark::State& state) {
    const auto rows_per_file = static_cast<std::size_t>(state.range(0));
    const auto file_count    = static_cast<std::size_t>(state.range(1));

    const auto body  = make_file(rows_per_file);
    const auto paths = make_paths(file_count);

    std::int64_t events = 0;
    for (auto _ : state) {
        state.PauseTiming();
        InstantPool pool(body);
        auto        stream = make_stream<P>(pool);
        stream->plan(paths, kFrom, kTo);
        state.ResumeTiming();

        MarketEvent event;
        while (stream->next(event)) {
            benchmark::DoNotOptimize(event);
            ++events;
        }
    }
    state.SetItemsProcessed(events);
}

void BM_StreamDrainKlineParser(benchmark::State& state) {
    drain_bench<parsers::KlineParser>(state);
}

void BM_StreamDrainNullParser(benchmark::State& state) { drain_bench<NullParser>(state); }

/// What the source pays sweeping a stream that has nothing to give. With a
/// wide universe this is most of what next() ever does.
void BM_StreamStarvedPoll(benchmark::State& state) {
    RefusingPool pool;
    auto         stream = make_stream<parsers::KlineParser>(pool);
    stream->plan(make_paths(64), kFrom, kTo);

    MarketEvent event;
    for (auto _ : state) {
        benchmark::DoNotOptimize(stream->next(event));
    }
    state.SetItemsProcessed(state.iterations());
}

/// The same sweep once a fetch is already outstanding, which is the normal
/// case. pump() bails before building a url, so this is the cheap path.
void BM_StreamPendingPoll(benchmark::State& state) {
    SilentPool pool;
    auto       stream = make_stream<parsers::KlineParser>(pool);
    stream->plan(make_paths(64), kFrom, kTo);

    MarketEvent event;
    for (auto _ : state) {
        benchmark::DoNotOptimize(stream->next(event));
    }
    state.SetItemsProcessed(state.iterations());
}

// One day of 1m bars against one day of 1h bars, so the file boundary cost
// shows up against a long file and a short one.
BENCHMARK(BM_StreamDrainKlineParser)->Args({1440, 64})->Args({24, 512});
BENCHMARK(BM_StreamDrainNullParser)->Args({1440, 64})->Args({24, 512});
BENCHMARK(BM_StreamStarvedPoll);
BENCHMARK(BM_StreamPendingPoll);

}  // namespace
