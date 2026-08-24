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
                   │ instantiates two Legs (D41)
        ┌──────────┴──────────────────┐
        ▼                              ▼
  Leg<SelectedParser,             Leg<SelectedSpotParser,
      SelectedAlignment>              SelectedSpotAlignment>
  ("futures")                     ("spot")
    GenericLiveWebSocketSource      GenericLiveWebSocketSource
    (libs/data_source/source)       (libs/data_source/source)
    connect, resync, gap-detect       "
    1 I/O thread + 1 resync thread    "
         │                                 │
         ▼                                 ▼
    FileRecorder ->                   FileRecorder ->
    data_dir/futures/                 data_dir/spot/
    (libs/data_source/sink)           (libs/data_source/sink)
```

`run()`'s loop polls both `Leg`s every iteration (`Leg::poll()`, non-
blocking either way — each `GenericLiveWebSocketSource` already runs its
own I/O/resync threads, D10) and sleeps only when neither had an event;
each leg's `MarketEvent` stream goes straight to that leg's own
`FileRecorder`, never merged (D41) — no `Engine`, no shared `SymbolTable`.

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
- **`Leg<Parser, Rule>`** (`collector.cpp`, internal) — one leg's
  `GenericLiveWebSocketSource` + `FileRecorder` pair plus its retry-count
  bookkeeping (D41). `poll()` is non-blocking; `log_status_if_due()` prints
  that leg's periodic/warning status, prefixed by leg name.
- **`GenericLiveWebSocketSource<P, Rule>`** (`libs/data_source/source`) —
  the **source**: connects, resyncs after gaps/reconnects, emits a
  `MarketEvent` stream on its own I/O thread. One instance per leg.
- **`FileRecorder`** (`libs/data_source/sink`) — the **sink**: writes the
  `MarketEvent` stream to disk (zstd, partitioned by symbol+date) on its own
  thread. One instance per leg, writing to that leg's own subdirectory.
- **`run()`** — the composition loop: polls both `Leg`s every iteration,
  each straight to its own recorder, logs periodic status per leg, stops on
  `stop_requested` or `config.run_duration`.

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
