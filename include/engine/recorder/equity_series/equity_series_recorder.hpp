#pragma once
#include <atomic>
#include <span>
#include <thread>
#include <vector>

#include "portfolio.hpp"
#include "recorder/recorder.hpp"
#include "spsc_queue.hpp"
#include "types.hpp"

namespace qp::engine {

/// Portfolio equity at one replay timestamp.
struct EquityPoint {
    Timestamp ts{};
    Notional  equity{};
};

/// Owns the off-hot-path side of equity recording: a drain thread empties a
/// lock-free queue into a growing series. The record thread only ever does a
/// bounded, allocation-free push; the vector grows here instead.
class EquitySeriesCollector {
    // Burst buffer between the record thread and the drain. Only needs to
    // absorb a scheduling hiccup, not the whole series.
    static constexpr std::size_t kQueueCapacity = 1u << 16;

   public:
    EquitySeriesCollector()                                        = default;
    EquitySeriesCollector(const EquitySeriesCollector&)            = delete;
    EquitySeriesCollector& operator=(const EquitySeriesCollector&) = delete;

    ~EquitySeriesCollector() { finish(); }

    /// Begins a fresh recording: clears the prior series and starts draining.
    void start() {
        finish();
        series_.clear();
        series_.reserve(kQueueCapacity);
        stop_.store(false, std::memory_order_relaxed);
        drain_ = std::thread([this] { drain_loop(); });
    }

    /// Record-thread side. Spins only if the burst buffer is momentarily
    /// full, never allocates.
    void push(EquityPoint point) {
        while (!queue_.push(point)) std::this_thread::yield();
    }

    /// Stops draining and flushes the queue into the series. Idempotent, and
    /// safe only once the record thread has stopped pushing.
    void finish() {
        if (!drain_.joinable()) return;
        stop_.store(true, std::memory_order_release);
        drain_.join();
    }

    std::span<const EquityPoint> series() const noexcept { return series_; }

   private:
    void drain_loop() {
        while (!stop_.load(std::memory_order_acquire)) {
            if (auto point = queue_.pop())
                series_.push_back(*point);
            else
                std::this_thread::yield();
        }
        while (auto point = queue_.pop())
            series_.push_back(*point);  // producer done, take the rest
    }

    SpscQueue<EquityPoint, kQueueCapacity, /*UseHeap=*/true> queue_;
    std::vector<EquityPoint>                                 series_;
    std::thread                                              drain_;
    std::atomic<bool>                                        stop_{false};
};

/// The record-thread handle Engine drives. sample() hands one point to the
/// collector, which owns the allocation and the drain.
class EquitySeriesRecorder {
   public:
    explicit EquitySeriesRecorder(EquitySeriesCollector* sink) noexcept : sink_(sink) {}

    template <class Book>
    void sample(Timestamp ts, const Book& book) {
        sink_->push({ts, book.equity()});
    }

   private:
    EquitySeriesCollector* sink_;
};

static_assert(Recorder<EquitySeriesRecorder, qp::Portfolio<qp::detail::kTrivialCounts>>);

}  // namespace qp::engine
