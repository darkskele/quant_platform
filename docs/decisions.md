# Decision log

Lightweight ADRs — the decisions made and why. Append, don't rewrite history.

## D1 — Target medium-frequency (MFT), not HFT
Cloud VMs are ms-latency; HFT needs colo + kernel-bypass + FPGAs. Compete on
signal quality + execution discipline + uptime, not tick-to-trade speed.

## D2 — Asset class: crypto perpetuals, Binance USD-M first
Free live L2 + free historical klines/trades/funding, no equities/CME data
licensing, 24/7 markets, cheap VMs near the exchange. Binance chosen for the
largest free dataset and deepest liquidity ("most room to feed the models").

## D3 — Same code in backtest and live (one code path)
Backtest and live are one Engine template with different policy types
(source/clock/execution/sink). Kills backtest-live divergence — the main way
retail quant projects die.

## D4 — Compile-time policies on hot/fixed seams, virtual on cold/runtime seams
Static dispatch: source, clock, execution, recorder. Virtual: strategy, risk.
Justified by cross-frequency-vs-latency-budget, not dogma.

## D5 — Production streamer first; collector is that streamer + a recorder sink
L2 order-book history isn't free and can't be backfilled — but rather than a
throwaway quick collector, build the *real* `LiveWebSocketSource` (socket + book
reconstruction) properly, and get the collector for free by wiring it to a
`FileRecorder` sink (no strategies). Foundation-first: the streamer is needed
anyway, and the collector falls out of it. Data-accumulation urgency is
deliberately deprioritized vs building the base right (the whole project will
take longer than estimated; do the simple, load-bearing parts well first).

## D6 — Strategy sequencing: carry → stat-arb → (factor) → microstructure ML
Increasing sophistication *and* data appetite. Families 1–3 use free data;
family 4 needs the accumulated L2 archive.

## D7 — Capital scale: small (£1–10k), paper-first
Prove determinism + parity + risk layer in paper/testnet before any real
capital. At this scale, cost-per-trade vs edge-per-trade rules strategy choice.

## D8 — Time budget: 5–8 h/week
Horizon ~6 months to live-with-small-real-capital on carry; ML is a 12-month+
arc gated on data accumulation.

## D9 — Dev on WSL Ubuntu, deploy to VM; storage on R2/B2 + local working set
WSL for dev + relative benchmarks; VM for absolute perf + 24/7 collection.
Object storage as cold archive, pull-slice-to-local for backtest.

## ~~D10 — LiveWebSocketSource resync: real concurrency, not an OS-buffer shortcut~~
**Relocated to `libs/data_source/source/docs/DECISIONS.md`** (the town-level "prod
streamer" doc — this is generic resync design, not root-scoped).

## ~~D11 — FileRecorder: local disk only in Phase 0, R2 sync deferred~~
**Relocated to `libs/data_source/sink/docs/DECISIONS.md`** (the "sinks" city doc).

## ~~D12 — Collector/recorder: shared wire format, retry-driven gap alerting, flush-based crash safety~~
**Split and relocated**: wire-format + crash-safety thirds to
`libs/data_source/sink/docs/DECISIONS.md`; gap-alerting third to
`libs/data_source/source/docs/DECISIONS.md` (originally
`protocol/docs/DECISIONS.md`, folded in when `protocol/websocket/`
flattened to `protocol/` and lost its own `docs/`).

## ~~D13 — Resync alignment was using spot's +1 rule, not futures'; found by live smoke-testing~~
**Relocated to `libs/data_source/source/docs/DECISIONS.md`** (the code it's about —
`resync.hpp` — lives there).

## ~~D14 — Resync snapshots are forwarded as a BookSnapshot event, not discarded after alignment~~
**Relocated to `libs/data_source/source/docs/DECISIONS.md`** (same file as D13 —
`resync_coordinator.hpp`).

## D15 — `libs/marketdata`/`libs/record` regrouped as `libs/data_source/{source,sink}`
Reverses the physical (not logical) side of the earlier "source/sinks are
coequal cities, not one combined data source" split: `MarketDataSource` and
`Sink` stay independent seams with independent consumers — that logic didn't
change — but the two now live as sibling directories under `libs/data_source/`
for discoverability, matching how the collector/live/backtest apps are all
"what does the data source connect to" questions. No C++ namespace or CMake
target renamed (`qp::marketdata`, `qp_marketdata_*`, `qp::record`,
`qp_record_*` all unchanged) — purely a directory move plus every path
reference (CMakeLists.txt, docs, `.vscode/`, skill tables) updated to match.
Verified with a full clean rebuild + `ctest` (7/7) + TSan on the two
concurrent integration suites, all unchanged from pre-move.

## D16 — Every lib's `include/` flattened; namespaces and CMake/test targets renamed to match, superseding D15's "unchanged"
D15 deliberately left C++ namespaces and CMake target names alone, moving
only directories. Follow-up push went further: no lib gets a `qp/`-wrapper
folder in its `include/` (every header sits directly in `include/`, included
by bare filename — `"types.hpp"`, `"wire.hpp"`, `"binance.hpp"`, ...), and
every namespace/target name that only matched the *old* `libs/marketdata`/
`libs/record` names now matches the *current* one instead: `qp::record` →
`qp::sink` (`qp_record*` → `qp_sink*`), town-level `libs/data_source/source`
content (`backoff`/`gap_detector`/`parser`/`resync`/`resync_coordinator`/
`source`/`venue_types`) moved from bare `qp` into `qp::source`
(`qp_marketdata_pure`/`qp_marketdata_tests`/`qp_marketdata_bench` →
`qp_source_*`), and the `protocol` village's own library (previously
confusingly named bare `qp_marketdata`, sharing no name with the town's
`_pure`/`_tests` targets) is now `qp_protocol`/`qp_protocol_test_support`/
`qp_protocol_integration_tests`. `venue` (`qp::venue::binance`, `qp_venue*`)
and `collector` (`qp::collector`, `qp_collector*`) already matched and are
unchanged; `core` (bare `qp`, `qp_core*`) is the deliberate exception — it's
the root/lingua-franca namespace, not a domain village. Every `.vscode/`
task/launch entry, the `/test` and `/bench` skill tables, and CMakeLists.txt
comments updated to match. Verified: full rebuild + `ctest`, 7/7 in both
debug and release, plus the combined `qp_bench` binary links clean.

## D17 — `MarketDataSource` concept renamed `Source`
Stuttered as `qp::source::MarketDataSource` once its content lived fully
under the `source` namespace/folder/`source.hpp` file — `Source` matches
this repo's convention of type name mirroring namespace/folder (`sink`'s
`Sink`). Repo-wide references updated (`CLAUDE.md`,
`docs/architecture-principles.md`, `docs/DESIGN.md`, `docs/repo-layout.md`,
and the code itself). Same pass also collapsed `libs/data_source/source`'s
`protocol/` village back into the town directly — town-scoped detail in
`libs/data_source/source/docs/DECISIONS.md` D18.

## D19 — `wire` split out of `sink` into its own lib; `source`/`sink` depend on it instead of on each other
`FileReplaySource`'s read side needs the same wire format/zstd codec/day-
segment naming `FileRecorder` already had, but `source` depending on `sink`
directly would violate "seams depend on core only, never each other."
`wire.hpp`/`zstd_stream.hpp`/`partition.hpp` moved into `libs/data_source/wire`
(`qp::wire`, `qp_wire` target) — core-like substrate scoped to
`data_source`'s two cities, not itself a city. `source` and `sink` both
depend downward on it now. `zstd_stream.hpp` gained `ZstdDecompressor`
(read-side mirror of `ZstdCompressor`) for `FileReplaySource` to use.
