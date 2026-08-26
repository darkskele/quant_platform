#pragma once
#include <zstd.h>

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace qp::wire {

// Thin RAII wrapper around zstd's streaming compression API — not a
// general-purpose binding, just the three operations FileRecorder needs:
//
//   compress()  ZSTD_e_continue — buffer/compress a chunk, no guarantee
//               it's flushed to `out` yet (zstd may hold it internally).
//   flush()     ZSTD_e_flush — force everything buffered out as an
//               independently-decodable point, WITHOUT ending the frame or
//               discarding the compression window/history. This is the
//               crash-safety mechanism (docs/decisions.md D12): called
//               periodically, it bounds how much a crash can lose without
//               sacrificing the compression ratio a full frame restart
//               would cost.
//   finish()    ZSTD_e_end — end the frame. Only on clean rotation/close.
//
// Uses the modern ZSTD_CCtx streaming API (ZSTD_CStream is just a
// compatibility alias for the same type) — ZSTD_createCStream/initCStream
// still work but are the deprecated simple API.
class ZstdCompressor {
   public:
    explicit ZstdCompressor(int level = 3) : ctx_(ZSTD_createCCtx()) {
        if (!ctx_) throw std::runtime_error("ZSTD_createCCtx failed");
        std::size_t rc = ZSTD_CCtx_setParameter(ctx_, ZSTD_c_compressionLevel, level);
        if (ZSTD_isError(rc)) throw std::runtime_error(ZSTD_getErrorName(rc));
    }

    ~ZstdCompressor() {
        if (ctx_) ZSTD_freeCCtx(ctx_);
    }

    ZstdCompressor(const ZstdCompressor&)            = delete;
    ZstdCompressor& operator=(const ZstdCompressor&) = delete;
    ZstdCompressor(ZstdCompressor&&)                 = delete;
    ZstdCompressor& operator=(ZstdCompressor&&)      = delete;

    void compress(std::span<const std::byte> input, std::vector<std::byte>& out) {
        drive(input, out, ZSTD_e_continue);
    }

    void flush(std::vector<std::byte>& out) { drive({}, out, ZSTD_e_flush); }

    void finish(std::vector<std::byte>& out) { drive({}, out, ZSTD_e_end); }

   private:
    void drive(std::span<const std::byte> input, std::vector<std::byte>& out,
               ZSTD_EndDirective mode) {
        ZSTD_inBuffer                    in{input.data(), input.size(), 0};
        std::array<std::byte, 64 * 1024> buf;

        for (;;) {
            ZSTD_outBuffer zout{buf.data(), buf.size(), 0};
            std::size_t    ret = ZSTD_compressStream2(ctx_, &zout, &in, mode);
            if (ZSTD_isError(ret)) throw std::runtime_error(ZSTD_getErrorName(ret));
            out.insert(out.end(), buf.data(), buf.data() + zout.pos);

            // ZSTD_e_continue: done once all input's been handed in (zstd
            // may still be holding some of it internally — that's fine,
            // nothing's lost, it just isn't a decodable checkpoint yet).
            // ZSTD_e_flush/ZSTD_e_end: `ret` is bytes still owed; 0 means
            // fully flushed/frame ended, per zstd's documented contract.
            const bool done = (mode == ZSTD_e_continue) ? (in.pos == in.size) : (ret == 0);
            if (done) break;
        }
    }

    ZSTD_CCtx* ctx_;
};

// The read-side mirror of ZstdCompressor: streaming decompression, driven
// by whatever chunks of a `.bin.zst` file the caller hands in (FileRecorder
// writes each segment as file bytes arrive on disk; FileReplaySource reads
// them back the same way — neither side needs the whole file in memory at
// once). A single instance decodes exactly one zstd frame's worth of
// history; a fresh instance per segment file mirrors FileRecorder opening a
// fresh ZstdCompressor (and frame) per segment (file_recorder.cpp's
// ensure_open) — there's no cross-segment state to carry.
class ZstdDecompressor {
   public:
    ZstdDecompressor() : ctx_(ZSTD_createDCtx()) {
        if (!ctx_) throw std::runtime_error("ZSTD_createDCtx failed");
    }

    ~ZstdDecompressor() {
        if (ctx_) ZSTD_freeDCtx(ctx_);
    }

    ZstdDecompressor(const ZstdDecompressor&)            = delete;
    ZstdDecompressor& operator=(const ZstdDecompressor&) = delete;
    ZstdDecompressor(ZstdDecompressor&&)                 = delete;
    ZstdDecompressor& operator=(ZstdDecompressor&&)      = delete;

    // Decodes as much of `input` as currently yields output, appending
    // decoded bytes to `out`. Safe to call repeatedly with successive
    // chunks of the same frame — zstd's streaming decoder carries state
    // (window/history) across calls, same as ZstdCompressor::compress does
    // on the write side. A partial trailing record left in `out` after the
    // last call (e.g. mid-write crash, flush()'d but not finish()'d) is
    // exactly wire::read_event's truncated-tail case, not this class's
    // concern to detect.
    void decompress(std::span<const std::byte> input, std::vector<std::byte>& out) {
        ZSTD_inBuffer                    in{input.data(), input.size(), 0};
        std::array<std::byte, 64 * 1024> buf;

        while (in.pos < in.size) {
            ZSTD_outBuffer zout{buf.data(), buf.size(), 0};
            std::size_t    ret = ZSTD_decompressStream(ctx_, &zout, &in);
            if (ZSTD_isError(ret)) throw std::runtime_error(ZSTD_getErrorName(ret));
            out.insert(out.end(), buf.data(), buf.data() + zout.pos);
            // ret == 0 means a frame just ended. FileRecorder writes one
            // frame per segment file, so normally that coincides with
            // `input` running out too; if trailing bytes remain regardless,
            // looping continues to feed them into the same call (a second
            // frame in one file never happens today, but nothing here
            // assumes otherwise).
        }
    }

   private:
    ZSTD_DCtx* ctx_;
};

}  // namespace qp::wire
