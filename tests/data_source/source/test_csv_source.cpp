#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "bin_hist_venue.hpp"
#include "csv_source.hpp"
#include "support/scratch_dir.hpp"

using qp::data_source::source::CsvSource;
using qp::data_source::source::venue::binance::binance_historical::BinHistVenue;
using qp::test::ScratchDir;

namespace {

std::string kline_line(std::int64_t open_time_ms, double close = 100.0) {
    return "BTCUSDT,K," + std::to_string(open_time_ms) + ",100.0,101.0,99.0," +
           std::to_string(close) + ",1.0," + std::to_string(open_time_ms + 59'999);
}

std::string funding_line(std::int64_t calc_time_ms, double rate = 0.0001) {
    return "BTCUSDT,F," + std::to_string(calc_time_ms) + ",8," + std::to_string(rate);
}

std::filesystem::path write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << content;
    return path;
}

// next() returns nullopt for two different.
template <class Source>
std::optional<qp::MarketEvent> next_blocking(Source& source) {
    for (;;) {
        if (auto ev = source.next()) return ev;
        if (source.is_done()) return std::nullopt;
    }
}

}  // namespace

TEST(CsvSource, SingleStreamReturnsEventsInFileOrder) {
    ScratchDir dir;
    auto path = write_file(dir.path / "a.csv", kline_line(0) + "\n" + kline_line(60'000) + "\n" +
                                                   kline_line(120'000) + "\n");

    std::array<std::vector<std::filesystem::path>, 1> streams{{{path}}};
    CsvSource<BinHistVenue, 1>                        source(streams);

    for (std::int64_t expected : {0, 60'000, 120'000}) {
        auto ev = next_blocking(source);
        ASSERT_TRUE(ev.has_value());
        EXPECT_EQ(qp::header_of(*ev).ts, expected * 1'000'000);  // ms -> ns
    }
    EXPECT_FALSE(next_blocking(source).has_value());
    EXPECT_TRUE(source.is_done());
}

TEST(CsvSource, MergesTwoStreamsByTimestampNotStreamOrder) {
    ScratchDir dir;
    // Stream 0: klines every 60s. Stream 1: funding, offset so it lands
    // strictly between two klines — a genuine merge, not two runs
    // concatenated.
    auto klines =
        write_file(dir.path / "klines.csv", kline_line(0) + "\n" + kline_line(60'000) + "\n");
    auto funding = write_file(dir.path / "funding.csv", funding_line(30'000) + "\n");

    std::array<std::vector<std::filesystem::path>, 2> streams{{{klines}, {funding}}};
    CsvSource<BinHistVenue, 2>                        source(streams);

    for (std::int64_t expected : {0, 30'000, 60'000}) {
        auto ev = next_blocking(source);
        ASSERT_TRUE(ev.has_value());
        EXPECT_EQ(qp::header_of(*ev).ts, expected * 1'000'000);
    }
    EXPECT_FALSE(next_blocking(source).has_value());
}

TEST(CsvSource, ContinuesAcrossMultipleFilesInOneStream) {
    ScratchDir dir;
    auto file0 = write_file(dir.path / "0.csv", kline_line(0) + "\n" + kline_line(60'000) + "\n");
    auto file1 = write_file(dir.path / "1.csv", kline_line(120'000) + "\n");

    std::array<std::vector<std::filesystem::path>, 1> streams{{{file0, file1}}};
    CsvSource<BinHistVenue, 1>                        source(streams);

    for (std::int64_t expected : {0, 60'000, 120'000}) {
        auto ev = next_blocking(source);
        ASSERT_TRUE(ev.has_value());
        EXPECT_EQ(qp::header_of(*ev).ts, expected * 1'000'000);
    }
    EXPECT_FALSE(next_blocking(source).has_value());
}

TEST(CsvSource, SkipsUnparseableAndBlankLinesWithoutFailing) {
    ScratchDir dir;
    auto path = write_file(dir.path / "a.csv", kline_line(0) + "\n" + "not,a,real,line\n" + "\n" +
                                                   "NOTASYMBOL,K,1,2,3,4,5,6,7\n" +
                                                   kline_line(60'000) + "\n");

    std::array<std::vector<std::filesystem::path>, 1> streams{{{path}}};
    CsvSource<BinHistVenue, 1>                        source(streams);

    for (std::int64_t expected : {0, 60'000}) {
        auto ev = next_blocking(source);
        ASSERT_TRUE(ev.has_value());
        EXPECT_EQ(qp::header_of(*ev).ts, expected * 1'000'000);
    }
    EXPECT_FALSE(next_blocking(source).has_value());
}

TEST(CsvSource, EmptyFileProducesNoEventsAndStillReportsDone) {
    ScratchDir dir;
    auto       empty  = write_file(dir.path / "empty.csv", "");
    auto       klines = write_file(dir.path / "klines.csv", kline_line(0) + "\n");

    std::array<std::vector<std::filesystem::path>, 2> streams{{{empty}, {klines}}};
    CsvSource<BinHistVenue, 2>                        source(streams);

    auto ev = next_blocking(source);
    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(qp::header_of(*ev).ts, 0);
    EXPECT_FALSE(next_blocking(source).has_value());
    EXPECT_TRUE(source.is_done());
}

TEST(CsvSource, ThrowsIfAFileCannotBeOpened) {
    ScratchDir dir;
    auto       missing = dir.path / "does_not_exist.csv";

    std::array<std::vector<std::filesystem::path>, 1> streams{{{missing}}};
    CsvSource<BinHistVenue, 1>                        source(streams);

    bool threw = false;
    for (int i = 0; i < 1'000'000 && !threw; ++i) {
        try {
            source.next();
        } catch (const std::runtime_error& e) {
            threw = true;
            EXPECT_NE(std::string(e.what()).find("does_not_exist.csv"), std::string::npos);
        }
    }
    EXPECT_TRUE(threw);
}
