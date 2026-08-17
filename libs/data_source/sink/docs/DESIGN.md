# sink — the "sinks" city

Coequal with **source** (root `docs/DESIGN.md`). Everything behind
`Sink`/`Recorder`: `FileRecorder`, day-partition rotation, crash-safe flush
cadence. The wire codec itself (encode/decode, zstd, day/segment naming)
moved to `libs/data_source/wire` (D19) — this lib depends on it rather than
owning it. One `Sink` implementation — no further nesting yet.

## Goals

1. ~~**Build the FileRecorder sink**~~ — binary/zstd/partitioned writer over
   the shared wire codec (`libs/data_source/wire`, also used by
   `FileReplaySource`), crash-safe, never blocks the caller (own thread,
   SPSC queue).
