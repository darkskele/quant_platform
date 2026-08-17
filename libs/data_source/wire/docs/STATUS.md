# wire status

- ~~G1 — Split out of sink so source and sink never depend on each other~~

## Last proof

Moved `wire.hpp`/`partition.hpp` (+ tests, `qp_sink_tests` → `qp_wire_tests`,
`qp_sink_bench` → `qp_wire_bench`) and `zstd_stream.hpp` out of
`libs/data_source/sink` into this lib (`qp::sink` → `qp::wire`, D19). Added
`ZstdDecompressor` (read-side mirror of `ZstdCompressor`) and
`segment_path`/`list_segments` (the day/segment naming convention,
extracted out of `FileRecorder::ensure_open`'s write-side-only loop) for
`FileReplaySource`'s read side. New `test_zstd_stream.cpp` covers the
compressor↔decompressor round trip, including chunked/incremental feed.
- Not rebuilt/tested this pass — pending `/test libs/data_source/wire` (or
  `/test all`) before this is trusted.
- As of: working tree (uncommitted).
