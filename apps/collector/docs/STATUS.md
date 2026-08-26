# collector status

- ~~G1 — Records real captured data end-to-end~~
- ~~G2 — Recorder never backs up the socket read~~
- ~~G3 — `--duration`/`stop_requested` both actually stop the collector~~
- G4 — Recorded snapshot+diff sequence reconstructs the real book
  Not proven. Needs a reconstruction routine (apply the recorded diffs to
  the recorded `BookSnapshot`, in order) plus an independent ground-truth
  reference to compare the result against (e.g. a REST snapshot fetched
  separately at a matching point). No test exercises this today —
  `qp_collector_integration_tests` proves recorded bytes match what was
  sent, not that the book you'd rebuild from them is correct.
- G5 — Records both legs (futures + spot), fully independent
  Built, not yet run this pass (see CLAUDE.md's build workflow). `run()`
  now unconditionally drives two `Leg` instances (futures via
  `SelectedParser`/`SelectedAlignment`, spot via new
  `SelectedSpotParser`/`SelectedSpotAlignment`, both `venue.hpp`), each
  with its own `GenericLiveWebSocketSource`/`FileRecorder` writing to
  `data_dir/futures/`/`data_dir/spot/`.
  `RecordsBothLegsToSeparateDirectoriesEndToEnd` (renamed from the old
  single-leg `RecordsRealCapturedEventsEndToEnd`) is written to assert both
  legs record correctly — distinct sequence ranges per leg, same symbol
  name on both, each leg's decoded events and manifest checked independent
  of the other's. `StopRequestedStopsBeforeTheDurationDeadline` updated to
  mock both legs too (every test now must, since `run()` always builds
  both — leaving spot unmocked would have made the test suite dial out to
  production Binance).

## Last proof

**D42 (run_data_source adoption)** — `run()`'s hand-written `Leg`/poll loop
replaced with `run_data_source` (`libs/data_source`, D43) on its own
thread; `run()`'s own thread now just watches `stop_requested`/
`config.run_duration` and logs periodic status. Same observable behavior
intended (same constructor args, same `data_dir/<leg>/` split, same status
line format) — not yet built/re-run against this specific change (see
CLAUDE.md's build workflow); the "2/2 passing" note below is from the
prior `Leg`-based implementation, not re-verified against this one yet.

- `qp_collector_integration_tests` → 2/2 tests updated for two legs (prior,
  `Leg`-based implementation).

**D17 (venue-select macro)** — `Config`/`run()` now generic over the
venue.hpp-selected `SelectedParser`/`WsEndpoint`/`RestEndpoint` instead of
naming `venue::binance::` directly; `-DQP_COLLECTOR_VENUE=kraken` confirmed
to fail the build at `venue.hpp`'s `#error`, not downstream.
- `qp_collector_integration_tests` → passing, unchanged behavior.
- TSan clean.

Carried forward from D14: `events[0]` is the `BookSnapshot` anchor with real
captured level counts, `events[1..]` are the diffs — recorded output has an
independent baseline, not diffs alone. Still true, not re-verified this pass
(no code change in that path).
