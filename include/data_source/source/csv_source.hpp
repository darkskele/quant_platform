#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "spsc_queue.hpp"
#include "types.hpp"
#include "venue.hpp"

namespace qp::data_source::source {

/// Backtest `Source` for CSV-line-shaped venue data (no assumption more
/// specific than that — genuinely venue-agnostic, unlike the name might
/// suggest at a glance). Merges N logical streams by timestamp, each an
/// ordered list of files (a "stream" is whatever the caller organized
/// together — one venue's kline history, one venue's funding history,
/// etc.; CsvSource has no notion of symbol, kind, or venue at all, only
/// Parser does). N is a compile-time bound, same convention as
/// BacktestInProcessTransport's N-rings template and Portfolio's
/// kMaxSymbols/kMaxVenues — the count is fixed at the type level, which
/// files is still a runtime constructor argument.
///
/// One background thread reads every stream's files ahead of consumption
/// (round-robin, index_sequence-unrolled — same idiom as
/// run_data_source::poll_round's fold-expansion and
/// RoundRobinPool::run_assigned's run_if_mine), pushing complete lines
/// into a per-stream queue. Whole-file reads, not chunked: every file this
/// venue actually produces is tens of KB (verified against real
/// data.binance.vision downloads), comfortably fits in memory at once, so
/// there's no "read a chunk, maybe not enough yet" loop to get right —
/// read the file, split every line, done. A queued line is a
/// std::string_view into a std::shared_ptr<const std::string> holding the
/// whole file's content — zero per-line allocation or copy; the shared_ptr
/// keeps the buffer alive exactly as long as any queued view into it is
/// still unconsumed. `next()` stays synchronous on its own thread: it only
/// ever pops an already-read line and calls Parser::parse() on it — the
/// genuinely unpredictable-latency part (open/read syscalls) never happens
/// on the calling thread.
///
/// One thread total, not one per stream: same reasoning as the earlier
/// FileReplaySource redesign — I/O here isn't the bottleneck once it's off
/// the calling thread, so N-way OS thread contention wouldn't buy
/// anything real.
template <venue::Parser Parser, std::size_t N, std::size_t LineQueueCapacity = 512>
class CsvSource {
    static_assert(N >= 1);

    static std::span<const std::byte> as_bytes(std::string_view text) {
        return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
    }

   public:
    /// `files[i]` is stream i's complete file list, already in the order
    /// they should be read/merged in (chronological, typically) — CsvSource
    /// doesn't sort or otherwise interpret it. Doesn't block: the
    /// background thread starts reading immediately but the constructor
    /// returns before any data has necessarily arrived — next()'s own "not
    /// ready yet" path (mirroring BacktestInProcessTransport's any_missing)
    /// handles that the same way it handles any later stall, no separate
    /// priming step needed.
    explicit CsvSource(std::array<std::vector<std::filesystem::path>, N> files) {
        for (std::size_t i = 0; i < N; ++i) producer_[i].files = std::move(files[i]);
        producer_thread_ = std::thread([this] { producer_loop(); });
    }

    // Producer thread captures `this` — a moved-from/copied instance would
    // leave it pointing at a stale address, same constraint RoundRobinPool
    // documents for itself.
    CsvSource(const CsvSource&)            = delete;
    CsvSource& operator=(const CsvSource&) = delete;
    CsvSource(CsvSource&&)                 = delete;
    CsvSource& operator=(CsvSource&&)      = delete;

    ~CsvSource() {
        stop_requested_.store(true, std::memory_order_relaxed);
        if (producer_thread_.joinable()) producer_thread_.join();
    }

    /// Single pass: fills each empty lookahead slot and tracks the running
    /// earliest at the same time, exactly like
    /// BacktestInProcessTransport::next() — each slot touched once per
    /// call. No global "stopped" state needed (unlike Transport): each
    /// stream's exhaustion is self-contained and permanent
    /// (SharedState::done, set once by that stream's own producer work) —
    /// once a stream has no lookahead and is done, it just drops out of
    /// consideration forever.
    std::optional<MarketEvent> next() {
        bool                       any_missing = false;
        std::optional<std::size_t> earliest;

        for (std::size_t i = 0; i < N; ++i) {
            if (!lookahead_[i]) {
                switch (try_refill(i)) {
                    case RefillResult::Filled:
                        break;  // falls through to the comparison below
                    case RefillResult::Exhausted:
                        continue;  // permanently done, never contributes again
                    case RefillResult::NotReady:
                        any_missing = true;  // might still beat the current earliest, later
                        continue;
                }
            }
            if (!earliest || header_of(*lookahead_[i]).ts < header_of(*lookahead_[*earliest]).ts) {
                earliest = i;
            }
        }

        if (any_missing) return std::nullopt;  // some stream might yet produce an earlier ts
        if (!earliest) return std::nullopt;    // every stream exhausted

        MarketEvent out = std::move(*lookahead_[*earliest]);
        lookahead_[*earliest].reset();
        return out;
    }

    /// Reflects state as of the last next() call, not a fresh check — same
    /// contract as BacktestInProcessTransport::is_done(): call next()
    /// first.
    bool is_done() const noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            if (lookahead_[i]) return false;
            if (!shared_[i].done.load(std::memory_order_acquire)) return false;
        }
        return true;
    }

   private:
    // One queued item: a view into a whole file's buffer, kept alive by
    // the shared_ptr riding along with it. Trivial to move (two pointers +
    // a length), no allocation of its own.
    struct Line {
        std::shared_ptr<const std::string> buffer;
        std::string_view                   text;
    };

    // Background-thread-only. current_lines is every line of the file
    // currently being drained (split once, up front, when the file is
    // read) — line_index tracks how many of those have been pushed to the
    // queue so far, since a whole file's lines may not fit in one queue-full
    // round.
    struct ProducerState {
        std::vector<std::filesystem::path> files;
        std::size_t                        file_index{0};
        std::shared_ptr<const std::string> current_buffer;
        std::vector<std::string_view>      current_lines;
        std::size_t                        line_index{0};
    };

    // The only cross-thread surface per stream. done/failed: producer
    // release-stores after its last push (done) or after writing `error`
    // (failed); next() acquire-loads BEFORE popping the queue, never
    // after — reading done/failed first is what makes "push-then-mark
    // between the consumer's failed pop and its done-check" safe to miss:
    // if done reads true, every push that happened before the producer set
    // it is already visible (release/acquire), so an empty queue at that
    // point really is empty for good.
    struct SharedState {
        SpscQueue<Line, LineQueueCapacity> lines;
        std::atomic<bool>                  done{false};
        std::atomic<bool>                  failed{false};
        std::string                        error;
    };

    // Filled: lookahead_[i] now holds a fresh event. NotReady: this
    // stream's queue is empty but its producer isn't done — might still
    // produce an earlier timestamp than whatever's currently winning, so
    // next() can't safely pick a winner yet. Exhausted: genuinely nothing
    // left, ever.
    enum class RefillResult { Filled, NotReady, Exhausted };

    /// Pops lines from shared_[i].lines until one parses into an event
    /// (unparseable lines — blank, malformed — are skipped, not fatal) or
    /// the queue runs dry. Throws, surfacing the producer's stored
    /// message, if shared_[i].failed was observed true.
    RefillResult try_refill(std::size_t i) {
        auto& shared = shared_[i];
        for (;;) {
            if (shared.failed.load(std::memory_order_acquire)) {
                throw std::runtime_error(shared.error);
            }
            // Read done BEFORE popping — see SharedState's own comment for
            // why this order, not the reverse, is what avoids the race.
            bool done = shared.done.load(std::memory_order_acquire);

            auto line = shared.lines.pop();
            if (!line) return done ? RefillResult::Exhausted : RefillResult::NotReady;

            if (auto ev = Parser::parse(as_bytes(line->text))) {
                lookahead_[i] = std::move(ev);
                return RefillResult::Filled;
            }
            // Unparseable line — loop and try the next one.
        }
    }

    /// One stream's unit of work for one producer round: push one more
    /// already-split line from the file currently being drained, or (once
    /// that file's lines are exhausted) read+split the next file, or (once
    /// the file list itself is exhausted) mark this stream done. Returns
    /// whether real progress happened — a full queue with a line still
    /// waiting is the only "no progress" case.
    bool producer_fill_one(std::size_t i) {
        auto& prod   = producer_[i];
        auto& shared = shared_[i];

        if (prod.line_index < prod.current_lines.size()) {
            Line item{prod.current_buffer, prod.current_lines[prod.line_index]};
            // push()'s fullness check runs before it ever touches its
            // argument, so `item` (and prod.line_index) are safe to leave
            // untouched if this returns false — nothing was moved-from.
            if (!shared.lines.push(std::move(item))) return false;
            ++prod.line_index;
            return true;
        }

        if (prod.file_index >= prod.files.size()) {
            shared.done.store(true, std::memory_order_release);
            return true;
        }

        const auto&   path = prod.files[prod.file_index];
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            shared.error = "CsvSource: failed to open " + path.string();
            shared.failed.store(true, std::memory_order_release);
            shared.done.store(true, std::memory_order_release);
            return true;
        }
        ++prod.file_index;

        file.seekg(0, std::ios::end);
        auto        size = static_cast<std::size_t>(file.tellg());
        std::string content(size, '\0');
        file.seekg(0, std::ios::beg);
        file.read(content.data(), static_cast<std::streamsize>(size));

        auto buffer         = std::make_shared<const std::string>(std::move(content));
        prod.current_buffer = buffer;
        prod.current_lines.clear();
        std::string_view view = *buffer;
        std::size_t      pos  = 0;
        while (pos < view.size()) {
            auto             nl = view.find('\n', pos);
            std::string_view line =
                view.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
            if (!line.empty()) prod.current_lines.push_back(line);
            if (nl == std::string_view::npos) break;
            pos = nl + 1;
        }
        prod.line_index = 0;
        return true;
    }

    template <std::size_t... Is>
    bool producer_round(std::index_sequence<Is...>) {
        bool any_progress = false;
        (([&] {
             if (shared_[Is].done.load(std::memory_order_relaxed)) return;
             if (producer_fill_one(Is)) any_progress = true;
         }()),
         ...);
        return any_progress;
    }

    // Structurally the same shape as run_data_source's own loop: try every
    // not-yet-done stream each round; if a full round makes zero progress
    // (every queue full), yield instead of busy-spinning — not a timed
    // sleep. Measured (bench_csv_source.cpp): a full 512-capacity queue
    // drains in ~90us at real per-event parse cost, so a fixed 1ms sleep
    // here left the producer asleep for ~90% of every fill-drain cycle —
    // this is disk I/O, not a slow remote call, so a full round genuinely
    // failing to make progress is a "queue's momentarily full," not "wait
    // a while," situation. yield() gives the consumer thread a scheduling
    // chance without pinning a full millisecond to it.
    void producer_loop() {
        while (!stop_requested_.load(std::memory_order_relaxed)) {
            bool all_done = true;
            for (std::size_t i = 0; i < N; ++i) {
                if (!shared_[i].done.load(std::memory_order_relaxed)) {
                    all_done = false;
                    break;
                }
            }
            if (all_done) return;

            if (!producer_round(std::make_index_sequence<N>{})) {
                std::this_thread::yield();
            }
        }
    }

    std::array<ProducerState, N>              producer_;
    std::array<SharedState, N>                shared_;
    std::array<std::optional<MarketEvent>, N> lookahead_{};

    std::thread       producer_thread_;
    std::atomic<bool> stop_requested_{false};
};

}  // namespace qp::data_source::source
