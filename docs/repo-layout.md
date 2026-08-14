# Repo layout

## Monorepo, not many repos

Single repo. Every component speaks the same event schema (`MarketEvent`,
`Order`, `Fill`, `Intent`); splitting into per-component repos means versioning
and re-pinning that schema across repos on every field change — constant early
friction, zero benefit for a solo dev. A monorepo gives one build, one version,
atomic refactors, and one Claude Code context. The **seams are module
boundaries inside one repo**, not separate repos. (You'd only split out a piece
with genuinely independent lifecycle + external consumers — none here.)

## Tree

```
quant-platform/
├── CMakeLists.txt          # top-level; adds libs + apps
├── CMakePresets.json       # release / debug(sanitizers); per-binary policy is at app level
├── vcpkg.json              # pinned deps (declared; not wired into CMake until implementation)
├── CLAUDE.md, MISSION.md, docs/, .gitignore
│
├── libs/                   # each is a CMake target = a seam (or group)
│   ├── core/               # LINGUA FRANCA: MarketEvent, Order, Fill, Intent, Timestamp,
│   │                       # Clock concept, Portfolio/StateView. Header-only. Depends on NOTHING.
│   ├── wire/               # on-disk/on-wire binary format (zstd, partitioning).
│   │                       # Shared by record (writes) AND file_replay_source (reads).
│   ├── marketdata/         # MarketDataSource concept + adapters:
│   │                       #   LiveWebSocketSource (+ book reconstruction), FileReplaySource
│   ├── venue/              # Binance glue: REST snapshot, symbol table, stream URLs, order xlate.
│   │                       # Isolated so a 2nd venue is additive.
│   ├── execution/          # ExecutionGateway concept + SimExecution, LiveExecution   (trader milestone)
│   ├── risk/               # RiskGate interface + impls (+ kill-switch)               (trader milestone)
│   ├── strategy/           # Strategy interface + concrete strategies                 (trader milestone)
│   ├── record/             # Sink concept + FileRecorder, NullSink
│   ├── engine/             # Engine<Src,Clk,Exec,Rec> template (trader only)          (trader milestone)
│   ├── telemetry/          # logging + metrics (uptime is a feature)                  (later)
│   └── analytics/          # backtest reporting: PnL, Sharpe, drawdown               (later)
│
├── apps/                   # thin mains; the ONLY place concrete types are named
│   ├── collector/main.cpp  # LiveWebSocketSource -> FileRecorder  (NOT an Engine)
│   ├── live/main.cpp       # Engine<LiveWebSocketSource, WallClock, LiveExecution, FileRecorder>  (later)
│   └── backtest/main.cpp   # Engine<FileReplaySource,    SimClock,  SimExecution,  NullSink>       (later)
│
├── research/               # Python subtree — stat-arb, ML training; reads the `wire` format
├── tools/                  # data-download scripts, ops
├── config/                 # config files (secrets .gitignored)
└── tests/                  # integration/parity tests (unit tests colocated per-lib)
```

## Dependency direction (the rule that makes it hold)

Dependencies point **inward toward `core`**; concrete adapters do **not** depend
on each other.

- `core` depends on nothing.
- `marketdata`, `execution`, `risk`, `strategy`, `record` depend on `core` only
  — never on each other. `strategy` cannot see `execution`; it knows only
  `Intent` / `StateView` / `MarketEvent`.
- `wire` depends on `core`; `record` and `file_replay_source` both depend on
  `wire` (shared format).
- `venue` depends on `core`; `marketdata` depends on `venue` privately.
- `engine` depends on the seam concepts, not concrete adapters.
- `apps/*` are the only place concrete types meet — all compile-time wiring
  happens in one thin file per binary.

That inward graph is ports-and-adapters made literal; it's what guarantees the
core can't tell live from sim.

## The collector is an app, not a lib

It's `source -> recorder`, ~a few lines, no engine. What it shares with the live
trader is the **source** (book reconstruction), so recorded data is built by the
same code as live data. Recorder runs on its own thread behind an SPSC queue so
a disk stall can't back up the socket read.

## Scaffold status

Folders only — this tree exists on disk, empty, no build files, no code.
Deliberately no CMake/vcpkg yet: writing build plumbing before the first real
adapter is cart-before-horse. Next real work is the collector slice (`core`,
`wire`, `venue`, `record`, `marketdata`, `apps/collector`) — implementation
first, CMake wired up around what actually exists. Trader libs (`execution`,
`risk`, `strategy`, `engine`) come with the trader milestone.
