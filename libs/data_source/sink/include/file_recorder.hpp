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

// The Sink concept's concrete Phase-0 implementation (architecture-
// principles.md): tees MarketEvents to local compressed, partitioned files
// on a dedicated writer thread behind an SPSC queue, so a disk stall can
// never back up the socket read this is fed from — same reasoning as
// LiveWebSocketSource's own I/O-thread/queue split.
//
// Local disk only, no cloud upload code (docs/decisions.md D11). Files land
// at {data_dir}/{symbol_name}/{YYYY-MM-DD}.bin.zst, using the shared wire
// format (wire.hpp) that FileReplaySource (Phase 1) will read
// back with — see D12. A colocated {YYYY-MM-DD}.manifest sidecar records
// gap/resync notes via log(), unfiltered, for whoever's calling record()
// (the collector) to use as it sees fit — D12's polling-driven alerting.
//
// Not thread-safe beyond record()/log() themselves: construction must
// finish (and symbol_names must already be complete) before any other
// thread touches this object, same contract as LiveWebSocketSource.
class FileRecorder {
   public:
    // `symbol_names`: SymbolId -> exchange symbol name, index == SymbolId —
    // pass LiveWebSocketSource::symbol_names() directly (safe to call any
    // time after that source's construction; see its own doc comment).
    // `data_dir` is created if it doesn't exist; per-symbol subdirectories
    // are created lazily, only for symbols that actually produce an event.
    // `flush_interval` is D12's crash-safety cadence — how often every open
    // partition gets an ZSTD_e_flush checkpoint. A real, tunable operational
    // parameter (not a test-only knob): shorter bounds data loss on a crash
    // tighter at some I/O-overhead cost, longer is the inverse trade.
    explicit FileRecorder(std::filesystem::path data_dir, std::vector<std::string> symbol_names,
                          std::chrono::milliseconds flush_interval = std::chrono::seconds(1));
    ~FileRecorder();

    FileRecorder(const FileRecorder&)            = delete;
    FileRecorder& operator=(const FileRecorder&) = delete;
    FileRecorder(FileRecorder&&)                 = delete;
    FileRecorder& operator=(FileRecorder&&)      = delete;

    // Non-blocking tee — the caller's hot path. Never touches disk itself;
    // just hands off to the writer thread.
    void record(MarketEvent event);

    // Rare (gap/resync notes from the collector's poll, not per-event) —
    // appends a timestamped line to `symbol`'s *currently open* manifest,
    // whatever day that happens to be. Routed through the same writer
    // thread as record() specifically so "which day is current" is decided
    // by one thread reusing one piece of state, never independently
    // recomputed from wall-clock time — that's what keeps a manifest line
    // paired with the data partition it actually describes.
    void log(SymbolId symbol, std::string message);

    // Events dropped because the queue was full (writer thread too slow —
    // disk/compression falling behind arrival rate).
    std::size_t dropped_count() const noexcept;

    // Log lines dropped (queue full). Should be ~never — log() is rare by
    // design; non-zero here means the writer thread itself is stuck, not
    // just slow (same signal LiveWebSocketSource's resync_request_dropped_
    // gives for its own request channel).
    std::size_t log_dropped_count() const noexcept;

    // Max observed depth of the event queue since start — the early
    // warning for "the writer thread isn't keeping up," before it starts
    // actually dropping.
    std::size_t queue_high_water_mark() const noexcept;

    // Short writes / flush failures on the underlying FILE* (e.g. disk
    // full) since start. libc's fwrite/fflush return codes are checked but
    // never thrown on — a failing disk shouldn't take down every other
    // symbol's recording — so this is the only signal a partial/corrupt
    // write actually happened.
    std::size_t write_error_count() const noexcept;

   private:
    struct LogLine {
        SymbolId    symbol;
        std::string message;
    };

    // One symbol's currently-open file state. Lazily populated — default-
    // constructed (no file handles, no compressor) until that symbol's
    // first event/log line actually arrives, so subscribing to N symbols
    // never eagerly creates N sets of files/zstd contexts for symbols that
    // turn out to be quiet or unused.
    struct Partition {
        std::optional<DayKey>           open_day;
        std::FILE*                      data_file     = nullptr;
        std::FILE*                      manifest_file = nullptr;
        std::unique_ptr<ZstdCompressor> compressor;
        std::vector<std::byte>          encode_buf;    // scratch: wire-format bytes
        std::vector<std::byte>          compress_buf;  // scratch: compressed output
    };

    void writer_run();
    void handle_event(MarketEvent event);
    void handle_log(const LogLine& line);
    void ensure_open(Partition& part, SymbolId symbol, DayKey day);
    void close_partition(Partition& part);
    void maybe_flush(Partition& part);

    // Checked fwrite/fflush — increments write_errors_ instead of throwing;
    // a bad write shouldn't unwind out of the writer thread (see writer_run).
    void checked_write(std::FILE* f, const std::byte* data, std::size_t size);
    void checked_flush(std::FILE* f);

    static constexpr std::size_t kQueueCapacity = 1 << 11;  // events — same sizing reasoning
                                                            // as LiveWebSocketSource's queue_:
                                                            // jitter absorption for an
                                                            // actively-draining consumer, not
                                                            // a stand-in for a dead one.
    static constexpr std::size_t kLogQueueCapacity = 64;    // log() is rare by design

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
