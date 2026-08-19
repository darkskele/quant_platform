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

## Last proof

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
