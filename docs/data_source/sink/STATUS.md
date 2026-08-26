# sink status

- ~~G1 — Build the FileRecorder sink~~
- ~~G2 — Build FanoutSink~~

## Last proof

`FanoutSink` (`libs/data_source/sink/include/fanout_sink.hpp`) added:
same `record(MarketEvent)` call shape as `FileRecorder`, but pushes onto a
`SpmcRing<shared_ptr<const MarketEvent>, Capacity, NumConsumers>`
(`libs/core`) instead of writing to disk — `attach()` hands out cursor ids,
consumer side is deliberately not built here (kept out of `sink` so
`source`/`sink` stay independent, D19). New `Sink` concept (`sink.hpp`,
D21) formalizes `record(MarketEvent) -> void` as the shape both
`FileRecorder` and `FanoutSink` satisfy; `apps/collector` now declares its
recorder as `sink::Sink auto`. `SpmcRing` gained a compile-time
`try_pop<Consumer>()` alongside the existing runtime `try_pop(size_t)` for
this use (see `libs/core/docs/STATUS.md`).

`FileRecorder` now writes `data_dir/symbols.manifest` at construction
(plain text, one symbol name per line, index == `SymbolId`) — the
canonical symbol list `FileReplaySource` reads back instead of taking its
own, independently-supplied one (D20). `qp_sink_integration_tests`
unaffected (existing tests don't assert on the manifest directly; the
cross-lib round trip that relies on it is proven in `tests/docs/STATUS.md`).

Wire codec (encode/decode, zstd, day/segment naming) moved to
`libs/data_source/wire` (D19) — this lib now depends on it instead of
owning it; `qp_sink_bench`/`qp_sink_tests` moved with it to
`qp_wire_bench`/`qp_wire_tests`. `qp_sink_integration_tests` (`FileRecorder`
itself, still here) unchanged in behavior, only in what it links against.
