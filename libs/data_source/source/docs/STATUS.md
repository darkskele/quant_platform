# source (prod streamer) status

- ~~G1 — Build a live streamer: WebSocket protocol + Binance venue~~
  - ~~G1a — Correct under real network conditions~~
  - ~~G1b — Thread-safe~~
  - G1c — Recorder never backs up the socket read (N/A here — see `apps/collector/docs/STATUS.md`)
  - ~~G1d — Venue isolation~~
  - ~~G1e — Fast parsing (hot-path benchmarked)~~
- ~~G2 — Build `FileReplaySource`: the backtest `Source`~~

## Last proof

**`symbols.manifest` added; `FileReplaySource` reads it instead of taking
its own symbol list** (D20) — closes a real correctness gap: two
independently-supplied `symbol_names` orderings (one to `FileRecorder`, one
to `FileReplaySource`) had nothing enforcing they agreed, so a replay using
a different order than record time silently mislabeled every event's
`SymbolId`. `FileRecorder` now writes `data_dir/symbols.manifest` at
construction; `FileReplaySource`'s constructor is now `(data_dir,
first_day, last_day, wanted = nullopt)`, reading that file as the sole
canonical mapping, with `wanted` optionally filtering to a subset (still
the manifest's global `SymbolId`, never renumbered). Every `wanted` name is
validated against the manifest — throwing on the first miss — *before* any
segment file is touched, so a bad name doesn't pay for however many other
(possibly large) symbols were already read. Also fixed: `advance()` now
stamps the cursor's own directory-derived `SymbolId` onto every returned
event instead of trusting whatever value happened to be baked into the
record bytes — the directory a cursor reads from is the source of truth,
not the file's own content (new regression test:
`ReturnedSymbolIdIsTheReplayDirectorysNotTheRecordedByte` →
`WantedFilterPreservesManifestSymbolIdNotRenumbered`/
`ThrowsWhenWantedSymbolIsNotInManifest`/`ThrowsWhenManifestIsMissing`).
Also: `wire.hpp`'s `read_levels` had a latent null-pointer `memcpy` on
empty bids/asks (Trade/Funding events), UB-caught once this path actually
ran under UBSan — fixed.

Test/bench event-builder duplication (`book_diff`/`trade`/`funding`, 5
near-identical copies across sink/source/wire/tests) and the `ScratchDir`
temp-dir helper (5 copies) both consolidated into `libs/core/tests/support/`
(`qp_core_test_support`) — see `libs/core/docs/STATUS.md`.

**`FileReplaySource` built** (G2) — reads `FileRecorder`'s on-disk segments
back via the shared wire codec (`libs/data_source/wire`'s `read_event`/
`list_segments`/`ZstdDecompressor`, D19), one `SymbolCursor` per requested
symbol decompressing/parsing on demand, k-way merged by `MarketEvent::ts`
(linear scan, `SymbolId` tiebreak) into a single `next()` stream.
Single-threaded/synchronous — no writer thread/SPSC queue, unlike
`FileRecorder`/`GenericLiveWebSocketSource` — a backtest's only consumer is
the Engine's own loop, so there's no live socket to protect from
backpressure. New CMake target `qp_file_replay` (unconditional, no Boost/
OpenSSL) and `qp_file_replay_tests` (`tests/test_file_replay_source.cpp`):
single/multi-segment, multi-symbol timestamp merge + tiebreak, multi-day
range filtering, missing-symbol-data, and a 20k-event case exercising the
internal read-chunk refill path. `qp_file_replay_bench` added
(`benchmarks/bench_file_replay_source.cpp`): single-symbol BookDiff replay
(realistic, level-heavy), single-symbol Trade replay (per-event/merge
overhead isolated from level-copy cost), and a 4-symbol interleaved merge
to isolate k-way-merge cost from the single-cursor case. Cross-lib
round-trip proof against a real `FileRecorder` (this town's own tests can't
provide that — source/sink never depend on each other) lives in
`tests/docs/STATUS.md` (`qp_parity_tests`).

**`protocol/` collapsed into this town directly** (D18) — the only
transport village, folded up now that `FileReplaySource` (Phase 1) is about
to become a second `Source` living directly here too, with no `Parser` and
no reason for a `protocol/`-shaped home. `live_websocket_source.hpp`/`.cpp`
and its tests/`support/` moved to this town's own `include/`/`src/`/`tests/`;
`qp::protocol` merged into `qp::source` (the `source::` qualifications the
file already used throughout became self-references, stripped).
`MarketDataSource` (`source.hpp`) renamed to `Source` — no longer stutters
under the merged namespace/folder/file. CMake targets: `qp_protocol` →
`qp_source`, `qp_protocol_test_support` → `qp_source_test_support`,
`qp_protocol_integration_tests` → `qp_source_integration_tests`. Every
reference updated repo-wide (`apps/collector`, root `CMakeLists.txt`,
`.vscode/`, `/test` skill table, `docs/architecture-principles.md`,
`docs/repo-layout.md`, root `docs/DESIGN.md`, `CLAUDE.md`).
- Mechanical rename/move only, no logic or test-assertion changes.
- `qp_source_integration_tests`/`qp_venue_tests` pass.

**Every `include/` flattened; namespaces/build targets renamed to match**
(D17) — headers sit directly in each lib's `include/`, included by bare
filename (`"types.hpp"`, `"wire.hpp"`, `"binance.hpp"`,
`"live_websocket_source.hpp"`, ...), repo-wide (`core`, `sink`, this town,
its `protocol`/`venue` villages). This town's own content moved from bare
`qp` into `qp::source`; `sink` moved `qp::record` into `qp::sink`;
`protocol`/`venue` already matched and are unchanged. CMake/test targets
renamed to match current names: `qp_marketdata_pure`/`_tests`/`_bench` →
`qp_source_*`, `protocol`'s own library `qp_marketdata` → `qp_protocol`
(+ `_test_support`/`_integration_tests`), `qp_record_*` → `qp_sink_*`. See
D17.
- Full rebuild + `ctest` → 7/7 passing in both debug and release (core,
  source, venue, protocol, sink, collector); `qp_bench` links clean.
  TSan not re-run this pass.

**`venue/binance/` flattened to `venue/`** — same treatment as `protocol/`:
`venue/`'s own `docs/` removed (it's a leaf, single implementation), goals
folded into this town's G1d/G1e above, its decisions folded into this
town's `docs/DECISIONS.md` (one superseded by D16, struck through there
rather than carried forward as live). File names (`binance.hpp`/`.cpp`)
and the `qp/marketdata/venue/binance.hpp` include path unchanged — no
file/class mismatch to fix here, unlike `protocol/`'s `live_ws_source.*`.
- Full rebuild + `ctest` → 7/7 passing.
- TSan clean on `qp_marketdata_integration_tests`/`qp_collector_integration_tests`.

**`protocol/websocket/` flattened to `protocol/`** — file/class-name
mismatch fixed (`live_ws_source.*` → `live_websocket_source.*`, matching
`GenericLiveWebSocketSource`); include path now
`qp/marketdata/protocol/live_websocket_source.hpp`. `protocol/`'s own
`docs/` removed — it's a leaf now (single implementation, no further
nesting), its goals folded into this town's G1 sub-goals above, its one
decision (gap-alerting metric choice) folded into this town's
`docs/DECISIONS.md`.
- Full rebuild + `ctest` → 7/7 passing.
- TSan clean on `qp_marketdata_integration_tests`/`qp_collector_integration_tests`.

Physical restructure (venue moved to `venue/`, websocket transport
moved to `protocol/`) — `qp_marketdata_tests`,
`qp_marketdata_integration_tests`, `qp_venue_tests` all pass unchanged
post-move; TSan clean on `qp_marketdata_integration_tests`. New town-level
aggregate targets `qp_prod_streamer_tests`/`qp_prod_streamer_bench` build
correctly. See D13/D14 in `libs/data_source/source/docs/DECISIONS.md` for the resync
correctness proof (live-testnet, not just unit tests).
- `/test all` → 7/7 passing.
