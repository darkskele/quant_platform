#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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

/// Answers 200 with html rather than an archive, which is what the zip path
/// has to reject.
constexpr std::string_view kNotAnArchive = "https://data.binance.vision/";

/// A monthly minute file, megabytes rather than kilobytes, so a fetch stays in
/// flight long enough to be observed.
constexpr std::string_view kLargeFile =
    "https://data.binance.vision/data/futures/um/monthly/klines/BTCUSDT/1m/"
    "BTCUSDT-1m-2025-01.zip";

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

// Every worker pops the shared task queue, whose pop is single consumer only.
// Two workers claiming the same slot moved out of it twice and destroyed it
// twice, which only a sanitizer sees. A bad url fails without touching the
// network, so this stays fast while still hammering the claim path.
TEST(HttpFetchPool, ConcurrentWorkersEachClaimATaskExactlyOnce) {
    constexpr std::size_t kTasks = 256;

    std::vector<std::unique_ptr<FileQueue>> queues;
    queues.reserve(kTasks);
    for (std::size_t i = 0; i < kTasks; ++i) queues.push_back(std::make_unique<FileQueue>());

    HttpFetchPoolConfig config;
    config.workers     = 16;
    config.max_retries = 0;

    HttpFetchPool pool(config);
    for (std::size_t i = 0; i < kTasks; ++i)
        ASSERT_TRUE(pool.submit("not-a-url-" + std::to_string(i), queues[i].get()));

    // One delivery per destination, never two and never none.
    for (auto& queue : queues) {
        binance::FetchedFile file;
        ASSERT_TRUE(await(*queue, file, std::chrono::seconds{30}));
        EXPECT_EQ(file.status, FetchStatus::TransportError);
        EXPECT_FALSE(queue->pop().has_value());
    }

    const auto stats = pool.stats();
    EXPECT_EQ(stats.submitted, kTasks);
    EXPECT_EQ(stats.completed_ok + stats.completed_failed, kTasks);
    EXPECT_EQ(stats.queued, 0u);
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

TEST(HttpFetchPool, BytesThatAreNotAnArchiveAreAZipError) {
    FileQueue     queue;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kNotAnArchive), &queue));

    binance::FetchedFile file;
    ASSERT_TRUE(await(queue, file));
    EXPECT_EQ(file.status, FetchStatus::ZipError);
    EXPECT_TRUE(file.body.empty());

    const auto stats = pool.stats();
    EXPECT_EQ(stats.zip_error, 1u);
    EXPECT_EQ(stats.completed_failed, 1u);
    // The transfer itself worked, so the fetched bytes are counted and none of
    // them reach the inflated total.
    EXPECT_GT(stats.bytes_fetched, 0u);
    EXPECT_EQ(stats.bytes_inflated, 0u);

    const auto failures = pool.recent_failures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures.front().status, FetchStatus::ZipError);
}

TEST(HttpFetchPool, CancellationsIntoAFullRingAreCountedNotWaitedOn) {
    // Three usable slots, all taken, so nothing the drain pushes can land.
    FileQueue abandoned;
    for (int i = 0; i < 3; ++i) ASSERT_TRUE(abandoned.push(binance::FetchedFile{}));
    ASSERT_FALSE(abandoned.push(binance::FetchedFile{}));

    HttpFetchPoolConfig config;
    config.workers     = 1;
    config.max_retries = 0;

    FileQueue     busy;
    HttpFetchPool pool(config);

    // Occupies the one worker, so the rest are still queued when quiesce runs
    // and are cancelled by the drain rather than by a worker.
    ASSERT_TRUE(pool.submit(std::string(kLargeFile), &busy));
    for (int i = 0; i < 3; ++i) ASSERT_TRUE(pool.submit(std::string(kMissingFile), &abandoned));

    const auto started = std::chrono::steady_clock::now();
    pool.quiesce();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    const auto stats = pool.stats();
    EXPECT_GE(stats.cancellations_dropped, 1u);
    EXPECT_GE(stats.cancelled, stats.cancellations_dropped);
    EXPECT_EQ(stats.queued, 0u);
    // Bounded attempts, so an undrained stream delays the shutdown rather than
    // hanging it.
    EXPECT_LT(elapsed, std::chrono::seconds{60});
}

TEST(HttpFetchPool, OldestInFlightAgesWhileAFetchRunsAndClearsAfter) {
    HttpFetchPoolConfig config;
    config.workers     = 1;
    config.max_retries = 0;

    FileQueue     queue;
    HttpFetchPool pool(config);

    EXPECT_EQ(pool.stats().oldest_in_flight_ms, 0);

    ASSERT_TRUE(pool.submit(std::string(kLargeFile), &queue));

    std::int64_t         peak = 0;
    binance::FetchedFile file;
    bool                 delivered = false;
    const auto           deadline  = std::chrono::steady_clock::now() + std::chrono::seconds{60};
    while (std::chrono::steady_clock::now() < deadline) {
        const auto stats = pool.stats();
        if (stats.in_flight > 0) peak = std::max(peak, stats.oldest_in_flight_ms);
        if (auto popped = queue.pop()) {
            file      = std::move(*popped);
            delivered = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    ASSERT_TRUE(delivered);
    EXPECT_EQ(file.status, FetchStatus::Ok);
    EXPECT_GT(peak, 0) << "never observed an in flight fetch aging";
    // Cleared before the file is handed over, so a settled pool reports no
    // stalled worker.
    EXPECT_EQ(pool.stats().oldest_in_flight_ms, 0);
    EXPECT_EQ(pool.stats().in_flight, 0u);
}

TEST(HttpFetchPool, SubmitIsRefusedOnceTheTaskQueueFills) {
    HttpFetchPoolConfig config;
    config.workers     = 1;
    config.max_retries = 0;

    HttpFetchPool pool(config);

    // No destination, so the drain discards these without touching a ring, and
    // the one worker stays busy on a file big enough that it drains nothing.
    constexpr std::size_t kSubmissions = 3000;
    std::size_t           accepted     = 0;
    for (std::size_t i = 0; i < kSubmissions; ++i)
        if (pool.submit(std::string(kLargeFile), nullptr)) ++accepted;

    const auto stats = pool.stats();
    EXPECT_GT(stats.refused, 0u) << "queue never filled, capacity may have grown";
    EXPECT_EQ(stats.submitted, accepted);
    EXPECT_EQ(stats.submitted + stats.refused, kSubmissions);

    pool.quiesce();
    EXPECT_EQ(pool.stats().queued, 0u);
}
