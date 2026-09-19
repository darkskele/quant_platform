#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "endpoints.hpp"
#include "http_fetch_pool.hpp"

namespace binance = qp::data_source::source::venue::binance;

using binance::FetchStatus;
using binance::FileQueue;
using binance::HttpFetchPool;
using binance::HttpFetchPoolConfig;

namespace {

constexpr std::string_view kRealFile =
    "https://data.binance.vision/data/futures/um/daily/klines/BTCUSDT/1h/"
    "BTCUSDT-1h-2025-06-02.zip";

constexpr std::string_view kMissingFile =
    "https://data.binance.vision/data/futures/um/daily/klines/BTCUSDT/1h/"
    "BTCUSDT-1h-1999-01-01.zip";

HttpFetchPoolConfig small_pool() {
    HttpFetchPoolConfig config;
    config.workers     = 2;
    config.max_retries = 0;
    return config;
}

/// Spins until the queue yields, so a hang fails rather than blocks forever.
bool await(FileQueue& queue, binance::FetchedFile& out,
           std::chrono::seconds timeout = std::chrono::seconds{60}) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto file = queue.pop()) {
            out = std::move(*file);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return false;
}

}  // namespace

TEST(HttpFetchPool, FetchesAndInflatesARealFile) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kRealFile), &queue));

    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));
    EXPECT_EQ(file.status, FetchStatus::Ok);
    EXPECT_FALSE(file.body.empty());

    const auto stats = pool.stats();
    EXPECT_EQ(stats.completed_ok, 1u);
    EXPECT_EQ(stats.completed_failed, 0u);
    EXPECT_GT(stats.bytes_fetched, 0u);
    EXPECT_GT(stats.bytes_inflated, stats.bytes_fetched);
}

// A 404 means the listing and the bucket disagree, so it is its own status and
// never retried.
TEST(HttpFetchPool, MissingFileIsNotFoundAndStillDelivered) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kMissingFile), &queue));

    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));
    EXPECT_EQ(file.status, FetchStatus::NotFound);
    EXPECT_TRUE(file.body.empty());

    const auto stats = pool.stats();
    EXPECT_EQ(stats.not_found, 1u);
    EXPECT_EQ(stats.completed_failed, 1u);
    EXPECT_EQ(stats.retries, 0u);
}

TEST(HttpFetchPool, FailureIsRecordedWithItsUrl) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kMissingFile), &queue));
    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));

    const auto failures = pool.recent_failures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures.front().url, kMissingFile);
    EXPECT_EQ(failures.front().status, FetchStatus::NotFound);
    EXPECT_EQ(failures.front().attempts, 1u);
}

// Counters have to balance, or a lost file would not show up anywhere.
TEST(HttpFetchPool, EverySubmissionReachesATerminalState) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kRealFile), &queue));
    ASSERT_TRUE(pool.submit(std::string(kMissingFile), &queue));

    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));
    ASSERT_TRUE(await(queue, file));

    const auto stats = pool.stats();
    EXPECT_EQ(stats.submitted, 2u);
    EXPECT_EQ(stats.completed_ok + stats.completed_failed, stats.submitted);
    EXPECT_EQ(stats.in_flight, 0u);
    EXPECT_EQ(stats.queued, 0u);
}

TEST(HttpFetchPool, SubmitIsRefusedAfterQuiesce) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    pool.quiesce();
    EXPECT_FALSE(pool.submit(std::string(kRealFile), &queue));
    EXPECT_EQ(pool.stats().workers_alive, 0u);
}

// Stats have to survive the shutdown, otherwise a run cannot be audited.
TEST(HttpFetchPool, QuiesceKeepsStatsReadable) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kRealFile), &queue));
    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));

    pool.quiesce();
    EXPECT_EQ(pool.stats().completed_ok, 1u);
    EXPECT_GT(pool.stats().bytes_inflated, 0u);
}

TEST(HttpFetchPool, QuiesceIsIdempotent) {
    HttpFetchPool pool(small_pool());
    pool.quiesce();
    pool.quiesce();
    SUCCEED();
}

// A second caller must wait for the first to finish joining, not return while
// workers are still running and the object is being torn down.
TEST(HttpFetchPool, ConcurrentQuiesceBothWaitForWorkers) {
    auto pool = std::make_unique<HttpFetchPool>(small_pool());

    std::thread other([&pool] { pool->quiesce(); });
    pool->quiesce();
    other.join();

    EXPECT_EQ(pool->stats().workers_alive, 0u);
    pool.reset();
    SUCCEED();
}

// A submit racing the drain must not leave a task queued with nobody to run it.
TEST(HttpFetchPool, NoTaskSurvivesQuiesce) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while this queue is still alive.
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    std::atomic<bool> stop{false};
    std::atomic<int>  accepted{0};
    std::atomic<int>  drained{0};

    std::thread submitter([&] {
        while (!stop.load()) {
            if (pool.submit(std::string(kMissingFile), &queue)) accepted.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    });
    // The ring is small, so without a reader it fills and the pool is measuring
    // a stalled stream rather than the shutdown race.
    std::thread reader([&] {
        while (!stop.load()) {
            if (queue.pop()) drained.fetch_add(1);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    pool.quiesce();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    stop.store(true);
    submitter.join();
    reader.join();

    const auto stats = pool.stats();
    EXPECT_EQ(stats.queued, 0u);
    EXPECT_EQ(stats.completed_ok + stats.completed_failed, stats.submitted);
    EXPECT_EQ(static_cast<std::size_t>(accepted.load()), stats.submitted);
    EXPECT_EQ(stats.cancellations_dropped, 0u);
}

// Worker count is the concurrency ceiling, so it has to be honoured.
TEST(HttpFetchPool, WorkerCountIsConfigurable) {
    HttpFetchPoolConfig config;
    config.workers = 5;

    HttpFetchPool pool(config);
    EXPECT_EQ(pool.stats().workers_alive, 5u);
    pool.quiesce();
    EXPECT_EQ(pool.stats().workers_alive, 0u);
}

// A connection cap under the worker count would leave the extra workers
// dialling fresh every time and losing keep alive.
TEST(HttpFetchPool, ConnectionCapKeepsUpWithWorkers) {
    FileQueue           queue;
    HttpFetchPoolConfig config;
    config.workers                       = 24;
    config.http.max_connections_per_host = 4;

    HttpFetchPool pool(config);
    ASSERT_TRUE(pool.submit(std::string(kRealFile), &queue));

    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));
    EXPECT_EQ(file.status, FetchStatus::Ok);
}

// A window of failures raises the alarm once, not per failure.
TEST(HttpFetchPool, CriticalFiresOnceOnASustainedFailureRate) {
    std::atomic<int>    fired{0};
    HttpFetchPoolConfig config;
    config.workers                = 2;
    config.max_retries            = 0;
    config.critical_window        = 4;
    config.critical_failure_ratio = 0.5;

    FileQueue     queue;
    HttpFetchPool pool(config, [&fired] { fired.fetch_add(1); });

    binance::FetchedFile file;
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(pool.submit(std::string(kMissingFile), &queue));
        ASSERT_TRUE(await(queue, file));
    }

    EXPECT_TRUE(pool.stats().critical);
    EXPECT_EQ(fired.load(), 1);
}
