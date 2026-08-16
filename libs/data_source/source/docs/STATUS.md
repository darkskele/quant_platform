# source (prod streamer) status

- ~~G1 — Build a live streamer: WebSocket protocol + Binance venue~~
  - ~~G1a — Correct under real network conditions~~
  - ~~G1b — Thread-safe~~
  - G1c — Recorder never backs up the socket read (N/A here — see `apps/collector/docs/STATUS.md`)
  - ~~G1d — Venue isolation~~
  - ~~G1e — Fast parsing (hot-path benchmarked)~~

## Last proof

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
- As of: working tree (uncommitted).

**`venue/binance/` flattened to `venue/`** — same treatment as `protocol/`:
`venue/`'s own `docs/` removed (it's a leaf, single implementation), goals
folded into this town's G1d/G1e above, its decisions folded into this
town's `docs/DECISIONS.md` (one superseded by D16, struck through there
rather than carried forward as live). File names (`binance.hpp`/`.cpp`)
and the `qp/marketdata/venue/binance.hpp` include path unchanged — no
file/class mismatch to fix here, unlike `protocol/`'s `live_ws_source.*`.
- Full rebuild + `ctest` → 7/7 passing.
- TSan clean on `qp_marketdata_integration_tests`/`qp_collector_integration_tests`.
- As of: working tree (uncommitted).

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
- As of: working tree (uncommitted).

Physical restructure (venue moved to `venue/`, websocket transport
moved to `protocol/`) — `qp_marketdata_tests`,
`qp_marketdata_integration_tests`, `qp_venue_tests` all pass unchanged
post-move; TSan clean on `qp_marketdata_integration_tests`. New town-level
aggregate targets `qp_prod_streamer_tests`/`qp_prod_streamer_bench` build
correctly. See D13/D14 in `libs/data_source/source/docs/DECISIONS.md` for the resync
correctness proof (live-testnet, not just unit tests).
- `/test all` → 7/7 passing.
- As of: working tree, post-restructure (root `docs/STATUS.md`).
