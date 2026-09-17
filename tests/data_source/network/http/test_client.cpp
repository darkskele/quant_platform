#include <gtest/gtest.h>

#include <stdexcept>

#include "client.hpp"

using qp::data_source::network::http::get;

// Hits the real endpoint. CHECKSUM file, a few bytes, not a zip.
TEST(HttpGetTest, GetsSmallBinanceVisionFile) {
    auto body =
        get("https://data.binance.vision/data/futures/um/daily/klines/BTCUSDT/1h/"
            "BTCUSDT-1h-2020-01-01.zip.CHECKSUM");

    EXPECT_FALSE(body.empty());
}

TEST(HttpGetTest, RejectsNonHttpsUrl) {
    EXPECT_THROW(get("http://data.binance.vision/"), std::invalid_argument);
}

TEST(HttpGetTest, ThrowsOnNotFound) {
    EXPECT_THROW(get("https://data.binance.vision/does-not-exist"), std::runtime_error);
}
