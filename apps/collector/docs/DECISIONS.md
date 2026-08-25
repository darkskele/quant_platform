# collector decisions

## D17 — Venue picked at build time via `QP_COLLECTOR_VENUE`, not runtime
A CMake cache option (default `binance`) sets `QP_VENUE_<NAME>`, which
`venue.hpp` — the one file allowed to name a concrete venue — dispatches on
via `#if`/`#error` to expose generic names (`SelectedParser`,
`kDefaultWsEndpoint`, etc.) that `collector.hpp`/`.cpp` consume without ever
naming a venue themselves. This is *not* the "never fork logic on a macro"
CLAUDE.md rule biting — that rule targets shared/strategy logic forking
behavior; this macro only selects which concrete types get named at the
composition root, the one place `apps/*` is explicitly allowed to do that.
An unsupported `QP_COLLECTOR_VENUE` value fails the build immediately and
loudly (`#error`), not silently — verified directly (`-DQP_COLLECTOR_VENUE=
kraken` fails at `venue.hpp` with a clear message, not a mysterious
downstream error). Binance is the only real branch today; a second venue
adds a `STRINGS` entry (CMakeLists.txt) and an `#elif` branch (venue.hpp) —
nothing in collector.hpp/.cpp changes. Depends on D16 (venue-agnostic
`WsEndpoint`/`RestEndpoint` types) — without that, `Config` would still be
forced to name `venue::binance::` regardless of this macro.

## D41 — Collector always records two legs (futures + spot), each fully independent
Funding-carry needs both legs' data. `run()` now unconditionally builds two
`Leg<Parser, Rule>` instances (`venue.hpp`'s `SelectedParser`/`SelectedAlignment`
for futures, new `SelectedSpotParser`/`SelectedSpotAlignment` for spot,
libs/data_source/source/docs/DECISIONS.md D40) instead of one — same
`config.symbols` list applies to both (carry's actual need: matching
underlying instruments on both markets), each leg gets its own `WsEndpoint`/
`RestEndpoint` in `Config` (`spot_ws_endpoint`/`spot_rest_endpoint`,
alongside the existing futures fields).

Each leg's `GenericLiveWebSocketSource` + `FileRecorder` pair is fully
independent — own `SymbolTable`, own poll loop, own recording — and each
`FileRecorder` writes into `data_dir/<leg>/` (`futures/`, `spot/`), not
`data_dir` directly: `FileRecorder` unconditionally writes
`data_dir/symbols.manifest` (D20), so two recorders sharing one `data_dir`
would clobber each other's manifest. Subdirectories avoid that with no new
required CLI flag — `--data-dir` still names one root.

Deliberately NOT merged into one `SymbolTable`/one output stream here:
both venues independently interning "BTCUSDT" would collide into the same
`SymbolId` if fed into a shared `Portfolio` — a real problem, but only once
something reads *both* legs into one `Engine` (a backtest/live wiring
concern via `CombinedTransport`, `libs/data_source/transport`), not a
collector one. Keeping each leg's `SymbolTable`/manifest/recording fully
separate here is exactly what avoids that collision ever arising in this
file.

~~The single poll loop (`Leg::poll()` called for both legs every
iteration, sleeping only when neither had anything) needed no new
threading: each `GenericLiveWebSocketSource` already runs its own I/O +
resync threads (D10), so `source.next()` was already a cheap non-blocking
poll on either leg.~~ **Superseded by D42** — the hand-written `Leg` struct
and its poll loop were replaced once `run_data_source` existed generically.

## D42 — `run()` adopts `run_data_source` instead of its own hand-written poll loop
The `Leg<Parser, Rule>` struct (D41) — one leg's `GenericLiveWebSocketSource`
+ `FileRecorder` pair, plus a hand-written `poll()` calling `source.next()`
then `recorder.record(...)` for both legs every iteration — was exactly the
pattern `run_data_source` (`libs/data_source`, D44) was built to generalize:
pair N sources with N sinks positionally, without a human writing out the
pairing by hand. Adopted here once that existed.

`Leg` is gone. `run()` now constructs each leg's `GenericLiveWebSocketSource`/
`FileRecorder` as plain locals (unchanged shape otherwise — same
constructor args, same `data_dir/<leg>/` subdirectory split from D41),
bundles them into two `std::tie`'d tuples (never owning/moving/copying
either type — both are non-movable), and hands them to `run_data_source`
running on its own thread.

That thread ownership split is the real design point: `run_data_source` is
generic over any `Source`/`Sink` pair and deliberately knows nothing about
`GenericLiveWebSocketSource`'s gap/resync counters or `FileRecorder`'s
drop/queue counters — the periodic status line and `--duration`/
`stop_requested` handling stay collector's own concern, now running on
`run()`'s own thread instead of interleaved into the same loop that used
to also do the polling. `run()`'s loop is simpler as a result: check the
deadline/stop flag, sleep briefly, occasionally log — it does no event
handling itself anymore. Shutdown: `run()` flips `running` to false and
`.join()`s the driver thread before returning, so the source/recorder
locals are never destroyed while the driver could still be touching them.

## Collector is a thin composition, not an `Engine`
Bundling `ExecutionGateway` into `Engine` as a policy would mean an
engine-shaped collector needs a fake `NullExecution` it never uses — a
smell. Instead: `LiveWebSocketSource -> FileRecorder` directly, no `Engine`.
See D5 in root `docs/decisions.md` for the fuller "production streamer
first" reasoning — this entry is just the shape decision, not the sequencing
one.
