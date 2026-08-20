# sink — the "sinks" city

Coequal with **source** (root `docs/DESIGN.md`). Everything behind
`Sink`/`Recorder`: `FileRecorder`, day-partition rotation, crash-safe flush
cadence, and the top-level `symbols.manifest` (D20) — the canonical symbol
list `FileReplaySource` reads back. The wire codec itself (encode/decode,
zstd, day/segment naming) moved to `libs/data_source/wire` (D19) — this lib
depends on it rather than owning it. `Sink` (`sink.hpp`) is the concept
both implementations satisfy — mirrors `source`'s `Source` concept
(D21) — so callers like `apps/collector` can hold `sink::Sink auto`
instead of a concrete type. Two `Sink` implementations now, no further
nesting.

## Goals

1. ~~**Build the FileRecorder sink**~~ — binary/zstd/partitioned writer over
   the shared wire codec (`libs/data_source/wire`, also used by
   `FileReplaySource`), crash-safe, never blocks the caller (own thread,
   SPSC queue).
2. ~~**Build `FanoutSink`**~~ — in-process fan-out counterpart to
   `FileRecorder`: redistributes to `NumConsumers` readers via
   `SpmcRing<shared_ptr<const MarketEvent>>` (`libs/core`) instead of
   writing to disk.
