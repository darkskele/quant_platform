#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "client.hpp"
#include "fetch_pool.hpp"
#include "work_queue.hpp"

namespace qp::data_source::source::venue::binance {

struct HttpFetchPoolConfig {
    /// Concurrency ceiling. Blocking calls, so this is how many fetches can be
    /// in flight at once.
    std::size_t workers = 8;

    std::size_t               max_retries = 3;
    std::chrono::milliseconds backoff_base{200};

    /// Idle connections kept per host. Raised to the worker count when it is
    /// lower, or the workers past the cap lose keep alive.
    network::http::HttpConfig http{};

    /// A completed window with at least this failure share fires the critical
    /// callback. Evaluated per window, so a single bad file cannot trip it.
    double      critical_failure_ratio = 0.25;
    std::size_t critical_window        = 64;
};

/// A snapshot. Counters are read off worker threads, so they never tear but a
/// set of them is not a single instant.
struct FetchPoolStats {
    std::size_t submitted{};
    std::size_t refused{};
    std::size_t completed_ok{};
    std::size_t completed_failed{};
    std::size_t retries{};

    std::size_t not_found{};
    std::size_t server_error{};
    std::size_t transport_error{};
    std::size_t zip_error{};
    std::size_t cancelled{};

    /// Cancellations that could not be pushed because the stream's ring stayed
    /// full. Nonzero means a stream was abandoned mid run.
    std::size_t cancellations_dropped{};

    std::size_t queued{};
    std::size_t in_flight{};
    std::size_t workers_alive{};

    std::uint64_t bytes_fetched{};
    std::uint64_t bytes_inflated{};

    /// How long the longest running fetch has been going. The tell for a worker
    /// parked on a dead socket.
    std::int64_t oldest_in_flight_ms{};

    bool critical{};
};

/// One recorded failure, kept in a small ring so a count can be traced back to
/// the file that caused it.
struct FetchFailure {
    std::string url;
    std::string detail;
    FetchStatus status{};
    std::size_t attempts{};
};

/// Fetch workers over the shared http client. Each task is one GET plus one
/// unzip, delivered into the stream's queue whether it worked or not.
class HttpFetchPool {
   public:
    /// on_critical fires at most once, off a worker thread. Wire it to a
    /// control channel's request_stop.
    explicit HttpFetchPool(HttpFetchPoolConfig config = {}, std::function<void()> on_critical = {});

    HttpFetchPool(const HttpFetchPool&)            = delete;
    HttpFetchPool& operator=(const HttpFetchPool&) = delete;

    ~HttpFetchPool();

    /// False when stopping or when the task queue is full.
    bool submit(std::string url, FileQueue* destination);

    /// Stops accepting, fails every task still queued or in flight so no stream
    /// is left waiting, joins the workers and drops the connections. The object
    /// stays alive and its stats stay readable.
    void quiesce();

    FetchPoolStats            stats() const;
    std::vector<FetchFailure> recent_failures() const;

   private:
    static constexpr std::size_t kTaskCapacity    = 2048;
    static constexpr std::size_t kFailureRingSize = 32;

    /// Bounded, so a stream nobody drains cannot hang the shutdown.
    static constexpr std::size_t kCancelPushAttempts = 200;

    struct Task {
        std::string url;
        FileQueue*  destination{};
    };

    void worker_loop(std::size_t index);
    bool take_task(Task& out);
    void run_task(Task& task, std::size_t index);
    void deliver(FileQueue* destination, FetchedFile file);
    void record_failure(const Task& task, FetchStatus status, const std::string& detail,
                        std::size_t attempts);
    void note_completion(bool failed);

    HttpFetchPoolConfig   config_;
    std::function<void()> on_critical_;

    WorkQueue<Task, kTaskCapacity, /*UseHeap=*/true> tasks_;
    std::vector<std::thread>                         workers_;

    mutable std::mutex      wake_mutex_;
    std::condition_variable wake_;
    std::atomic<bool>       stopping_{false};
    std::atomic<bool>       critical_{false};

    /// Held for the whole shutdown, so a second caller waits for the first to
    /// finish rather than returning while workers still run.
    std::mutex shutdown_mutex_;
    bool       quiesced_{false};

    /// Submits part way through. quiesce waits for these before draining, so no
    /// task can be pushed after the last one is cancelled.
    std::atomic<std::size_t> submits_active_{0};

    std::atomic<std::size_t> sleepers_{0};
    std::atomic<std::size_t> queued_{0};
    std::atomic<std::size_t> in_flight_{0};
    std::atomic<std::size_t> workers_alive_{0};

    std::atomic<std::size_t>   submitted_{0};
    std::atomic<std::size_t>   refused_{0};
    std::atomic<std::size_t>   completed_ok_{0};
    std::atomic<std::size_t>   completed_failed_{0};
    std::atomic<std::size_t>   retries_{0};
    std::atomic<std::size_t>   not_found_{0};
    std::atomic<std::size_t>   server_error_{0};
    std::atomic<std::size_t>   transport_error_{0};
    std::atomic<std::size_t>   zip_error_{0};
    std::atomic<std::size_t>   cancelled_{0};
    std::atomic<std::size_t>   cancellations_dropped_{0};
    std::atomic<std::uint64_t> bytes_fetched_{0};
    std::atomic<std::uint64_t> bytes_inflated_{0};

    std::atomic<std::size_t> window_count_{0};
    std::atomic<std::size_t> window_failures_{0};

    /// Start time of each worker's current fetch, zero when idle.
    std::vector<std::atomic<std::int64_t>> started_at_;

    mutable std::mutex                         failures_mutex_;
    std::array<FetchFailure, kFailureRingSize> failures_;
    std::size_t                                failure_head_{0};
    std::size_t                                failure_count_{0};
};

}  // namespace qp::data_source::source::venue::binance
