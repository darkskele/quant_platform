#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "partition.hpp"
#include "spsc_queue.hpp"
#include "types.hpp"
#include "zstd_stream.hpp"

namespace qp::sink {

using wire::DayKey;
using wire::ZstdCompressor;

// Sink concept's Phase-0 implementation (architecture-principles.md): tees
// MarketEvents to compressed, partitioned local files on their own writer
// thread + SPSC queue. Local disk only (D11). Not thread-safe beyond
// record()/log() — construction must finish before any other thread
// touches this object, same contract as LiveWebSocketSource.
class FileRecorder {
   public:
    // symbol_names index == SymbolId (pass LiveWebSocketSource::
    // symbol_names() directly). flush_interval bounds crash data loss vs
    // I/O overhead (D12).
    explicit FileRecorder(std::filesystem::path data_dir, std::vector<std::string> symbol_names,
                          std::chrono::milliseconds flush_interval = std::chrono::seconds(1));
    ~FileRecorder();

    FileRecorder(const FileRecorder&)            = delete;
    FileRecorder& operator=(const FileRecorder&) = delete;
    FileRecorder(FileRecorder&&)                 = delete;
    FileRecorder& operator=(FileRecorder&&)      = delete;

    // Non-blocking; drops silently if the queue is full — see dropped_count().
    void record(MarketEvent event);

    // Rare (gap/resync notes, not per-event) — see log_dropped_count().
    void log(SymbolId symbol, std::string message);

    std::size_t dropped_count() const noexcept;
    std::size_t log_dropped_count() const noexcept;
    std::size_t queue_high_water_mark() const noexcept;

    // libc write/flush failures never throw (see checked_write); this is
    // the only signal a partial/corrupt write actually happened.
    std::size_t write_error_count() const noexcept;

   private:
    struct LogLine {
        SymbolId    symbol;
        std::string message;
    };

    // One symbol's currently-open file state; default-constructed (nothing
    // open) until ensure_open() first touches it.
    struct Partition {
        std::optional<DayKey>           open_day;
        std::FILE*                      data_file     = nullptr;
        std::FILE*                      manifest_file = nullptr;
        std::unique_ptr<ZstdCompressor> compressor;
        std::vector<std::byte>          encode_buf;    // scratch: wire-format bytes
        std::vector<std::byte>          compress_buf;  // scratch: compressed output
    };

    void write_symbols_manifest();
    void writer_run();
    void handle_event(MarketEvent event);
    void handle_log(const LogLine& line);
    void ensure_open(Partition& part, SymbolId symbol, DayKey day);
    void close_partition(Partition& part);
    void maybe_flush(Partition& part);
    void checked_write(std::FILE* f, const std::byte* data, std::size_t size);
    void checked_flush(std::FILE* f);

    static constexpr std::size_t kQueueCapacity    = 1 << 11;  // see LiveWebSocketSource's queue_
    static constexpr std::size_t kLogQueueCapacity = 64;

    std::filesystem::path     data_dir_;
    std::vector<std::string>  symbol_names_;  // SymbolId -> name, immutable after construction
    std::vector<Partition>    partitions_;    // writer-thread-only; sized to symbol_names_.size()
    std::chrono::milliseconds flush_interval_;

    SpscQueue<MarketEvent, kQueueCapacity> queue_;
    SpscQueue<LogLine, kLogQueueCapacity>  log_queue_;

    std::atomic<bool>        running_{true};
    std::atomic<std::size_t> dropped_{0};
    std::atomic<std::size_t> log_dropped_{0};
    std::atomic<std::size_t> queue_high_water_{0};
    std::atomic<std::size_t> write_errors_{0};

    std::thread writer_thread_;
};

}  // namespace qp::sink
