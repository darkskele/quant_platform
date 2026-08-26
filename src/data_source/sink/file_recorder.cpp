#include "file_recorder.hpp"

#include <cassert>
#include <chrono>
#include <format>
#include <stdexcept>

#include "wire.hpp"

namespace qp::sink {

using wire::day_key_for;
using wire::format_day;
using wire::needs_rotation;
using wire::segment_path;
using wire::write_event;

FileRecorder::FileRecorder(std::filesystem::path data_dir, std::vector<std::string> symbol_names,
                           std::chrono::milliseconds flush_interval)
    : data_dir_(std::move(data_dir)),
      symbol_names_(std::move(symbol_names)),
      partitions_(symbol_names_.size()),
      flush_interval_(flush_interval) {
    std::filesystem::create_directories(data_dir_);
    write_symbols_manifest();
    writer_thread_ = std::thread([this] { writer_run(); });
}

// The canonical symbol list FileReplaySource reads back — so it doesn't
// need its own independently-supplied copy that could silently disagree
// with what was actually recorded. Written once, on the constructing
// thread, before the writer thread starts.
void FileRecorder::write_symbols_manifest() {
    std::FILE* f = std::fopen((data_dir_ / "symbols.manifest").c_str(), "w");
    if (!f) {
        throw std::runtime_error("FileRecorder: failed to write " +
                                 (data_dir_ / "symbols.manifest").string());
    }
    for (const auto& name : symbol_names_) std::fprintf(f, "%s\n", name.c_str());
    std::fclose(f);
}

FileRecorder::~FileRecorder() {
    running_.store(false, std::memory_order_release);
    if (writer_thread_.joinable()) writer_thread_.join();
}

void FileRecorder::record(MarketEvent event) {
    if (!queue_.push(std::move(event))) dropped_.fetch_add(1, std::memory_order_relaxed);
}

// Appends to `symbol`'s *currently open* manifest, whatever day that is.
// Routed through the writer thread (not written directly here) so "which
// day is current" is decided by one thread reusing one piece of state,
// never independently recomputed from wall-clock time — that's what keeps
// a manifest line paired with the data partition it actually describes.
void FileRecorder::log(SymbolId symbol, std::string message) {
    if (!log_queue_.push(LogLine{symbol, std::move(message)})) {
        log_dropped_.fetch_add(1, std::memory_order_relaxed);
    }
}

std::size_t FileRecorder::dropped_count() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
}

std::size_t FileRecorder::log_dropped_count() const noexcept {
    return log_dropped_.load(std::memory_order_relaxed);
}

std::size_t FileRecorder::queue_high_water_mark() const noexcept {
    return queue_high_water_.load(std::memory_order_relaxed);
}

std::size_t FileRecorder::write_error_count() const noexcept {
    return write_errors_.load(std::memory_order_relaxed);
}

// --- Writer thread -------------------------------------------------------

// A bad write shouldn't unwind out of the writer thread (see writer_run's
// try/catch around every handle_event/handle_log call) — checked_write/
// checked_flush increment write_errors_ instead of throwing.
void FileRecorder::checked_write(std::FILE* f, const std::byte* data, std::size_t size) {
    if (std::fwrite(data, 1, size, f) != size)
        write_errors_.fetch_add(1, std::memory_order_relaxed);
}

void FileRecorder::checked_flush(std::FILE* f) {
    if (std::fflush(f) != 0) write_errors_.fetch_add(1, std::memory_order_relaxed);
}

void FileRecorder::ensure_open(Partition& part, SymbolId symbol, DayKey day) {
    if (!needs_rotation(part.open_day, day)) return;
    close_partition(part);

    const std::string&    name       = symbol_names_[symbol];
    std::filesystem::path symbol_dir = data_dir_ / name;
    std::filesystem::create_directories(symbol_dir);

    const std::string date_str = format_day(day);

    // A fresh, uniquely-named segment every time a partition is (re)opened
    // for this symbol+day — never appended to across a process restart.
    // Reason: an interrupted (crashed) zstd frame's dangling tail bytes
    // aren't a valid place to resume writing a *new* frame — a reader
    // decoding frame-by-frame would choke on the garbage before ever
    // reaching fresh data appended after it. A brand-new file has no such
    // risk: it's either a complete, valid frame (clean close) or valid up
    // to its last flush() checkpoint (crash) — never "valid content stuck
    // after garbage." FileReplaySource reads {date}.NNN.bin.zst segments in
    // order (wire::list_segments) and concatenates. The manifest doesn't
    // need this — plain text tolerates a ragged/duplicate tail line just
    // fine, so it stays a single append-mode file per day.
    std::filesystem::path data_path;
    for (int seq = 0;; ++seq) {
        data_path = segment_path(symbol_dir, day, seq);
        if (!std::filesystem::exists(data_path)) break;
    }
    std::filesystem::path manifest_path = symbol_dir / (date_str + ".manifest");

    part.data_file = std::fopen(data_path.c_str(), "wb");
    if (!part.data_file)
        throw std::runtime_error("FileRecorder: failed to open " + data_path.string());

    part.manifest_file = std::fopen(manifest_path.c_str(), "a");
    if (!part.manifest_file) {
        std::fclose(part.data_file);
        part.data_file = nullptr;
        throw std::runtime_error("FileRecorder: failed to open " + manifest_path.string());
    }

    part.compressor = std::make_unique<ZstdCompressor>();
    part.open_day   = day;
}

void FileRecorder::close_partition(Partition& part) {
    if (part.compressor) {
        part.compress_buf.clear();
        part.compressor->finish(part.compress_buf);
        if (!part.compress_buf.empty() && part.data_file) {
            checked_write(part.data_file, part.compress_buf.data(), part.compress_buf.size());
        }
        part.compressor.reset();
    }
    if (part.data_file) {
        std::fclose(part.data_file);
        part.data_file = nullptr;
    }
    if (part.manifest_file) {
        std::fclose(part.manifest_file);
        part.manifest_file = nullptr;
    }
    part.open_day.reset();
}

void FileRecorder::maybe_flush(Partition& part) {
    if (!part.compressor) return;  // nothing open for this symbol right now

    part.compress_buf.clear();
    part.compressor->flush(part.compress_buf);
    if (!part.compress_buf.empty()) {
        checked_write(part.data_file, part.compress_buf.data(), part.compress_buf.size());
    }
    // libc buffer -> OS; bounds a crash's real data loss to this point.
    checked_flush(part.data_file);
    if (part.manifest_file) checked_flush(part.manifest_file);
}

void FileRecorder::handle_event(MarketEvent event) {
    assert(event.symbol < partitions_.size());
    Partition& part = partitions_[event.symbol];

    ensure_open(part, event.symbol, day_key_for(event.ts));

    part.encode_buf.clear();
    write_event(part.encode_buf, event);

    part.compress_buf.clear();
    part.compressor->compress(part.encode_buf, part.compress_buf);
    if (!part.compress_buf.empty()) {
        checked_write(part.data_file, part.compress_buf.data(), part.compress_buf.size());
    }
}

void FileRecorder::handle_log(const LogLine& line) {
    assert(line.symbol < partitions_.size());
    Partition& part = partitions_[line.symbol];

    if (!part.compressor) {
        // No data partition open yet for this symbol — fall back to
        // today's UTC date so the note isn't lost. See the class doc: this
        // is the only case log() doesn't just reuse the data partition's
        // already-open day.
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
        ensure_open(part, line.symbol, day_key_for(now_ns));
    }

    auto        now = std::chrono::system_clock::now();
    std::string ts  = std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(now));
    if (std::fprintf(part.manifest_file, "%s %s %s\n", ts.c_str(),
                     symbol_names_[line.symbol].c_str(), line.message.c_str()) < 0) {
        write_errors_.fetch_add(1, std::memory_order_relaxed);
    }
}

void FileRecorder::writer_run() {
    static constexpr auto kIdleSleep = std::chrono::milliseconds(20);

    auto next_flush = std::chrono::steady_clock::now() + flush_interval_;

    while (running_.load(std::memory_order_acquire)) {
        bool did_work = false;

        while (auto ev = queue_.pop()) {
            did_work = true;
            try {
                handle_event(std::move(*ev));
            } catch (const std::exception& e) {
                write_errors_.fetch_add(1, std::memory_order_relaxed);
                std::fprintf(stderr, "[FileRecorder] dropping event, write failed: %s\n", e.what());
            }
        }
        while (auto line = log_queue_.pop()) {
            did_work = true;
            try {
                handle_log(*line);
            } catch (const std::exception& e) {
                write_errors_.fetch_add(1, std::memory_order_relaxed);
                std::fprintf(stderr, "[FileRecorder] dropping log line, write failed: %s\n",
                             e.what());
            }
        }

        std::size_t depth   = queue_.size();
        std::size_t current = queue_high_water_.load(std::memory_order_relaxed);
        while (depth > current && !queue_high_water_.compare_exchange_weak(
                                      current, depth, std::memory_order_relaxed)) {
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= next_flush) {
            for (auto& part : partitions_) maybe_flush(part);
            next_flush = now + flush_interval_;
        }

        if (!did_work) std::this_thread::sleep_for(kIdleSleep);
    }

    // Clean shutdown: drain whatever's left, then close every partition
    // properly (finish() ends the zstd frame — a real close, not just the
    // last flush() checkpoint the periodic cadence leaves mid-run).
    while (auto ev = queue_.pop()) {
        try {
            handle_event(std::move(*ev));
        } catch (const std::exception& e) {
            write_errors_.fetch_add(1, std::memory_order_relaxed);
            std::fprintf(stderr, "[FileRecorder] dropping event, write failed: %s\n", e.what());
        }
    }
    while (auto line = log_queue_.pop()) {
        try {
            handle_log(*line);
        } catch (const std::exception& e) {
            write_errors_.fetch_add(1, std::memory_order_relaxed);
            std::fprintf(stderr, "[FileRecorder] dropping log line, write failed: %s\n", e.what());
        }
    }
    for (auto& part : partitions_) close_partition(part);
}

}  // namespace qp::sink
