#include "http_fetch_pool.hpp"

#include <algorithm>
#include <utility>

#include "client.hpp"
#include "zip.hpp"

namespace qp::data_source::source::venue::binance {
namespace {

namespace http = network::http;

/// Wall clock, because this measures real network time. Event timestamps come
/// from file contents, so determinism is untouched.
std::int64_t now_millis() noexcept {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

constexpr bool retryable(FetchStatus status) noexcept {
    return status == FetchStatus::ServerError || status == FetchStatus::TransportError;
}

}  // namespace

HttpFetchPool::HttpFetchPool(HttpFetchPoolConfig config, std::function<void()> on_critical)
    : config_(config), on_critical_(std::move(on_critical)), started_at_(config.workers) {
    config_.http.max_connections_per_host =
        std::max(config_.http.max_connections_per_host, config_.workers);

    workers_.reserve(config_.workers);
    workers_alive_.store(config_.workers, std::memory_order_relaxed);
    for (std::size_t i = 0; i < config_.workers; ++i)
        workers_.emplace_back([this, i] { worker_loop(i); });
}

HttpFetchPool::~HttpFetchPool() { quiesce(); }

bool HttpFetchPool::submit(std::string url, FileSlots* destination, std::size_t at) {
    // Marks this submit in progress before testing the flag, so quiesce cannot
    // drain between the test and the push.
    submits_active_.fetch_add(1, std::memory_order_acquire);

    struct Leave {
        std::atomic<std::size_t>& count;

        ~Leave() { count.fetch_sub(1, std::memory_order_release); }
    } leave{submits_active_};

    if (stopping_.load(std::memory_order_acquire)) return false;

    if (!tasks_.push(Task{std::move(url), destination, at})) {
        refused_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    submitted_.fetch_add(1, std::memory_order_relaxed);
    queued_.fetch_add(1, std::memory_order_release);

    // Only touch the condvar when somebody is actually asleep. A worker that
    // sleeps just after this still sees the task through its wait predicate.
    if (sleepers_.load(std::memory_order_acquire) > 0) wake_.notify_one();
    return true;
}

void HttpFetchPool::quiesce() {
    // Held throughout, so a second caller blocks until the workers are joined
    // rather than returning while they still run.
    std::lock_guard shutdown(shutdown_mutex_);
    if (quiesced_) return;

    // Set under the wait mutex, so a worker between its predicate check and its
    // sleep cannot miss the notify.
    {
        std::lock_guard wake_lock(wake_mutex_);
        stopping_.store(true, std::memory_order_release);
    }
    wake_.notify_all();

    for (auto& worker : workers_)
        if (worker.joinable()) worker.join();
    workers_.clear();

    // Let any submit that read stopping_ as false finish its push first.
    while (submits_active_.load(std::memory_order_acquire) > 0) std::this_thread::yield();

    // Anything the workers never reached still has a stream waiting on it, so
    // the cancellation is placed here instead. The slot was reserved when the
    // task was submitted, so this cannot block and cannot be refused.
    while (auto task = tasks_.try_pop()) {
        queued_.fetch_sub(1, std::memory_order_release);
        cancelled_.fetch_add(1, std::memory_order_relaxed);
        completed_failed_.fetch_add(1, std::memory_order_relaxed);
        if (task->destination == nullptr) continue;

        if (!task->destination->place(task->at, FetchedFile{{}, FetchStatus::Cancelled}))
            cancellations_dropped_.fetch_add(1, std::memory_order_relaxed);
    }

    quiesced_ = true;
}

void HttpFetchPool::worker_loop(std::size_t index) {
    Task task;
    while (take_task(task)) run_task(task, index);
    workers_alive_.fetch_sub(1, std::memory_order_release);
}

bool HttpFetchPool::take_task(Task& out) {
    for (;;) {
        // Tested before the queue, so shutdown does not first work through the
        // whole backlog. Those tasks are cancelled by the drain instead.
        if (stopping_.load(std::memory_order_acquire)) return false;

        if (auto task = tasks_.try_pop()) {
            queued_.fetch_sub(1, std::memory_order_release);
            out = std::move(*task);
            return true;
        }
        if (stopping_.load(std::memory_order_acquire)) return false;

        std::unique_lock lock(wake_mutex_);
        sleepers_.fetch_add(1, std::memory_order_release);
        wake_.wait_for(lock, std::chrono::milliseconds{50}, [this] {
            return stopping_.load(std::memory_order_acquire) ||
                   queued_.load(std::memory_order_acquire) > 0;
        });
        sleepers_.fetch_sub(1, std::memory_order_release);
    }
}

void HttpFetchPool::run_task(Task& task, std::size_t index) {
    in_flight_.fetch_add(1, std::memory_order_release);
    started_at_[index].store(now_millis(), std::memory_order_release);

    FetchedFile result;
    std::string detail;
    std::size_t attempts = 0;

    for (std::size_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
        ++attempts;
        std::vector<std::byte> zip_bytes;

        try {
            zip_bytes = http::get(task.url, config_.http);
            bytes_fetched_.fetch_add(zip_bytes.size(), std::memory_order_relaxed);
            result.status = FetchStatus::Ok;
        } catch (const http::HttpStatusError& e) {
            result.status = e.status == 404 ? FetchStatus::NotFound : FetchStatus::ServerError;
            detail        = e.what();
        } catch (const std::exception& e) {
            result.status = FetchStatus::TransportError;
            detail        = e.what();
        }

        if (result.status == FetchStatus::Ok) {
            try {
                result.body = archive::zip::unzip_single_entry(zip_bytes);
                bytes_inflated_.fetch_add(result.body.size(), std::memory_order_relaxed);
                break;
            } catch (const std::exception& e) {
                result.status = FetchStatus::ZipError;
                detail        = e.what();
                result.body.clear();
            }
        }

        if (!retryable(result.status) || attempt == config_.max_retries ||
            stopping_.load(std::memory_order_acquire))
            break;

        retries_.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(config_.backoff_base * (std::size_t{1} << attempt));
    }

    switch (result.status) {
        case FetchStatus::Ok:
            break;
        case FetchStatus::NotFound:
            not_found_.fetch_add(1, std::memory_order_relaxed);
            break;
        case FetchStatus::ServerError:
            server_error_.fetch_add(1, std::memory_order_relaxed);
            break;
        case FetchStatus::TransportError:
            transport_error_.fetch_add(1, std::memory_order_relaxed);
            break;
        case FetchStatus::ZipError:
            zip_error_.fetch_add(1, std::memory_order_relaxed);
            break;
        case FetchStatus::Cancelled:
            cancelled_.fetch_add(1, std::memory_order_relaxed);
            break;
    }

    const bool failed = is_failure(result.status);
    if (failed) record_failure(task, result.status, detail, attempts);
    note_completion(failed);

    started_at_[index].store(0, std::memory_order_release);
    in_flight_.fetch_sub(1, std::memory_order_release);

    deliver(task, std::move(result));
}

void HttpFetchPool::deliver(const Task& task, FetchedFile file) {
    if (task.destination == nullptr) return;
    // The slot belongs to this task alone and was reserved before the fetch
    // started, so a refusal means the caller reused a position.
    if (!task.destination->place(task.at, std::move(file)))
        cancellations_dropped_.fetch_add(1, std::memory_order_relaxed);
}

void HttpFetchPool::note_completion(bool failed) {
    if (failed)
        completed_failed_.fetch_add(1, std::memory_order_relaxed);
    else
        completed_ok_.fetch_add(1, std::memory_order_relaxed);

    if (failed) window_failures_.fetch_add(1, std::memory_order_relaxed);
    if (window_count_.fetch_add(1, std::memory_order_acq_rel) + 1 < config_.critical_window) return;

    const auto failures = window_failures_.exchange(0, std::memory_order_acq_rel);
    window_count_.store(0, std::memory_order_release);

    const double ratio =
        static_cast<double>(failures) / static_cast<double>(config_.critical_window);
    if (ratio < config_.critical_failure_ratio) return;
    if (critical_.exchange(true, std::memory_order_acq_rel)) return;
    if (on_critical_) on_critical_();
}

void HttpFetchPool::record_failure(const Task& task, FetchStatus status, const std::string& detail,
                                   std::size_t attempts) {
    std::lock_guard lock(failures_mutex_);
    failures_[failure_head_] = FetchFailure{task.url, detail, status, attempts};
    failure_head_            = (failure_head_ + 1) % kFailureRingSize;
    if (failure_count_ < kFailureRingSize) ++failure_count_;
}

FetchPoolStats HttpFetchPool::stats() const {
    FetchPoolStats out;
    out.submitted             = submitted_.load(std::memory_order_relaxed);
    out.refused               = refused_.load(std::memory_order_relaxed);
    out.completed_ok          = completed_ok_.load(std::memory_order_relaxed);
    out.completed_failed      = completed_failed_.load(std::memory_order_relaxed);
    out.retries               = retries_.load(std::memory_order_relaxed);
    out.not_found             = not_found_.load(std::memory_order_relaxed);
    out.server_error          = server_error_.load(std::memory_order_relaxed);
    out.transport_error       = transport_error_.load(std::memory_order_relaxed);
    out.zip_error             = zip_error_.load(std::memory_order_relaxed);
    out.cancelled             = cancelled_.load(std::memory_order_relaxed);
    out.cancellations_dropped = cancellations_dropped_.load(std::memory_order_relaxed);
    out.queued                = queued_.load(std::memory_order_relaxed);
    out.in_flight             = in_flight_.load(std::memory_order_relaxed);
    out.workers_alive         = workers_alive_.load(std::memory_order_relaxed);
    out.bytes_fetched         = bytes_fetched_.load(std::memory_order_relaxed);
    out.bytes_inflated        = bytes_inflated_.load(std::memory_order_relaxed);
    out.critical              = critical_.load(std::memory_order_relaxed);

    const auto   now    = now_millis();
    std::int64_t oldest = 0;
    for (const auto& started : started_at_) {
        const auto at = started.load(std::memory_order_acquire);
        if (at != 0 && now - at > oldest) oldest = now - at;
    }
    out.oldest_in_flight_ms = oldest;
    return out;
}

std::vector<FetchFailure> HttpFetchPool::recent_failures() const {
    std::lock_guard           lock(failures_mutex_);
    std::vector<FetchFailure> out;
    out.reserve(failure_count_);
    for (std::size_t i = 0; i < failure_count_; ++i) {
        const auto index =
            (failure_head_ + kFailureRingSize - failure_count_ + i) % kFailureRingSize;
        out.push_back(failures_[index]);
    }
    return out;
}

}  // namespace qp::data_source::source::venue::binance
