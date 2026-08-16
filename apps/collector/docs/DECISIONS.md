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

## Collector is a thin composition, not an `Engine`
Bundling `ExecutionGateway` into `Engine` as a policy would mean an
engine-shaped collector needs a fake `NullExecution` it never uses — a
smell. Instead: `LiveWebSocketSource -> FileRecorder` directly, no `Engine`.
See D5 in root `docs/decisions.md` for the fuller "production streamer
first" reasoning — this entry is just the shape decision, not the sequencing
one.
