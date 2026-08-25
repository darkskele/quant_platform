# sink (sinks) decisions

## D11 — FileRecorder: local disk only in Phase 0, R2 sync deferred
*(relocated from root `docs/decisions.md`, struck through there)*

Storage capacity was never the constraint (50GB laptop headroom ≈ 2-7 weeks
of BTCUSDT+ETHUSDT compressed depth+trades) — uptime is, and that's a VM
problem, not a disk problem. So `FileRecorder` writes local compressed/
partitioned files only; no cloud SDK/credentials in `libs/data_source/sink`. Syncing to
R2 is deferred, not rejected — an ops-level job (e.g. rclone) to add later,
independent of the recorder/collector design.

## D12 (wire-format + crash-safety thirds) — split from root `docs/decisions.md`, see also `libs/data_source/source/protocol/docs/DECISIONS.md` for the gap-alerting third

~~**Wire format** (`write_event`/`read_event`) lives in `libs/data_source/sink`...~~
**Further relocated to `libs/data_source/wire/docs/DECISIONS.md` by D19**
(root `docs/decisions.md`) — the format itself now lives in its own lib so
`source` and `sink` don't depend on each other for it.

## D21 — `Sink` concept added, mirroring `source`'s `Source`

`FanoutSink` (in-process fan-out via `SpmcRing`) gave this lib a second
`record(MarketEvent)` implementation alongside `FileRecorder` — nothing
formalized that the two share a shape. `sink.hpp` adds a `Sink` concept
(`record(MarketEvent) -> void`), compile-time like `Source`. `apps/collector`
now takes its recorder as `sink::Sink auto` instead of naming
`FileRecorder` concretely.

**Crash safety** (still this lib's own concern — `FileRecorder`'s flush
cadence, not the format): periodic `ZSTD_e_flush` (not a full frame close) every ~1s
or K events bounds data loss to that window without discarding the
compressor's window/history. Combined with the wire format's truncated-tail
tolerance, a crash mid-write leaves a still-fully-readable file up to the
last flush.

## D43 — `MarketEvent` gains a `venue` field, stamped by `run_data_source`, not by any `Sink`
The `SymbolId` collision problem flagged in `apps/collector/docs/DECISIONS.md`
D41 (two venues each intern "BTCUSDT" to `SymbolId 0`, colliding once read
into one `Portfolio`) needed a real fix before `FundingCarryStrategy` could
hold spot and perp positions independently. Explored and rejected, in
order: a shared, qualified-string `SymbolTable` (per-message allocation on
a path that's deliberately allocation-free today); `OffsetTransport<Tx,
Offset>` (correct, but needed hand-picked non-overlapping offset ranges, a
caller invariant the type system couldn't check — see
`libs/data_source/transport/docs/DECISIONS.md`'s struck D42);
`FanoutSink<Capacity, NumConsumers, NumSources>` with a `record<I>` per
venue plus a matching `IndexedFileRecorders` and a new `IndexedSink`
concept to formalize the shape — correct too, but real machinery (a second
`Sink`-like concept, multi-ring/multi-recorder variants of both `Sink`
implementations) for a problem that didn't need any of it.

Resolved with the simplest version: `MarketEvent` gains `VenueId venue{}`
(`libs/core/include/types.hpp`, a plain `uint8_t`, meaningless on its own
like `SymbolId`) — `(symbol, venue)` together identify an instrument,
`symbol` alone doesn't — and it's stamped by `run_data_source`
(`libs/data_source/include/run_data_source.hpp`, D44), the driver that
already pairs a `Source` at index `i` with a `Sink` at index `i`. Since the
driver already knows `i` from the same fold expansion that does the
pairing, it just sets `event.venue = i` before calling `sink.record(event)`
— the *existing* `Sink::record(event)`, unchanged. `FanoutSink` and
`FileRecorder` need no changes at all: both already satisfied `Sink`
before any of this, and still do. No new concept, no multi-ring/multi-
recorder variants — a multi-venue setup is just N ordinary `FanoutSink`s
(or `FileRecorder`s) paired with N sources by the driver, the same way
`InProcessTransport`+`CombinedTransport` already pairs N rings on the read
side.

`test_wire.cpp` gained a non-zero-venue round-trip case — every other
round-trip test uses the default (0), which wouldn't catch a read path
that silently drops the byte.

## D13 — `qp::record`/`qp_record_*` renamed to `qp::sink`/`qp_sink_*`
Namespace and CMake/test targets both said "record" while every folder
(`libs/data_source/sink`, this doc's own directory) has said "sink" since
the D15 physical restructure (root `docs/decisions.md`) — that move
deliberately left names alone, but a later pass (source town's D17,
`libs/data_source/source/docs/DECISIONS.md`) renamed every remaining
name-vs-folder mismatch repo-wide, this one included. Every `qp::record::`
call site, `using`-declaration, and CMake target
(`qp_record`/`_pure`/`_tests`/`_integration_tests`/`_bench`) renamed to
`qp_sink`/`qp::sink`. Verified: full rebuild + `ctest`, `qp_sink_tests`/
`qp_sink_integration_tests` both green in debug and release.
