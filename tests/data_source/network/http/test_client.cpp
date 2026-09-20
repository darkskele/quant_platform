#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "client.hpp"

using qp::data_source::network::http::close_idle_connections;
using qp::data_source::network::http::get;
using qp::data_source::network::http::HttpConfig;
using qp::data_source::network::http::idle_connection_count;

namespace {

/// A few bytes each, so a fetch is dominated by the connection rather than the
/// body. Distinct days, so nothing is served from a cache.
std::string checksum_url(int day) {
    const std::string stamp = (day < 10 ? "0" : "") + std::to_string(day);
    return "https://data.binance.vision/data/futures/um/daily/klines/BTCUSDT/1h/"
           "BTCUSDT-1h-2020-01-" +
           stamp + ".zip.CHECKSUM";
}

}  // namespace

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

// The cache is process wide, so each of these clears it first rather than
// assuming what ran before left it empty.

TEST(HttpConnectionPool, DrainedResponseGoesBackToThePool) {
    close_idle_connections();
    ASSERT_EQ(idle_connection_count(), 0u);

    get(checksum_url(1));
    EXPECT_EQ(idle_connection_count(), 1u);
}

TEST(HttpConnectionPool, SequentialGetsReuseTheOneConnection) {
    close_idle_connections();
    get(checksum_url(2));
    get(checksum_url(3));
    get(checksum_url(4));

    // Released and reacquired each time, so the pool never grows past one.
    EXPECT_EQ(idle_connection_count(), 1u);
}

// A non 2xx response is not returned to the pool, so the socket is dropped
// rather than handed to the next caller mid-stream.
TEST(HttpConnectionPool, FailedRequestDoesNotPoolItsConnection) {
    close_idle_connections();
    EXPECT_THROW(get("https://data.binance.vision/does-not-exist"), std::runtime_error);
    EXPECT_EQ(idle_connection_count(), 0u);
}

TEST(HttpConnectionPool, IdleConnectionsAreCappedPerHost) {
    close_idle_connections();

    constexpr std::size_t kThreads = 8;
    HttpConfig            config;
    config.max_connections_per_host = 2;

    std::vector<std::thread> threads;
    std::atomic<int>         failures{0};
    for (std::size_t i = 0; i < kThreads; ++i)
        threads.emplace_back([i, config, &failures] {
            try {
                get(checksum_url(static_cast<int>(i) + 5), config);
            } catch (const std::exception&) {
                failures.fetch_add(1);
            }
        });
    for (auto& t : threads) t.join();

    ASSERT_EQ(failures.load(), 0);
    // Concurrent gets dial as many sockets as they need, but only the cap is
    // kept afterwards. The rest are closed on release.
    EXPECT_LE(idle_connection_count(), config.max_connections_per_host);
}

TEST(HttpConnectionPool, CloseIdleDropsEveryPooledConnection) {
    close_idle_connections();
    get(checksum_url(1));
    ASSERT_GT(idle_connection_count(), 0u);

    close_idle_connections();
    EXPECT_EQ(idle_connection_count(), 0u);

    // Still usable afterwards, it redials rather than staying broken.
    EXPECT_NO_THROW(get(checksum_url(1)));
}
