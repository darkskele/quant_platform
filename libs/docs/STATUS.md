# libs status

- ~~G1 — Working data source~~
- ~~G2 — SimClock~~
- ~~G3 — SimExecution~~

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
