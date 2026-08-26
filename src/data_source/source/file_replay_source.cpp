#include "file_replay_source.hpp"

#include <algorithm>
#include <fstream>
#include <span>
#include <stdexcept>
#include <utility>

#include "wire.hpp"

namespace qp::source {

namespace {
constexpr std::size_t kReadChunk = 64 * 1024;

// data_dir/symbols.manifest, written by FileRecorder — one name per line,
// line index == SymbolId. The single canonical source of that mapping;
// FileReplaySource never accepts an independently-supplied one that could
// disagree with it.
std::vector<std::string> read_symbols_manifest(const std::filesystem::path& data_dir) {
    std::ifstream in(data_dir / "symbols.manifest");
    if (!in) {
        throw std::runtime_error("FileReplaySource: missing " +
                                 (data_dir / "symbols.manifest").string());
    }
    std::vector<std::string> names;
    std::string              line;
    while (std::getline(in, line)) {
        if (!line.empty()) names.push_back(line);
    }
    return names;
}
}  // namespace

FileReplaySource::FileReplaySource(std::filesystem::path data_dir, wire::DayKey first_day,
                                   wire::DayKey                            last_day,
                                   std::optional<std::vector<std::string>> wanted) {
    std::vector<std::string> manifest_names = read_symbols_manifest(data_dir);

    // Resolve every symbol to load against the manifest.
    std::vector<std::pair<std::string, SymbolId>> to_load;
    if (wanted) {
        to_load.reserve(wanted->size());
        for (const auto& name : *wanted) {
            auto it = std::find(manifest_names.begin(), manifest_names.end(), name);
            if (it == manifest_names.end()) {
                throw std::runtime_error("FileReplaySource: requested symbol \"" + name +
                                         "\" not found in " +
                                         (data_dir / "symbols.manifest").string());
            }
            // symbol is the manifest's global index
            to_load.emplace_back(name, static_cast<SymbolId>(it - manifest_names.begin()));
        }
    } else {
        to_load.reserve(manifest_names.size());
        for (SymbolId symbol = 0; symbol < manifest_names.size(); ++symbol) {
            to_load.emplace_back(manifest_names[symbol], symbol);
        }
    }

    // Now that every requested symbol is known-valid, actually
    // list segments and prime each cursor.
    cursors_.reserve(to_load.size());
    for (const auto& [name, symbol] : to_load) {
        SymbolCursor cursor;
        cursor.symbol = symbol;

        std::filesystem::path symbol_dir = data_dir / name;
        for (wire::DayKey day = first_day; day <= last_day; ++day) {
            auto day_segments = wire::list_segments(symbol_dir, day);
            cursor.segments.insert(cursor.segments.end(),
                                   std::make_move_iterator(day_segments.begin()),
                                   std::make_move_iterator(day_segments.end()));
        }

        cursor.advance();  // prime `pending` with this symbol's first event, if any
        cursors_.push_back(std::move(cursor));
    }
}

std::optional<MarketEvent> FileReplaySource::next() {
    SymbolCursor* best = nullptr;
    for (auto& cursor : cursors_) {
        if (!cursor.pending) continue;
        if (!best || cursor.pending->ts < best->pending->ts ||
            (cursor.pending->ts == best->pending->ts && cursor.symbol < best->symbol)) {
            best = &cursor;
        }
    }
    if (!best) return std::nullopt;

    MarketEvent event = std::move(*best->pending);
    best->advance();
    return event;
}

bool FileReplaySource::SymbolCursor::fill_more() {
    for (;;) {
        if (!file.is_open()) {
            if (segment_index >= segments.size()) return false;
            file.open(segments[segment_index], std::ios::binary);
            if (!file) {
                throw std::runtime_error("FileReplaySource: failed to open " +
                                         segments[segment_index].string());
            }
            decompressor = std::make_unique<wire::ZstdDecompressor>();
            ++segment_index;
        }

        read_buf.resize(kReadChunk);
        file.read(reinterpret_cast<char*>(read_buf.data()),
                  static_cast<std::streamsize>(read_buf.size()));
        const auto got = file.gcount();
        if (got == 0) {
            file.close();  // this segment exhausted; loop opens the next one
            continue;
        }

        const std::size_t before = decoded_buf.size();
        decompressor->decompress(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(read_buf.data()),
                                       static_cast<std::size_t>(got)),
            decoded_buf);
        if (decoded_buf.size() > before) return true;
        // zstd buffered these bytes internally without emitting output yet
        // (e.g. still inside a frame header) — go around and read more from
        // the same still-open file.
    }
}

bool FileReplaySource::SymbolCursor::advance() {
    for (;;) {
        std::span<const std::byte> view(decoded_buf.data() + consumed,
                                        decoded_buf.size() - consumed);
        if (auto ev = wire::read_event(view)) {
            consumed = static_cast<std::size_t>(view.data() - decoded_buf.data());
            // The directory this cursor is reading from is the source of
            // truth for which symbol this is — stamp it over whatever
            // SymbolId happens to be baked into the record bytes, rather
            // than trusting that the caller's symbol_names ordering here
            // matches whatever ordering was live at record time.
            ev->symbol = symbol;
            pending    = std::move(ev);
            return true;
        }

        // Not enough bytes for a full record yet — compact what's already
        // been consumed, then pull more input.
        if (consumed > 0) {
            decoded_buf.erase(decoded_buf.begin(),
                              decoded_buf.begin() + static_cast<std::ptrdiff_t>(consumed));
            consumed = 0;
        }
        if (!fill_more()) {
            pending.reset();
            return false;
        }
    }
}

}  // namespace qp::source
