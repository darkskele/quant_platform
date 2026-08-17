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

**Crash safety** (still this lib's own concern — `FileRecorder`'s flush
cadence, not the format): periodic `ZSTD_e_flush` (not a full frame close) every ~1s
or K events bounds data loss to that window without discarding the
compressor's window/history. Combined with the wire format's truncated-tail
tolerance, a crash mid-write leaves a still-fully-readable file up to the
last flush.

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
