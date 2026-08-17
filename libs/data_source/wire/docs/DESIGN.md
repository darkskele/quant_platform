# wire — the on-disk `MarketEvent` codec

Substrate for **source** and **sink** (root `docs/DESIGN.md`'s two cities),
not a city itself — same relationship `core` has to the whole repo, scoped
instead to just these two. Everything behind turning a `MarketEvent` into
bytes on disk and back: `wire.hpp` (encode/decode), `zstd_stream.hpp`
(compress/decompress), `partition.hpp` (day/segment naming). Pure, no file
I/O — `FileRecorder` (sink) and `FileReplaySource` (source) each do their
own file I/O against these primitives.

## Goals

1. ~~**Split out of `sink` so `source` and `sink` never depend on each
   other**~~ (D19) — `write_event`/`read_event` (D12) plus the day/segment
   naming convention (`segment_path`/`list_segments`) both the write and
   read sides share, and `ZstdCompressor`/`ZstdDecompressor` for the codec.
