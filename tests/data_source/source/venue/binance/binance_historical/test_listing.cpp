#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "listing.hpp"

namespace binance = qp::data_source::source::venue::binance;

using binance::list_children;
using binance::list_keys;
using binance::detail::last_segment;
using binance::detail::parse_listing;

namespace {

// Shapes copied from live bucket responses.
constexpr std::string_view kKeysXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/"><Name>data.binance.vision</Name><Prefix>data/futures/um/daily/klines/BTCUSDT/1h/</Prefix><Marker></Marker><MaxKeys>1000</MaxKeys><IsTruncated>true</IsTruncated><Contents><Key>data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2019-12-31.zip</Key><LastModified>2022-03-21T08:58:02.000Z</LastModified><ETag>&quot;4c65e8f69a3ebae0cebef683f18dff3c&quot;</ETag><Size>1387</Size><StorageClass>STANDARD</StorageClass></Contents><Contents><Key>data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2019-12-31.zip.CHECKSUM</Key><Size>92</Size></Contents><Contents><Key>data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2020-01-01.zip</Key><Size>1370</Size></Contents></ListBucketResult>)";

constexpr std::string_view kPrefixesXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/"><Name>data.binance.vision</Name><Prefix>data/futures/um/daily/klines/BTCUSDT/</Prefix><Marker></Marker><MaxKeys>1000</MaxKeys><Delimiter>/</Delimiter><IsTruncated>false</IsTruncated><CommonPrefixes><Prefix>data/futures/um/daily/klines/BTCUSDT//</Prefix></CommonPrefixes><CommonPrefixes><Prefix>data/futures/um/daily/klines/BTCUSDT/12h/</Prefix></CommonPrefixes><CommonPrefixes><Prefix>data/futures/um/daily/klines/BTCUSDT/1h/</Prefix></CommonPrefixes></ListBucketResult>)";

constexpr std::string_view kTruncatedPrefixesXml =
    R"(<ListBucketResult><Prefix>data/futures/um/daily/klines/</Prefix><IsTruncated>true</IsTruncated><NextMarker>data/futures/um/daily/klines/XVGUSDT/</NextMarker><CommonPrefixes><Prefix>data/futures/um/daily/klines/0GUSDT/</Prefix></CommonPrefixes></ListBucketResult>)";

bool contains(const std::vector<std::string>& v, std::string_view s) {
    return std::ranges::find(v, s) != v.end();
}

}  // namespace

TEST(BinanceListing, ParsesKeys) {
    const auto page = parse_listing(kKeysXml);
    ASSERT_EQ(page.keys.size(), 3u);
    EXPECT_EQ(page.keys.front(),
              "data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2019-12-31.zip");
    EXPECT_TRUE(page.common_prefixes.empty());
}

TEST(BinanceListing, ParsesTruncatedFlag) {
    EXPECT_TRUE(parse_listing(kKeysXml).truncated);
    EXPECT_FALSE(parse_listing(kPrefixesXml).truncated);
}

// The document level Prefix echoes the request and must not be read as a child.
TEST(BinanceListing, EchoedRequestPrefixIsNotAChild) {
    const auto page = parse_listing(kPrefixesXml);
    ASSERT_EQ(page.common_prefixes.size(), 3u);
    EXPECT_FALSE(contains(page.common_prefixes, "data/futures/um/daily/klines/BTCUSDT/"));
}

TEST(BinanceListing, NextMarkerPresentOnlyWithDelimiter) {
    EXPECT_TRUE(parse_listing(kKeysXml).next_marker.empty());
    EXPECT_EQ(parse_listing(kTruncatedPrefixesXml).next_marker,
              "data/futures/um/daily/klines/XVGUSDT/");
}

TEST(BinanceListing, LastSegment) {
    EXPECT_EQ(last_segment("data/futures/um/daily/klines/BTCUSDT/1h/"), "1h");
    EXPECT_EQ(last_segment("data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2020-01-01.zip"),
              "BTCUSDT-1h-2020-01-01.zip");
    EXPECT_EQ(last_segment("data/futures/um/daily/klines/BTCUSDT//"), "");
    EXPECT_EQ(last_segment("BTCUSDT"), "BTCUSDT");
}

TEST(BinanceListingLive, ChildrenOfSymbolAreIntervals) {
    const auto intervals = list_children("data/futures/um/daily/klines/BTCUSDT/");
    EXPECT_TRUE(contains(intervals, "1h"));
    EXPECT_TRUE(contains(intervals, "1m"));
    // The bucket holds an empty directory here.
    EXPECT_FALSE(contains(intervals, ""));
}

// Over a thousand symbols, so this only passes if marker pagination works.
TEST(BinanceListingLive, SymbolListingPagesPastOneThousand) {
    const auto symbols = list_children("data/futures/um/daily/klines/");
    EXPECT_GT(symbols.size(), 1000u);
    EXPECT_TRUE(contains(symbols, "BTCUSDT"));
    EXPECT_TRUE(contains(symbols, "ETHUSDT"));
}

TEST(BinanceListingLive, KeysAreZipsWithNoChecksums) {
    const auto keys = list_keys("data/futures/um/daily/klines/BTCUSDT/1d/");
    ASSERT_FALSE(keys.empty());
    for (const auto& key : keys) {
        SCOPED_TRACE(key);
        EXPECT_TRUE(key.ends_with(".zip"));
    }
}

// Funding is monthly only and every file carries the kind in its name.
TEST(BinanceListingLive, FundingRateKeys) {
    const auto keys = list_keys("data/futures/um/monthly/fundingRate/BTCUSDT/");
    ASSERT_FALSE(keys.empty());
    EXPECT_TRUE(std::ranges::all_of(keys, [](const std::string& k) {
        return k.find("BTCUSDT-fundingRate-") != std::string::npos;
    }));
}
