# libs status

- ~~G1 — Working data source~~
- ~~G2 — SimClock~~

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
