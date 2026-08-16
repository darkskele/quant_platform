# sink — the "sinks" city

Coequal with **source** (root `docs/DESIGN.md`). Everything behind
`Sink`/`Recorder`: wire format, `FileRecorder`, partitioning, zstd. One
`Sink` implementation — no further nesting yet.

## Goals

1. ~~**Build the FileRecorder sink**~~ — binary/zstd/partitioned writer,
   shared wire format (`write_event`/`read_event`, reused by future
   `FileReplaySource`), crash-safe, never blocks the caller (own thread,
   SPSC queue).
