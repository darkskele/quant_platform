#pragma once
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "partition.hpp"
#include "types.hpp"
#include "zstd_stream.hpp"

namespace qp::source {

/// Backtest `Source`: replays what FileRecorder wrote, via the shared wire
/// codec (D12), in ascending timestamp order across every requested symbol.
///
/// Single-threaded/synchronous — no reader thread/queue like
/// FileRecorder's or GenericLiveWebSocketSource's, since there's no live
/// socket to protect from backpressure here. `next()` returns nullopt at
/// true end-of-data (unlike the live source's nullopt, which just means
/// "queue empty right now").
class FileReplaySource {
   public:
    /// Reads data_dir/symbols.manifest (written by FileRecorder) for the
    /// canonical SymbolId <-> name order — never independently re-supplied
    /// here, so it can't silently disagree with what was actually
    /// recorded. `wanted`, if given, replays only those symbols (SymbolIds
    /// still come from the manifest's own order, not renumbered over the
    /// subset). Throws if the manifest is missing, or if a `wanted` name
    /// isn't in it. Replays every event with
    /// `first_day <= day_key_for(ts) <= last_day`.
    FileReplaySource(std::filesystem::path data_dir, wire::DayKey first_day, wire::DayKey last_day,
                     std::optional<std::vector<std::string>> wanted = std::nullopt);

    std::optional<MarketEvent> next();

   private:
    // Per-symbol read position; next() k-way-merges across these by ts.
    // `decompressor` is heap-allocated since ZstdDecompressor isn't
    // movable and this struct lives in a std::vector.
    struct SymbolCursor {
        SymbolId                                symbol{};
        std::vector<std::filesystem::path>      segments;
        std::size_t                             segment_index{0};
        std::ifstream                           file;
        std::unique_ptr<wire::ZstdDecompressor> decompressor;
        std::vector<std::byte>                  read_buf;
        std::vector<std::byte>                  decoded_buf;
        std::size_t                             consumed{0};
        std::optional<MarketEvent>              pending;

        bool fill_more();  // decompress more into decoded_buf; false once segments are exhausted
        bool advance();    // parse next record into pending; false once this symbol is done
    };

    std::vector<SymbolCursor> cursors_;
};

}  // namespace qp::source
