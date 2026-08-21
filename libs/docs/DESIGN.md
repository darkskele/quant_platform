# libs — every seam, one lib each

Not itself a city — the parent directory holding every lib (`core`'s
substrate, `data_source`'s cities, `clock`, and the trader-milestone libs
still to come). Topology and dependency direction live in
`docs/repo-layout.md`; the seams themselves in
`docs/architecture-principles.md`. This file states only this level's own
goals, per the city/town/village convention.

## Goals

1. ~~**A working data source.**~~ `core` + `data_source/wire` + `.../source`
   + `.../sink`, proven end-to-end by `apps/collector` and
   `tests/test_recorder_replay_parity.cpp` (real record → replay agreement,
   not just unit tests in isolation).
2. ~~**SimClock.**~~ `clock`'s `Clock` concept + `SimClock` (D22/D23),
   proven by `qp_clock_tests`. `WallClock` deliberately deferred to the live
   milestone (D23). No `Engine` consumer yet — arrives with the trader
   milestone.
3. ~~**SimExecution.**~~ `execution`'s `ExecutionGateway`/`Matcher`
   concepts + `SimExecution<M>` + `LastTradeMatcher` (D25), proven by
   `qp_execution_tests`. Deliberately one `Matcher` today — more (book-aware,
   slippage-modeling) are the expected direction as strategy families need
   them, sibling files under `execution` (D25). `LiveExecution` — real order
   execution, not simulated — deferred to the live milestone. No `Engine`
   consumer yet — arrives with the trader milestone.
