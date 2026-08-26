#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "zstd_stream.hpp"

using qp::wire::ZstdCompressor;
using qp::wire::ZstdDecompressor;

namespace {

std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> out(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) out[i] = static_cast<std::byte>(s[i]);
    return out;
}

std::string to_string(const std::vector<std::byte>& b) {
    std::string out(b.size(), '\0');
    for (std::size_t i = 0; i < b.size(); ++i) out[i] = static_cast<char>(b[i]);
    return out;
}

}  // namespace

TEST(ZstdStream, RoundTripsSingleCompressThenFinish) {
    ZstdCompressor compressor;
    auto           input = to_bytes("the quick brown fox jumps over the lazy dog");

    std::vector<std::byte> compressed;
    compressor.compress(input, compressed);
    compressor.finish(compressed);

    ZstdDecompressor        decompressor;
    std::vector<std::byte> decoded;
    decompressor.decompress(compressed, decoded);

    EXPECT_EQ(to_string(decoded), to_string(input));
}

TEST(ZstdStream, RoundTripsMultipleCompressCallsWithPeriodicFlush) {
    // Mirrors FileRecorder's actual usage: several compress() calls (one per
    // event) interleaved with periodic flush() checkpoints, ended by
    // finish() on close — decompression should reconstruct the exact
    // concatenation regardless of where flush() boundaries fell.
    ZstdCompressor compressor;
    std::vector<std::byte> compressed;
    std::string             expected;

    for (int i = 0; i < 50; ++i) {
        std::string chunk = "event-" + std::to_string(i) + "-payload;";
        expected += chunk;
        compressor.compress(to_bytes(chunk), compressed);
        if (i % 7 == 0) compressor.flush(compressed);
    }
    compressor.finish(compressed);

    ZstdDecompressor        decompressor;
    std::vector<std::byte> decoded;
    decompressor.decompress(compressed, decoded);

    EXPECT_EQ(to_string(decoded), expected);
}

TEST(ZstdStream, DecompressesIncrementallyFedChunksOfCompressedInput) {
    // The exact shape FileReplaySource uses: compressed bytes arrive in
    // arbitrary-sized reads from disk, decompress() is called once per read
    // rather than once for the whole buffer.
    ZstdCompressor compressor;
    std::vector<std::byte> compressed;
    std::string             expected(200'000, 'x');  // large enough to span multiple zstd output blocks
    compressor.compress(to_bytes(expected), compressed);
    compressor.finish(compressed);

    ZstdDecompressor        decompressor;
    std::vector<std::byte> decoded;
    std::span<const std::byte> remaining{compressed};
    constexpr std::size_t       kChunk = 97;  // deliberately not aligned to anything
    while (!remaining.empty()) {
        std::size_t n = std::min(kChunk, remaining.size());
        decompressor.decompress(remaining.first(n), decoded);
        remaining = remaining.subspan(n);
    }

    EXPECT_EQ(to_string(decoded), expected);
}

TEST(ZstdStream, EmptyInputRoundTrips) {
    ZstdCompressor compressor;
    std::vector<std::byte> compressed;
    compressor.finish(compressed);

    ZstdDecompressor        decompressor;
    std::vector<std::byte> decoded;
    decompressor.decompress(compressed, decoded);

    EXPECT_TRUE(decoded.empty());
}
