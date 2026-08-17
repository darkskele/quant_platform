# collector

## Diagram

```
argv
 │
 ▼
parse_args() ──▶ Config
                   │  symbols, data_dir, ws/rest endpoint, run_duration
                   ▼
                  run(Config, stop_requested)
                   │
        ┌──────────┴────────────────────────────────┐
        │ venue.hpp (compile-time — QP_COLLECTOR_VENUE│
        │ CMake option -> QP_VENUE_* define)          │
        │   SelectedParser                            │
        │   kDefaultWsEndpoint / kDefaultRestEndpoint  │
        │   kDefaultWsTestnet  / kDefaultRestTestnet   │
        └──────────┬────────────────────────────────┘
                   │ instantiates
                   ▼
  GenericLiveWebSocketSource<SelectedParser>          FileRecorder
  (libs/data_source/source)                           (libs/data_source/sink)
  connect, resync, gap-detect, reconnect               zstd + partition to disk
  1 I/O thread + 1 resync thread                        1 writer thread
                   │                                        ▲
                   └───────────── MarketEvent ───────────────┘
                    (in-process; run() polls source.next(),
                     pushes to recorder.record())
```

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

- **`Config`** (`collector.hpp`) — venue-agnostic run parameters: symbols,
  `data_dir`, generic `WsEndpoint`/`RestEndpoint`
  (`venue_types.hpp`), `run_duration`.
- **`venue.hpp`** — the one file allowed to name a concrete venue.
  `QP_COLLECTOR_VENUE` picks the `#if`/`#error` branch defining
  `SelectedParser` and the `kDefault*` endpoint constants; every other file
  in this directory reads only those generic names.
- **`GenericLiveWebSocketSource<SelectedParser>`** (`libs/data_source/source`)
  — the **source**: connects, resyncs after gaps/reconnects, emits a
  `MarketEvent` stream on its own I/O thread.
- **`FileRecorder`** (`libs/data_source/sink`) — the **sink**: writes the
  `MarketEvent` stream to disk (zstd, partitioned by symbol+date) on its own
  thread.
- **`run()`** — the composition loop: polls `source.next()`, pushes to
  `recorder.record()`, logs periodic status, stops on `stop_requested` or
  `config.run_duration`.

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
