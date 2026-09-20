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
using binance::FileSlots;
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

/// Slots plus the two cursors a caller would otherwise keep, so a test reads
/// like one stream running its fetch cycle.
class Reader {
   public:
    FileSlots* slots() noexcept { return &slots_; }

    /// The position for the next submission, handed out in plan order.
    std::size_t next_position() noexcept { return submitted_++; }

    std::size_t submitted() const noexcept { return submitted_; }

    /// Spins until the next position is filled, so a hang fails rather than
    /// blocks forever.
    bool await(binance::FetchedFile& out, std::chrono::seconds timeout = std::chrono::seconds{60}) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto file = slots_.take(read_)) {
                ++read_;
                out = std::move(*file);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return false;
    }

    /// One position if it is ready, without waiting.
    bool try_take(binance::FetchedFile& out) {
        if (auto file = slots_.take(read_)) {
            ++read_;
            out = std::move(*file);
            return true;
        }
        return false;
    }

    /// Takes whatever is ready without waiting, so a drain can be counted.
    std::size_t drain() {
        std::size_t taken = 0;
        while (slots_.take(read_)) {
            ++read_;
            ++taken;
        }
        return taken;
    }

   private:
    FileSlots   slots_;
    std::size_t submitted_{};
    std::size_t read_{};
};

}  // namespace

TEST(HttpFetchPool, FetchesAndInflatesARealFile) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kRealFile), reader.slots(), reader.next_position()));

    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));
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
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kMissingFile), reader.slots(), reader.next_position()));

    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));
    EXPECT_EQ(file.status, FetchStatus::NotFound);
    EXPECT_TRUE(file.body.empty());

    const auto stats = pool.stats();
    EXPECT_EQ(stats.not_found, 1u);
    EXPECT_EQ(stats.completed_failed, 1u);
    EXPECT_EQ(stats.retries, 0u);
}

TEST(HttpFetchPool, FailureIsRecordedWithItsUrl) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kMissingFile), reader.slots(), reader.next_position()));
    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));

    const auto failures = pool.recent_failures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures.front().url, kMissingFile);
    EXPECT_EQ(failures.front().status, FetchStatus::NotFound);
    EXPECT_EQ(failures.front().attempts, 1u);
}

// Counters have to balance, or a lost file would not show up anywhere.
TEST(HttpFetchPool, EverySubmissionReachesATerminalState) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kRealFile), reader.slots(), reader.next_position()));
    ASSERT_TRUE(pool.submit(std::string(kMissingFile), reader.slots(), reader.next_position()));

    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));
    ASSERT_TRUE(reader.await(file));

    const auto stats = pool.stats();
    EXPECT_EQ(stats.submitted, 2u);
    EXPECT_EQ(stats.completed_ok + stats.completed_failed, stats.submitted);
    EXPECT_EQ(stats.in_flight, 0u);
    EXPECT_EQ(stats.queued, 0u);
}

TEST(HttpFetchPool, SubmitIsRefusedAfterQuiesce) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    pool.quiesce();
    EXPECT_FALSE(pool.submit(std::string(kRealFile), reader.slots(), reader.next_position()));
    EXPECT_EQ(pool.stats().workers_alive, 0u);
}

// Stats have to survive the shutdown, otherwise a run cannot be audited.
TEST(HttpFetchPool, QuiesceKeepsStatsReadable) {
    // Declared before the pool, so the pool is destroyed first and its workers
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kRealFile), reader.slots(), reader.next_position()));
    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));

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
    // are joined while these slots are still alive.
    Reader        reader;
    HttpFetchPool pool(small_pool());

    std::atomic<bool> stop{false};
    std::atomic<int>  accepted{0};
    std::atomic<int>  drained{0};

    // One thread submits and reads, because a position may only be reused once
    // its slot has been taken and both cursors belong to the caller.
    std::thread driver([&] {
        std::size_t outstanding = 0;
        while (!stop.load()) {
            if (outstanding < binance::kFileSlotCount) {
                if (pool.submit(std::string(kMissingFile), reader.slots(),
                                reader.next_position())) {
                    ++outstanding;
                    accepted.fetch_add(1);
                }
            }
            const auto taken = reader.drain();
            outstanding -= taken;
            drained.fetch_add(static_cast<int>(taken));
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    pool.quiesce();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    stop.store(true);
    driver.join();

    const auto stats = pool.stats();
    EXPECT_EQ(stats.queued, 0u);
    EXPECT_EQ(stats.completed_ok + stats.completed_failed, stats.submitted);
    EXPECT_EQ(stats.cancellations_dropped, 0u);
    EXPECT_GT(accepted.load(), 0);
}

// A connection cap under the worker count would leave the extra workers
// dialling fresh every time and losing keep alive.
TEST(HttpFetchPool, ConnectionCapKeepsUpWithWorkers) {
    Reader              reader;
    HttpFetchPoolConfig config;
    config.workers                       = 24;
    config.http.max_connections_per_host = 4;

    HttpFetchPool pool(config);
    ASSERT_TRUE(pool.submit(std::string(kRealFile), reader.slots(), reader.next_position()));

    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));
    EXPECT_EQ(file.status, FetchStatus::Ok);
}

// Every worker pops the shared task queue, whose pop is single consumer only.
// Two workers claiming the same slot moved out of it twice and destroyed it
// twice, which only a sanitizer sees. A bad url fails without touching the
// network, so this stays fast while still hammering the claim path.
TEST(HttpFetchPool, ConcurrentWorkersEachClaimATaskExactlyOnce) {
    constexpr std::size_t kTasks = 256;

    std::vector<std::unique_ptr<Reader>> readers;
    readers.reserve(kTasks);
    for (std::size_t i = 0; i < kTasks; ++i) readers.push_back(std::make_unique<Reader>());

    HttpFetchPoolConfig config;
    config.workers     = 16;
    config.max_retries = 0;

    HttpFetchPool pool(config);
    for (std::size_t i = 0; i < kTasks; ++i)
        ASSERT_TRUE(pool.submit("not-a-url-" + std::to_string(i), readers[i]->slots(),
                                readers[i]->next_position()));

    // One delivery per destination, never two and never none.
    for (auto& reader : readers) {
        binance::FetchedFile file;
        ASSERT_TRUE(reader->await(file, std::chrono::seconds{30}));
        EXPECT_EQ(file.status, FetchStatus::TransportError);
        EXPECT_EQ(reader->drain(), 0u);
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

    Reader        reader;
    HttpFetchPool pool(config, [&fired] { fired.fetch_add(1); });

    binance::FetchedFile file;
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(pool.submit(std::string(kMissingFile), reader.slots(), reader.next_position()));
        ASSERT_TRUE(reader.await(file));
    }

    EXPECT_TRUE(pool.stats().critical);
    EXPECT_EQ(fired.load(), 1);
}

TEST(HttpFetchPool, BytesThatAreNotAnArchiveAreAZipError) {
    Reader        reader;
    HttpFetchPool pool(small_pool());

    ASSERT_TRUE(pool.submit(std::string(kNotAnArchive), reader.slots(), reader.next_position()));

    binance::FetchedFile file;
    ASSERT_TRUE(reader.await(file));
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

// Slots are reserved when a task is submitted, so a cancellation always has
// somewhere to land and the shutdown never waits on a reader.
TEST(HttpFetchPool, QueuedTasksAreCancelledIntoTheirOwnSlots) {
    Reader              reader;
    HttpFetchPoolConfig config;
    config.workers     = 1;
    config.max_retries = 0;

    HttpFetchPool pool(config);

    // The one worker is busy on a large file, so the rest are still queued when
    // quiesce runs and are cancelled by the drain rather than by a worker.
    ASSERT_TRUE(pool.submit(std::string(kLargeFile), reader.slots(), reader.next_position()));
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(pool.submit(std::string(kMissingFile), reader.slots(), reader.next_position()));

    const auto started = std::chrono::steady_clock::now();
    pool.quiesce();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    const auto stats = pool.stats();
    EXPECT_EQ(stats.cancellations_dropped, 0u) << "a reserved slot was not free";
    EXPECT_GT(stats.cancelled, 0u);
    EXPECT_EQ(stats.queued, 0u);
    EXPECT_EQ(stats.completed_ok + stats.completed_failed, stats.submitted);

    // Every position was filled, so the reader is never left waiting.
    EXPECT_EQ(reader.drain(), reader.submitted());
    EXPECT_LT(elapsed, std::chrono::seconds{60});
}

TEST(HttpFetchPool, OldestInFlightAgesWhileAFetchRunsAndClearsAfter) {
    HttpFetchPoolConfig config;
    config.workers     = 1;
    config.max_retries = 0;

    Reader        reader;
    HttpFetchPool pool(config);

    EXPECT_EQ(pool.stats().oldest_in_flight_ms, 0);

    ASSERT_TRUE(pool.submit(std::string(kLargeFile), reader.slots(), reader.next_position()));

    std::int64_t         peak = 0;
    binance::FetchedFile file;
    bool                 delivered = false;
    const auto           deadline  = std::chrono::steady_clock::now() + std::chrono::seconds{60};
    while (std::chrono::steady_clock::now() < deadline) {
        const auto stats = pool.stats();
        if (stats.in_flight > 0) peak = std::max(peak, stats.oldest_in_flight_ms);
        if (reader.try_take(file)) {
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
        if (pool.submit(std::string(kLargeFile), nullptr, i)) ++accepted;

    const auto stats = pool.stats();
    EXPECT_GT(stats.refused, 0u) << "queue never filled, capacity may have grown";
    EXPECT_EQ(stats.submitted, accepted);
    EXPECT_EQ(stats.submitted + stats.refused, kSubmissions);

    pool.quiesce();
    EXPECT_EQ(pool.stats().queued, 0u);
}
