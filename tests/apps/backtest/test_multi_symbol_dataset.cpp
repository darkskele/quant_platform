#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/compose.hpp"
#include "config/futures_leg.hpp"
#include "config/spot_leg.hpp"
#include "source.hpp"
#include "support/scratch_dir.hpp"
#include "types.hpp"

using qp::test::ScratchDir;
namespace config = qp::backtest::config;

namespace {

std::string kline_line(std::string_view symbol, std::int64_t ts_ms, double close = 100.0) {
    return std::string(symbol) + ",K," + std::to_string(ts_ms) + ",100.0,101.0,99.0," +
           std::to_string(close) + ",1.0," + std::to_string(ts_ms + 59'999);
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

// Backtest fixed date: one daily file per symbol, one monthly funding file.
constexpr auto kDay = std::chrono::year_month_day{std::chrono::year{2024}, std::chrono::month{1},
                                                  std::chrono::day{1}};

// klines under the symbol's futures dir, plus the empty funding file the
// futures leg always opens (monthly_funding_files does not probe disk).
void write_futures(const std::filesystem::path& root, std::string_view symbol,
                   const std::vector<std::int64_t>& ts_ms) {
    std::string body;
    for (auto t : ts_ms) body += kline_line(symbol, t) + "\n";
    write_file(root / symbol / "futures" / "klines" / (std::string(symbol) + "-1m-2024-01-01.csv"),
               body);
    write_file(
        root / symbol / "futures" / "funding" / (std::string(symbol) + "-fundingRate-2024-01.csv"),
        "");
}

void write_spot(const std::filesystem::path& root, std::string_view symbol,
                const std::vector<std::int64_t>& ts_ms) {
    std::string body;
    for (auto t : ts_ms) body += kline_line(symbol, t) + "\n";
    write_file(root / symbol / "spot" / "klines" / (std::string(symbol) + "-1m-2024-01-01.csv"),
               body);
}

template <class Source>
std::vector<qp::MarketEvent> drain(Source& source) {
    std::vector<qp::MarketEvent> out;
    for (;;) {
        auto r = source.next();
        if (r) {
            out.push_back(std::move(*r));
            continue;
        }
        if (r.error() == qp::data_source::source::SourceStatus::Eof) return out;
    }
}

std::map<qp::SymbolId, std::vector<std::int64_t>> by_symbol_ms(
    const std::vector<qp::MarketEvent>& events) {
    std::map<qp::SymbolId, std::vector<std::int64_t>> out;
    for (const auto& e : events)
        out[qp::header_of(e).symbol].push_back(qp::header_of(e).ts / 1'000'000);
    return out;
}

}  // namespace

TEST(MultiSymbolDataset, FuturesLegMergesSymbolsByTimestampKeepingIdentity) {
    ScratchDir               dir;
    std::vector<std::string> symbols{"BTCUSDT", "ETHUSDT"};
    write_futures(dir.path, "BTCUSDT", {0, 120'000});
    write_futures(dir.path, "ETHUSDT", {60'000, 180'000});

    auto source = config::make_futures_source(dir.path, symbols, kDay, kDay);
    auto events = drain(source);

    ASSERT_EQ(events.size(), 4u);
    for (std::size_t i = 1; i < events.size(); ++i)
        EXPECT_GE(qp::header_of(events[i]).ts, qp::header_of(events[i - 1]).ts);

    auto btc = config::FuturesTable::id_of("BTCUSDT");
    auto eth = config::FuturesTable::id_of("ETHUSDT");
    ASSERT_TRUE(btc && eth);
    auto grouped = by_symbol_ms(events);
    EXPECT_EQ(grouped[*btc], (std::vector<std::int64_t>{0, 120'000}));
    EXPECT_EQ(grouped[*eth], (std::vector<std::int64_t>{60'000, 180'000}));
}

TEST(MultiSymbolDataset, SpotLegMergesSymbolsByTimestampKeepingIdentity) {
    ScratchDir               dir;
    std::vector<std::string> symbols{"BTCUSDT", "ETHUSDT"};
    write_spot(dir.path, "BTCUSDT", {30'000});
    write_spot(dir.path, "ETHUSDT", {90'000});

    auto source = config::make_spot_source(dir.path, symbols, kDay, kDay);
    auto events = drain(source);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_LE(qp::header_of(events[0]).ts, qp::header_of(events[1]).ts);

    auto btc = config::FuturesTable::id_of("BTCUSDT");
    auto eth = config::FuturesTable::id_of("ETHUSDT");
    ASSERT_TRUE(btc && eth);
    auto grouped = by_symbol_ms(events);
    EXPECT_EQ(grouped[*btc], (std::vector<std::int64_t>{30'000}));
    EXPECT_EQ(grouped[*eth], (std::vector<std::int64_t>{90'000}));
}

// The single-symbol overload still composes, so existing callers are intact.
TEST(MultiSymbolDataset, SingleSymbolOverloadStillComposes) {
    ScratchDir dir;
    write_futures(dir.path, "BTCUSDT", {0, 60'000});

    auto source = config::make_futures_source(dir.path, "BTCUSDT", kDay, kDay);
    auto events = drain(source);

    ASSERT_EQ(events.size(), 2u);
    auto btc = config::FuturesTable::id_of("BTCUSDT");
    ASSERT_TRUE(btc);
    EXPECT_EQ(qp::header_of(events[0]).symbol, *btc);
}
