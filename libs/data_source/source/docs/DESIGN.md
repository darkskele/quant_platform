# source — the "prod streamer" town

Part of the **source** city (root `docs/DESIGN.md`). Venue-agnostic
resync/gap-detection machinery plus the live transport
(`GenericLiveWebSocketSource`, Boost.Beast) — both town-level content now
(D18, `docs/DECISIONS.md`: the transport used to be its own `protocol/`
village, collapsed back once it was clear only one would ever exist). One
village plugs in below: `venue/` (which venue), describing itself in its
own `DESIGN.md`.

## Diagram

```
raw WS/REST bytes                    (GenericLiveWebSocketSource, this town)
        │
        ▼
Parser::parse_message() / ::parse_depth_snapshot()   (venue/ village
        │                                              plugs in here)
        ▼
    MarketEvent
        │
        ▼
┌────────────────────────────────────┐
│ ResyncCoordinator.on_event() /       │  town-level, this directory —
│ .on_snapshot()                        │  zero venue knowledge
│   SequenceGapDetector (gap_detector.hpp)
│   find_resync_point   (resync.hpp)
└──────────────────┬───────────────────┘
                    │ Forward | Buffer | BufferAndRequest
                    ▼
        Source::next()                    (source.hpp — the output seam)
                    │
                    ▼
        consumer (apps/collector, future Engine)
```

`backoff.hpp` (reconnect delay schedule) sits on the transport side, not
the event pipeline above — used by `GenericLiveWebSocketSource`
(`live_websocket_source.hpp`, this directory) between reconnect attempts.

## Generic components

- **`Source` concept** (`source.hpp`) — output seam: `next() ->
  optional<MarketEvent>`, satisfied by this town's live streamer or a
  future file-replay source.
- **`Parser` concept** (`parser.hpp`) — input seam: what a venue must
  implement to plug in.
- **`ResyncCoordinator`** (`resync_coordinator.hpp`) — per-symbol
  buffer/align/replay decision.
- **`SequenceGapDetector`** (`gap_detector.hpp`) — per-symbol
  sequence-gap detection.
- **`find_resync_point`** (`resync.hpp`) — the snapshot+diff alignment
  rule (USD-M futures).
- **`backoff.hpp`** — reconnect delay schedule.
- **`GenericLiveWebSocketSource`** (`live_websocket_source.hpp`/`.cpp`) —
  the live transport: Boost.Beast websocket I/O thread + REST resync
  thread, templated on `Parser`. This town's only `Source` implementation
  today.
- **`SymbolTable`/`WsEndpoint`/`RestEndpoint`/`DepthSnapshot`**
  (`venue_types.hpp`) — venue-agnostic types the `Parser` concept and
  `GenericLiveWebSocketSource` are built against; `venue::binance::` aliases
  them rather than owning its own copies (D16).

## High-level implementation

Town-level content, pure logic plus the one live transport — `venue/`
describes its own files in its own `DESIGN.md`.

- `source.hpp` / `parser.hpp` — the two concepts.
- `resync_coordinator.hpp` / `gap_detector.hpp` / `resync.hpp` — resync
  and gap-detection state machine.
- `backoff.hpp` — reconnect backoff.
- `venue_types.hpp` — the venue-agnostic types `parser.hpp` requires.
- `live_websocket_source.hpp` / `.cpp` — `GenericLiveWebSocketSource`, the
  Boost.Beast transport (needs Boost/OpenSSL; degrades gracefully without
  them, see this town's `CMakeLists.txt`).

## Goals

1. ~~**Build a live streamer: WebSocket protocol + Binance venue**~~ — the
   generic resync/gap-detection engine this town owns, venue-agnostic
   enough that a second venue plugs in without a rewrite. `venue/` is a
   leaf (single implementation, no separate `DESIGN.md`) — its sub-goals
   below.
   - ~~**Correct under real network conditions**~~ — connection loss,
     reconnect, backoff, REST latency, not just a synthetic mock server.
   - ~~**Thread-safe**~~ — I/O + REST-resync threads coordinated via SPSC
     queues only, no other shared mutable state.
   - **Recorder never backs up the socket read** — N/A here; proven at
     `apps/collector`, the level where that composition actually exists.
   - ~~**Venue isolation**~~ — nothing outside `venue/` names a
     Binance-specific type directly; the town only sees it through the
     `Parser` concept.
   - ~~**Fast parsing**~~ — real-time depth-diff parsing is the actual
     I/O-thread hot path.
