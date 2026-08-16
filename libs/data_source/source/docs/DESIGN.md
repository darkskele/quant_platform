# source — the "prod streamer" town

Part of the **source** city (root `docs/DESIGN.md`). Generic,
protocol-and-venue-agnostic: two villages plug in below —
`venue/` (which venue) and `protocol/` (which
transport) — `prod streamer = protocol + venue`. Villages describe
themselves in their own `DESIGN.md`.

## Diagram

```
raw WS/REST bytes                          (protocol/ village)
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
│ .on_snapshot()                        │  zero venue/protocol knowledge
│   SequenceGapDetector (gap_detector.hpp)
│   find_resync_point   (resync.hpp)
└──────────────────┬───────────────────┘
                    │ Forward | Buffer | BufferAndRequest
                    ▼
        MarketDataSource::next()          (source.hpp — the output seam)
                    │
                    ▼
        consumer (apps/collector, future Engine)
```

`backoff.hpp` (reconnect delay schedule) sits on the transport side, not
the event pipeline above — used by `protocol/`'s
`GenericLiveWebSocketSource` between reconnect attempts.

## Generic components

- **`MarketDataSource` concept** (`source.hpp`) — output seam: `next() ->
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
- **`SymbolTable`/`WsEndpoint`/`RestEndpoint`/`DepthSnapshot`**
  (`venue_types.hpp`) — venue-agnostic types the `Parser` concept and
  `GenericLiveWebSocketSource` are built against; `venue::binance::` aliases
  them rather than owning its own copies (D16).

## High-level implementation

Town-level pure logic only — villages (`venue/`,
`protocol/`) describe their own files in their own `DESIGN.md`.

- `source.hpp` / `parser.hpp` — the two concepts.
- `resync_coordinator.hpp` / `gap_detector.hpp` / `resync.hpp` — resync
  and gap-detection state machine.
- `backoff.hpp` — reconnect backoff.
- `venue_types.hpp` — the venue-agnostic types `parser.hpp` requires.

## Goals

1. ~~**Build a live streamer: WebSocket protocol + Binance venue**~~ — the
   generic resync/gap-detection engine this town owns, venue-and-protocol-
   agnostic enough that a second venue or protocol plugs in without a
   rewrite. `protocol/` and `venue/` are both leaves (single
   implementation each, no separate `DESIGN.md`) — their sub-goals below.
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
