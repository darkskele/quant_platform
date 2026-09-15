#pragma once
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

#include "source.hpp"
#include "spsc_queue.hpp"
#include "types.hpp"
#include "venue.hpp"

namespace qp::data_source::source {

/// Backtest `Source` for CSV-line-shaped venue data. Merges a runtime number
/// of logical streams by timestamp, each an ordered list of files.
/// One background thread reads every stream's files ahead of consumption,
/// pushing complete lines into a per-stream queue.
template <venue::Parser Parser, std::size_t LineQueueCapacity = 512>
class CsvSource {
    static std::span<const std::byte> as_bytes(std::string_view text) {
        return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
    }

   public:
    /// `files[i]` is stream i's complete file list, already in the order
    /// they should be merged in. The stream count is files.size().
    explicit CsvSource(std::vector<std::vector<std::filesystem::path>> files)
        : n_(files.size()), producer_(n_), shared_(n_), lookahead_(n_) {
        for (std::size_t i = 0; i < n_; ++i) producer_[i].files = std::move(files[i]);
        producer_thread_ = std::thread([this] { producer_loop(); });
    }

    // Producer thread captures `this`.
    CsvSource(const CsvSource&)            = delete;
    CsvSource& operator=(const CsvSource&) = delete;
    CsvSource(CsvSource&&)                 = delete;
    CsvSource& operator=(CsvSource&&)      = delete;

    ~CsvSource() {
        stop_requested_.store(true, std::memory_order_relaxed);
        if (producer_thread_.joinable()) producer_thread_.join();
    }

    /// Single pass: fills each empty lookahead slot and tracks the running
    /// earliest at the same time, each slot touched once per call. NoData
    /// while any stream is merely not-ready-yet; Eof only once every stream
    /// is genuinely exhausted.
    PullResult next() {
        bool                       any_missing = false;
        std::optional<std::size_t> earliest;

        for (std::size_t i = 0; i < n_; ++i) {
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
            if (!earliest || base_of(*lookahead_[i]).ts < base_of(*lookahead_[*earliest]).ts) {
                earliest = i;
            }
        }

        // Some stream might yet produce an earlier ts -> can't emit, not done.
        if (any_missing) return std::unexpected(SourceStatus::NoData);
        if (!earliest) return std::unexpected(SourceStatus::Eof);  // every stream exhausted

        MarketEvent out = std::move(*lookahead_[*earliest]);
        lookahead_[*earliest].reset();
        return out;
    }

   private:
    // One queued item: a view into a whole file's buffer, kept alive by
    // the shared_ptr riding along with it.
    struct Line {
        std::shared_ptr<const std::string> buffer;
        std::string_view                   text;
    };

    // Background-thread-only. current_lines is every line of the file
    // currently being drained
    struct ProducerState {
        std::vector<std::filesystem::path> files;
        std::size_t                        file_index{0};
        std::shared_ptr<const std::string> current_buffer;
        std::vector<std::string_view>      current_lines;
        std::size_t                        line_index{0};
    };

    // The only cross-thread surface per stream.
    struct SharedState {
        SpscQueue<Line, LineQueueCapacity> lines;
        std::atomic<bool>                  done{false};
        std::atomic<bool>                  failed{false};
        std::string                        error;
    };

    // Filled: lookahead_[i] now holds a fresh event. NotReady: this
    // stream's queue is empty but its producer isn't done.
    enum class RefillResult { Filled, NotReady, Exhausted };

    /// Pops lines from shared_[i].lines until one parses into an event.
    /// Throws, surfacing the producer's stored message, if shared_[i].failed
    /// was observed true.
    RefillResult try_refill(std::size_t i) {
        auto& shared = shared_[i];
        for (;;) {
            if (shared.failed.load(std::memory_order_acquire)) {
                throw std::runtime_error(shared.error);
            }
            // Read done BEFORE popping, see SharedState's own comment for
            // why this order, not the reverse, is what avoids the race.
            bool done = shared.done.load(std::memory_order_acquire);

            auto line = shared.lines.pop();
            if (!line) return done ? RefillResult::Exhausted : RefillResult::NotReady;

            if (auto ev = Parser::parse(as_bytes(line->text))) {
                lookahead_[i] = std::move(ev);
                return RefillResult::Filled;
            }
            // Unparseable line, loop and try the next one.
        }
    }

    /// One stream's unit of work for one producer round.
    bool producer_fill_one(std::size_t i) {
        auto& prod   = producer_[i];
        auto& shared = shared_[i];

        if (prod.line_index < prod.current_lines.size()) {
            Line item{prod.current_buffer, prod.current_lines[prod.line_index]};
            // push()'s fullness check.
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

    bool producer_round() {
        bool any_progress = false;
        for (std::size_t i = 0; i < n_; ++i) {
            if (shared_[i].done.load(std::memory_order_relaxed)) continue;
            if (producer_fill_one(i)) any_progress = true;
        }
        return any_progress;
    }

    // Try every not-yet-done stream each round; if a full round makes zero
    // progress (every queue full), yield instead of a timed sleep.
    void producer_loop() {
        while (!stop_requested_.load(std::memory_order_relaxed)) {
            bool all_done = true;
            for (std::size_t i = 0; i < n_; ++i) {
                if (!shared_[i].done.load(std::memory_order_relaxed)) {
                    all_done = false;
                    break;
                }
            }
            if (all_done) return;

            if (!producer_round()) {
                std::this_thread::yield();
            }
        }
    }

    std::size_t                             n_;
    std::vector<ProducerState>              producer_;
    std::vector<SharedState>                shared_;
    std::vector<std::optional<MarketEvent>> lookahead_;

    std::thread       producer_thread_;
    std::atomic<bool> stop_requested_{false};
};

}  // namespace qp::data_source::source
