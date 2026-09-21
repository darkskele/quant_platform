#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "book_depth.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::parsers::BookDepthParser;
using binance::parsers::parse_book_depth_group;
using qp::BookDepthBands;
using qp::BookDepthEvent;
using qp::Timestamp;

namespace {

// A real sample, BTCUSDT on 2025-06-02 at 00:00:10 UTC, in bucket order.
constexpr std::array<std::string_view, 10> kSample = {
    "2025-06-02 00:00:10,-5,7708.55000000,795335825.75690000",
    "2025-06-02 00:00:10,-4,6826.68800000,706388757.23630000",
    "2025-06-02 00:00:10,-3,5082.39800000,528538384.14710000",
    "2025-06-02 00:00:10,-2,3236.90700000,338777356.57760000",
    "2025-06-02 00:00:10,-1,1846.19200000,194035262.14000000",
    "2025-06-02 00:00:10,1,2747.22900000,291449666.44800000",
    "2025-06-02 00:00:10,2,4318.91700000,459767333.40140000",
    "2025-06-02 00:00:10,3,4985.88500000,531882812.33460000",
    "2025-06-02 00:00:10,4,6152.93400000,659291989.76400000",
    "2025-06-02 00:00:10,5,6701.42700000,719804265.48500000",
};

constexpr Timestamp kStamp = 1748822410LL * 1'000'000'000LL;

std::vector<std::string_view> sample() { return {kSample.begin(), kSample.end()}; }

/// The rows as the stream hands them over, one block with newlines between.
std::string block_of(const std::vector<std::string_view>& rows) {
    std::string out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) out += '\n';
        out += rows[i];
    }
    return out;
}

bool parse(const std::vector<std::string_view>& rows, Timestamp& ts, BookDepthBands& out) {
    return parse_book_depth_group(block_of(rows), ts, out);
}

}  // namespace

TEST(BinanceBookDepth, ParsesRealSampleIntoBands) {
    Timestamp      ts{};
    BookDepthBands out{};
    ASSERT_TRUE(parse(sample(), ts, out));

    EXPECT_EQ(ts, kStamp);
    // Band k is k + 1 percent from mid, so -1 is bids[0] and -5 is bids[4].
    EXPECT_DOUBLE_EQ(out.bids[0].depth, 1846.192);
    EXPECT_DOUBLE_EQ(out.bids[0].notional, 194035262.14);
    EXPECT_DOUBLE_EQ(out.bids[4].depth, 7708.55);
    EXPECT_DOUBLE_EQ(out.bids[4].notional, 795335825.7569);
    EXPECT_DOUBLE_EQ(out.asks[0].depth, 2747.229);
    EXPECT_DOUBLE_EQ(out.asks[4].depth, 6701.427);
    EXPECT_DOUBLE_EQ(out.asks[4].notional, 719804265.485);
}

// Depth is cumulative from mid, which is what makes a band ladder readable.
TEST(BinanceBookDepth, BandsWidenOutwardOnBothSides) {
    Timestamp      ts{};
    BookDepthBands out{};
    ASSERT_TRUE(parse(sample(), ts, out));
    for (std::size_t k = 1; k < out.bids.size(); ++k) {
        EXPECT_GT(out.bids[k].depth, out.bids[k - 1].depth) << "bid band " << k;
        EXPECT_GT(out.asks[k].depth, out.asks[k - 1].depth) << "ask band " << k;
    }
}

TEST(BinanceBookDepth, RowOrderDoesNotMatter) {
    auto rows = sample();
    std::swap(rows[0], rows[9]);
    std::swap(rows[3], rows[6]);
    Timestamp      ts{};
    BookDepthBands out{};
    ASSERT_TRUE(parse(rows, ts, out));
    EXPECT_DOUBLE_EQ(out.bids[4].depth, 7708.55);
    EXPECT_DOUBLE_EQ(out.asks[4].depth, 6701.427);
}

TEST(BinanceBookDepth, RejectsShortGroup) {
    auto rows = sample();
    rows.pop_back();
    Timestamp      ts{};
    BookDepthBands out{};
    EXPECT_FALSE(parse(rows, ts, out));
}

TEST(BinanceBookDepth, RejectsDuplicateBand) {
    auto rows = sample();
    rows[9]   = "2025-06-02 00:00:10,4,6152.93400000,659291989.76400000";
    Timestamp      ts{};
    BookDepthBands out{};
    EXPECT_FALSE(parse(rows, ts, out));
}

TEST(BinanceBookDepth, RejectsOutOfRangeBands) {
    for (std::string_view bad : {"2025-06-02 00:00:10,0,1.0,1.0", "2025-06-02 00:00:10,6,1.0,1.0",
                                 "2025-06-02 00:00:10,-6,1.0,1.0"}) {
        auto rows = sample();
        rows[9]   = bad;
        Timestamp      ts{};
        BookDepthBands out{};
        EXPECT_FALSE(parse(rows, ts, out)) << bad;
    }
}

TEST(BinanceBookDepth, RejectsMixedStamps) {
    auto rows = sample();
    rows[9]   = "2025-06-02 00:00:11,5,6701.42700000,719804265.48500000";
    Timestamp      ts{};
    BookDepthBands out{};
    EXPECT_FALSE(parse(rows, ts, out));
}

TEST(BinanceBookDepth, RejectsTrailingColumn) {
    auto rows = sample();
    rows[9]   = "2025-06-02 00:00:10,5,6701.42700000,719804265.48500000,extra";
    Timestamp      ts{};
    BookDepthBands out{};
    EXPECT_FALSE(parse(rows, ts, out));
}

TEST(BinanceBookDepth, FailureLeavesOutputUntouched) {
    auto rows = sample();
    rows.pop_back();
    Timestamp      ts = 42;
    BookDepthBands out{};
    out.bids[0].depth = 7.0;
    EXPECT_FALSE(parse(rows, ts, out));
    EXPECT_EQ(ts, 42);
    EXPECT_DOUBLE_EQ(out.bids[0].depth, 7.0);
}

TEST(BinanceBookDepth, RejectsBlankLineInsideBlock) {
    auto rows = sample();
    rows.insert(rows.begin() + 5, "");
    Timestamp      ts{};
    BookDepthBands out{};
    EXPECT_FALSE(parse(rows, ts, out));
}

TEST(BinanceBookDepth, AcceptsCrlfBetweenRows) {
    std::string crlf;
    for (const auto row : kSample) {
        if (!crlf.empty()) crlf += "\r\n";
        crlf += row;
    }
    Timestamp      ts{};
    BookDepthBands out{};
    ASSERT_TRUE(parse_book_depth_group(crlf, ts, out));
    EXPECT_DOUBLE_EQ(out.asks[4].notional, 719804265.485);
}

TEST(BinanceBookDepth, ParserFillsSharedBands) {
    const auto     block = block_of(sample());
    Timestamp      ts{};
    BookDepthEvent out{};
    const auto&    entry =
        binance::endpoint(binance::BinanceMarket::UsdM, binance::EndpointKind::BookDepth);
    ASSERT_TRUE(BookDepthParser::parse(entry, block, ts, out));
    ASSERT_NE(out.bands, nullptr);
    EXPECT_DOUBLE_EQ(out.bands->bids[4].depth, 7708.55);
}
