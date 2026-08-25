# collector

## Diagram

```
argv
 │
 ▼
parse_args() ──▶ Config
                   │  symbols, data_dir, ws/rest + spot_ws/spot_rest endpoints, run_duration
                   ▼
                  run(Config, stop_requested)
                   │
        ┌──────────┴──────────────────────────────────────┐
        │ venue.hpp (compile-time — QP_COLLECTOR_VENUE      │
        │ CMake option -> QP_VENUE_* define)                │
        │   SelectedParser         / SelectedSpotParser      │
        │   SelectedAlignment      / SelectedSpotAlignment   │
        │   kDefaultWs/RestEndpoint / kDefaultSpotWs/Rest...  │
        └──────────┬──────────────────────────────────────┘
                   │ constructs both legs' source+recorder (D41)
        ┌──────────┴──────────────────┐
        ▼                              ▼
  GenericLiveWebSocketSource      GenericLiveWebSocketSource
  <SelectedParser,                <SelectedSpotParser,
   SelectedAlignment>              SelectedSpotAlignment>
  ("futures")                     ("spot")
    (libs/data_source/source)       (libs/data_source/source)
    connect, resync, gap-detect       "
    1 I/O thread + 1 resync thread    "
         │                                 │
         ▼                                 ▼
    FileRecorder ->                   FileRecorder ->
    data_dir/futures/                 data_dir/spot/
    (libs/data_source/sink)           (libs/data_source/sink)
         │                                 │
         └────────────────┬────────────────┘
                           ▼
           run_data_source(sources, sinks, running)
           (libs/data_source, D44) — its own thread; pairs
           source i with sink i positionally, stamps
           MarketEvent::venue=i as it does
```

`run_data_source` (D44) owns the poll/record loop on its own thread —
generic over any `Source`/`Sink` pair, so it knows nothing about
`GenericLiveWebSocketSource`'s specific gap/resync counters. `run()`'s own
thread is freed up to just watch `stop_requested`/the `--duration`
deadline and print periodic per-leg status; each leg's `MarketEvent`
stream goes straight to that leg's own `FileRecorder`, never merged
(D41) — no `Engine`, no shared `SymbolTable`.

Which macro compiles which concrete variant — today `QP_COLLECTOR_VENUE`
only selects **venue** (which `Parser`); the transport is fixed to
`GenericLiveWebSocketSource` (Boost.Beast websocket), the only one that
exists — it used to be its own `protocol/` village, now it's this town's
own content (D18, `libs/data_source/source/docs/DECISIONS.md`):

```
                      transport
                  websocket          multicast
                ┌─────────────┬───────────────────────┐
        binance │    BUILT     │ illustrative only —    │
       (venue)  │ QP_VENUE_    │ no such transport      │
                │ BINANCE      │ exists; shown only to  │
                │ (default)    │ show the axis          │
                └─────────────┴───────────────────────┘
```

## Generic components

- **`Config`** (`collector.hpp`) — venue-agnostic run parameters: symbols
  (shared across both legs, D41), `data_dir` (root; `run()` appends
  `futures/`/`spot/`), generic `WsEndpoint`/`RestEndpoint` per leg
  (`venue_types.hpp`), `run_duration`.
- **`venue.hpp`** — the one file allowed to name a concrete venue.
  `QP_COLLECTOR_VENUE` picks the `#if`/`#error` branch defining
  `SelectedParser`/`SelectedAlignment` (futures) and
  `SelectedSpotParser`/`SelectedSpotAlignment` (spot, D40/D41), plus each
  leg's `kDefault*` endpoint constants; every other file in this directory
  reads only those generic names.
- **`GenericLiveWebSocketSource<P, Rule>`** (`libs/data_source/source`) —
  the **source**: connects, resyncs after gaps/reconnects, emits a
  `MarketEvent` stream on its own I/O thread. One instance per leg.
- **`FileRecorder`** (`libs/data_source/sink`) — the **sink**: writes the
  `MarketEvent` stream to disk (zstd, partitioned by symbol+date) on its own
  thread. One instance per leg, writing to that leg's own subdirectory.
- **`run_data_source`** (`libs/data_source`, D44) — pairs the two legs'
  sources with their recorders positionally (`std::tie`'d tuples) and
  stamps `MarketEvent::venue` as it drives them, on its own thread.
- **`log_status_if_due`** (`collector.cpp`, internal, templated on
  source/recorder type) — one leg's periodic/warning status line, prefixed
  by leg name, called from `run()`'s own thread once per `kPollInterval`.
- **`run()`** — constructs both legs' source+recorder, hands them to
  `run_data_source` on a background thread, then watches
  `stop_requested`/`config.run_duration` and logs periodic status itself
  until either fires, at which point it stops the driver and joins it.

## High-level implementation

- `venue.hpp` — compile-time venue dispatch (D17).
- `collector.hpp` / `collector.cpp` — `Config`, `parse_args`, `run`.
- `main.cpp` — signal handling (`SIGINT`/`SIGTERM` -> `stop_requested`),
  thin entrypoint.
- `tests/test_collector_integration.cpp` — end-to-end proof against real
  captured payloads.

## Goals

1. ~~**Records real captured data end-to-end**~~ — real Binance wire
   format through the actual resync machinery, not a synthetic round-trip.
2. ~~**Recorder never backs up the socket read**~~ — own thread, SPSC queue.
3. ~~**`--duration`/`stop_requested` both actually stop the collector**~~.
4. **Recorded snapshot+diff sequence reconstructs the real book.** Proving
   recorded bytes match what was sent (G1) isn't proving the book you'd
   rebuild from them is *correct* — usable for anything downstream needs
   the latter.
   - **Success metric:** a reconstruction check — apply the full recorded
     diff sequence on top of the recorded `BookSnapshot`, in order, and
     compare the resulting book (top-of-book at minimum, full depth where
     feasible) against an independently-obtained reference at a matching
     point in time (e.g. a REST snapshot fetched separately) — any mismatch
     is a hard failure, not a warning.
5. ~~**Records both legs (futures + spot), fully independent**~~ (D41) —
   funding-carry needs both; each leg's own `SymbolTable`/manifest/recording
   stays separate, avoiding the `SymbolId` collision merging them would hit.
