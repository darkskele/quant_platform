# libs status

- ~~G1 — Working data source~~
- ~~G2 — SimClock~~
- ~~G3 — SimExecution~~
- ~~G4 — Trader-milestone skeleton~~

## Last proof

`Clock` concept + `SimClock` (`libs/clock`) added (D22/D23). `advance()` is
the Engine loop's future hook for driving simulated time off replayed event
timestamps. `WallClock` deliberately not built — a live wall clock safe
against `system_clock`'s non-monotonicity wants an anchor-plus-
`steady_clock` design, ahead of Phase 2's actual live requirements right
now (D23). `qp_clock_tests`: concept satisfaction, `SimClock` advance
semantics — 2/2 passing.

`apps/collector`'s live streamer → `FileRecorder` → `FileReplaySource`
round trip was already proven (`qp_parity_tests`) before this — the
"working data source" goal predates `libs/` having its own goal-tracking
doc, restated here now that it does.

`ExecutionGateway`/`Matcher` concepts + `SimExecution<M>` +
`LastTradeMatcher` (`libs/execution`) added (D25): `SimExecution<M>` wraps a
`Matcher` with outcome-queue/poll plumbing; `LastTradeMatcher` is the first
`Matcher` — per-symbol last-`Trade`-price map, instant full fill at flat
taker fee, `NoPriceAvailable` reject if no price seen yet.
`core/types.hpp` gained `Order`/`Fill`/`Reject`/`RejectReason`/`OrderId`/
`Notional` (D25) and a `Price`/`Qty` fixed-point exactness gap flagged and
deferred, not fixed (D26). `qp_execution_tests`: concept-satisfaction
`static_assert`s, reject before any trade, fill at last-seen price,
per-symbol price independence, `nullopt` on an empty queue, and outcome
ordering (reject then fill, matching submission order rather than grouped
by kind).

`Strategy`/`RiskGate` moved from a planned virtual-dispatch design to C++20
concepts, and `Engine` from `vector<unique_ptr<Strategy>>` to a closed
`std::tuple<Strategies...>` + a concept-constrained `Risk` — D4's "virtual:
strategy, risk" was wrong, superseded by D27: this project isn't opting out
of latency sensitivity, only out of hardware (colo/kernel-bypass/FPGA) it
can't afford, so a vtable/heap allocation on this path is a self-inflicted,
avoidable cost. `Intent`/`Portfolio`/`StateView` (`core`), `Strategy`
(`strategy`), `RiskGate`/`RiskDecision` (`risk`), and
`Engine<Tx,Clk,Exec,Risk,NumWorkers,Strategies...>` (`engine`) added (D28)
— see `libs/engine/docs/STATUS.md` for `Engine`'s own proof, including the
follow-up `RoundRobinPool`-based parallel dispatch (D33). `qp_strategy_tests`:
concept satisfaction against an inline `NoopStrategy` — 1/1 passing.
`qp_risk_tests`: concept satisfaction, an approved decision carries an
`Order` sized from the `Intent`, a rejected one carries none — 2/2 passing.

`Portfolio`'s `unordered_map<SymbolId, Qty>` replaced with a fixed
`std::array<Qty, 64>`, direct-indexed by `SymbolId` (D31) — `SymbolId` is
already a dense id assigned by the run's own bounded, config-known symbol
set, so array indexing is the correct structure, not a premature
optimization; no allocation, no hashing on `apply_fill`/`position`.
Separately, `Fill`/`Reject`/`MarketEvent` (pre-existing, D25) reordered for
alignment (D32): `Fill` 56→48 bytes, `Reject` 32→24, `MarketEvent`
~128→~112 — `static_assert(sizeof(...) == N)` guards added to `Fill`/
`Reject` matching `PriceLevel`'s existing convention (verified against a
real compilation, not just hand arithmetic). No behavior change; three
designated-initializer call sites updated to the new declaration order
(`last_trade_matcher.hpp`, `test_portfolio.cpp`).
