# source (prod streamer) decisions

## `Parser` is a C++20 concept, not CRTP
Structural typing gets the same zero-cost static dispatch as CRTP with
better error messages, and this project's own convention already prefers
concepts on hot/fixed seams (`CLAUDE.md`). `GenericLiveWebSocketSource<P>`
is templated on it; `venue::binance::BinanceParser` is the sole
implementation today.

## D10 — LiveWebSocketSource resync: real concurrency, not an OS-buffer shortcut
*(relocated from root `docs/decisions.md`, struck through there)*

Binance's documented procedure (buffer diffs, fetch REST snapshot, align,
replay) needs true concurrent buffering — a "connect then immediately block
on the REST call" shortcut risks silently missing diffs. Two threads (WS I/O,
dedicated REST-resync), coordinated via two SpscQueues; no third "coordinator"
thread — that logic runs on the I/O thread. Per-symbol, not connection-wide:
one symbol resyncing doesn't pause the others. We can't do full ULL (no
exotic hardware), but SPSC-queue-coordinated threads is the correctness-grade
concurrency architecture-principles.md already calls for.

## D13 — Resync alignment was using spot's +1 rule, not futures'; found by live smoke-testing
*(relocated from root `docs/decisions.md`, struck through there)*

`find_resync_point` checked `U <= lastUpdateId+1 <= u` — Binance's *spot*
convention. This project is USD-M futures only, where the documented rule is
`U <= lastUpdateId <= u`, no offset. The wrong offset made every mock/
synthetic test pass while resync failed 100% of the time against real
testnet data — real snapshots land exactly on a buffered event's `seq` far
more often than chance (Binance's snapshot generation tracks the diff
stream's own sequencing closely), and the +1 slack turned that valid
boundary into a false "hole." Caught only by actually running the packaged
collector against real Binance testnet, not by any hand-constructed test
fixture. Root-caused with temporary diagnostic logging comparing
`lastUpdateId` against the buffer's real range live, then fixed by dropping
the offset; affected tests audited and two rewritten around the real
boundary. Lesson: hand-built fixtures can systematically dodge the exact
edge real data hits — a live smoke test against the actual venue found what
extensive mock-based testing didn't.

## D14 — Resync snapshots are forwarded as a BookSnapshot event, not discarded after alignment
*(relocated from root `docs/decisions.md`, struck through there)*

`ResyncCoordinator::on_snapshot` now prepends the REST snapshot's own
bids/asks as a new `EventKind::BookSnapshot` event before the replayed
diffs, instead of dropping them once used for alignment — a recording of
diffs alone has no independent baseline. Reuses `MarketEvent`'s existing
generic bids/asks fields, no wire-format change. Rejected an implicit
"first diff after resync = full book" convention: reintroduces D13-style
implicit-state fragility. Building this, the new event initially left `.ts`
unset (defaulting to 0); `FileRecorder` partitions by `event.ts`, so every
resync silently wrote a stray `1970-01-01` partition file. Only caught by a
live-testnet run inspecting the actual data directory — the 9 updated test
assertions all passed regardless. Fixed by borrowing the first replayed
diff's `.ts`. Same lesson as D13: live smoke-test the resync/recording path,
mocks alone miss this class of bug.

## D16 — `SymbolTable`/`WsEndpoint`/`RestEndpoint`/`DepthSnapshot` moved out of `venue::binance`
Finishes the genericization `parser.hpp`'s own comment deferred: these 4
types had zero Binance-specific content already (pure string<->id interning,
plain host/port/base-url structs, `{last_update_id, bids, asks}`) — they were
venue::binance-namespaced only because no second venue existed to design a
generic home against. Now in `qp/marketdata/venue_types.hpp`, plain `qp::`
types; `venue::binance::` keeps `using` aliases to them so `binance.hpp`,
`binance.cpp`, and their tests read unchanged. `Parser`'s `requires`-clause
and `GenericLiveWebSocketSource`'s constructor/member types now name these
directly — zero `venue::binance::` left in either, completing G2's stated
goal for this file too (it only fully applied to the resync/gap-detection
files before). `qp_venue` picked up a new `qp_marketdata_pure` PUBLIC
dependency (binance.hpp's public header now names `venue_types.hpp`'s
types). Verified: full rebuild + `ctest` (7/7) + TSan on both concurrent
integration suites, all unchanged from before the move.

## D17 — Every `include/` is flat now; namespaces and build targets renamed to match
`qp/marketdata/...` never corresponded to an actual namespace (town-level
content was always plain `qp`), and even after dropping just the
`marketdata` segment the remaining `qp/` wrapper (and `qp/protocol/`,
`qp/venue/`) still bought nothing — every lib already gets its own
`include/` directory on the compiler's search path via
`target_include_directories`, so the wrapper folder was pure ceremony, not
a real namespace boundary. Every header in this town (and its `protocol`/
`venue` villages) now sits directly in its lib's `include/`, included by
bare filename (`"types.hpp"`, `"wire.hpp"`, `"binance.hpp"`,
`"live_websocket_source.hpp"`, ...) — same treatment applied repo-wide,
including `core` and `sink`.

Taken further than the physical move: this town's own pure content
(`backoff`/`gap_detector`/`parser`/`resync`/`resync_coordinator`/`source`/
`venue_types`) moved out of bare `qp` into `qp::source`, matching the
folder it's always lived in. `protocol/`'s content was already `qp::protocol`
(from an earlier pass); `venue` was already `qp::venue::binance` — neither
needed to move. Cross-namespace references fixed up: `venue/binance.hpp`'s
`using SymbolTable = qp::SymbolTable;`-style aliases now point at
`qp::source::*`; `protocol/live_websocket_source.hpp`/`.cpp` qualify
`Parser`/`WsEndpoint`/`RestEndpoint`/`SymbolTable`/`ResyncCoordinator`/
`DepthSnapshot`/`ExponentialBackoff` as `source::*` throughout (found via
enclosing-namespace lookup from `qp::protocol`, same mechanism as
`venue::binance::*`); `apps/collector`'s `Config` does the same. CMake/test
targets renamed to match: `qp_marketdata_pure`/`_tests`/`_bench` →
`qp_source_*`; the `protocol` village's library (previously bare
`qp_marketdata`, confusingly sharing no name with the town's own `_pure`/
`_tests` targets) → `qp_protocol`/`qp_protocol_test_support`/
`qp_protocol_integration_tests`; `sink`'s `qp::record`/`qp_record_*` →
`qp::sink`/`qp_sink_*` (same reasoning, done at the same time). `venue`
(`qp::venue::binance`, `qp_venue*`) and `collector` (`qp::collector`,
`qp_collector*`) already matched and are untouched; `core` stays bare `qp`
deliberately — it's the root namespace, not a domain village. No filename
collisions across libs (checked). Verified: full rebuild + `ctest`, 7/7 in
both debug and release (core, source, venue, protocol, sink, collector),
plus the combined `qp_bench` binary links clean.

## Gap-alerting metric choice — split from D12
*(relocated from `protocol/docs/DECISIONS.md` when `protocol/websocket/`
flattened to `protocol/` and lost its own `docs/` — see this town's G1c)*

Alerting polls `resync_retry_count()`, not `gap_count()`/`resync_count()` —
a gap that buffers and cleanly resyncs on the first attempt never moves it,
so the expected/self-healing case (guaranteed once per symbol per
connection, and again every reconnect — Binance caps connections at 24h)
doesn't cry wolf. `resync_retry_count()` only moves when a round had to
restart, a genuine "not self-healing" signal, off metrics
`GenericLiveWebSocketSource` already exposes — no new per-symbol timestamp
tracking added. The per-symbol-day manifest sidecar (`libs/data_source/sink`)
still logs every gap/resync/retry unfiltered regardless of what alerts,
since there's no measured gap-rate data yet; thresholds get tuned from real
data later, not guessed now.

## `BinanceParser` thin-wraps the existing free functions, not a rewrite
*(relocated from `venue/docs/DECISIONS.md` when `venue/binance/`
flattened to `venue/` and lost its own `docs/` — see this town's G1d)*

`build_stream_path`/`parse_message`/`depth_snapshot_url`/`parse_depth_snapshot`
already existed and were already tested; `BinanceParser` is a static-method
struct that forwards to them (fully-qualified, to avoid self-recursion since
method names match) so the `Parser` concept has something to bind to,
without touching proven code.

## ~~`DepthSnapshot`/`SymbolTable`/`RestEndpoint` stay `venue::binance`-typed~~
**Superseded by D16** — moved to generic `qp::` types in `venue_types.hpp`
to unblock `apps/collector`'s venue-selection macro (D17), which needed
`Config`/`Parser` to stop naming a venue at all; `venue::binance::` now
just aliases them.
