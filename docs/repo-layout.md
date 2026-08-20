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
│   ├── data_source/        # "source" + "sinks" cities — coequal, independent seams
│   │   │                   # (Source vs Sink), grouped as sibling dirs for
│   │   │                   # filesystem convenience only — see DESIGN.md's "Cities today".
│   │   ├── wire/            # codec substrate (like core, but scoped to data_source):
│   │   │                    # wire.hpp (MarketEvent<->bytes), zstd_stream.hpp
│   │   │                    # (bytes<->zstd bytes), partition.hpp (day/segment naming).
│   │   │                    # Not itself a city — source and sink both depend on it.
│   │   ├── source/         # "prod streamer" town: Source concept,
│   │   │   │               # ResyncCoordinator/gap-detection/backoff, the Parser concept,
│   │   │   │               # GenericLiveWebSocketSource (Boost.Beast transport),
│   │   │   │               # FileReplaySource (backtest Source, reads via wire) — all
│   │   │   │               # town-level now, venue-agnostic. One village nested below:
│   │   │   └── venue/               # "venue" village: Binance glue (REST
│   │   │                        # snapshot, symbol table, stream URLs). Flat --
│   │   │                        # a leaf, no separate docs/. Only venue today.
│   │   └── sink/            # "sinks" city: Sink concept + FileRecorder, NullSink.
│   │                        # No further nesting.
│   ├── execution/          # ExecutionGateway concept + SimExecution, LiveExecution   (trader milestone)
│   ├── risk/               # RiskGate interface + impls (+ kill-switch)               (trader milestone)
│   ├── strategy/           # Strategy interface + concrete strategies                 (trader milestone)
│   ├── engine/             # Engine<Tx,Clk,Exec> template (trader only)               (trader milestone)
│   ├── telemetry/          # logging + metrics (uptime is a feature)                  (later)
│   └── analytics/          # backtest reporting: PnL, Sharpe, drawdown               (later)
│
├── apps/                   # thin mains; the ONLY place concrete types are named
│   ├── collector/main.cpp  # LiveWebSocketSource -> FileRecorder  (NOT an Engine)
│   ├── live/main.cpp       # Engine<LiveWebSocketSource, WallClock, LiveExecution>  (later)
│   └── backtest/main.cpp   # Engine<FileReplaySource,    SimClock,  SimExecution>   (later)
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
- `data_source/source`, `execution`, `risk`, `strategy`, `data_source/sink`
  depend on `core` only — never on each other. `strategy` cannot see
  `execution`; it knows only `Intent` / `StateView` / `MarketEvent`.
- `data_source/wire` is substrate too, like `core`, but scoped to
  `data_source`'s two cities rather than the whole repo: depends on `core`
  only. Both `data_source/source` (`FileReplaySource`'s read side) and
  `data_source/sink` (`FileRecorder`'s write side) depend **publicly** on it
  for the shared wire format/zstd codec/day-segment naming — never on each
  other (D19).
- `data_source/source/venue` depends on `core`;
  `data_source/source` itself (the live transport,
  `GenericLiveWebSocketSource`) depends on `venue`
  **publicly** (its own public header names venue types in the class
  interface — see `docs/environment.md`'s PUBLIC/PRIVATE rule).
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

## The city/town/village doc convention

Every non-leaf directory gets a `docs/` folder holding `DESIGN.md`
(goals + success metrics — tests, and benchmarks where hot-path applies, N/A
stated explicitly where cold-path) and `STATUS.md` (a goal-tracking list + a
"Last proof" section — descriptions live only in `DESIGN.md`, `STATUS.md`
never restates them). `DECISIONS.md` joins them in
that same `docs/` folder wherever there's real content — an append-only log,
title-only where that's the whole story, longer where a decision genuinely
earned it (a bug's root-cause narrative). A superseded decision is struck
through (`~~...~~`) at its old location with a pointer to where it moved —
never deleted. Leaf directories get code comments only, no doc files.

**Goals cascade by level, not by content.** Root `DESIGN.md`'s goals are the
repo's own milestones (`docs/roadmap.md`'s phases). Every directory below
that states its goals as what should *exist at that directory* — a
deliverable, not a design essay — naturally sharper the deeper you go (a
town's goal names a capability, a village's names a component, a leaf's
would name a file if leaves had `DESIGN.md`s — they don't). Once a "goal"
would only make sense talking about a single file's contents, it's stopped
being a goal — that's implementation, and it lives inline as a code
comment, not a doc.

**Goal tracking**: goals are stated super high level — a title, one clause
at most (`~~**Shared wire format**~~ — one encoder, not two`, not a
paragraph). A completed goal's title is struck through (`~~...~~`) at the
point it's stated — same convention as a superseded decision, never
deleted — and drops its success metric once struck through: proof of a done
goal lives in `STATUS.md`'s "Last proof" and the test suite, not restated
prose. An incomplete goal keeps its success metric (what "done" would look
like is exactly what's still needed) and stays a title someone can act on,
not a design essay. `STATUS.md` lists every goal by title only (one line);
a completed goal needs nothing more. An incomplete goal's `STATUS.md` line
expands with a short paragraph — what's missing, what proving it would take
— so there's somewhere to look for "why isn't this done" without
re-deriving it each time.

**Composition-level `DESIGN.md`** (a directory that wires together concrete
types from more than one lib/village — today, `apps/*`) additionally opens
with a diagram of the pieces and how data flows between them, then a bullet
list of the generic components involved (what each represents, how they
connect), then a high-level rundown of the directory's own source files —
*before* the goals section. Leaf/pure-logic `DESIGN.md`s (a single lib with
no sub-wiring) don't need this — it earns its place at a composition point,
not everywhere.

"City"/"town" are logical groupings, not always a physical directory — root
`DESIGN.md`'s **sinks** city (`libs/data_source/sink/`) has no further nesting and
needs none. Physically restructuring to mirror a logical grouping is worth
doing when it's cheap and the grouping is real — not forced pre-emptively,
and never with placeholder folders for a variant that doesn't exist yet:
`libs/data_source/source/venue/` is flat, a leaf (no `venue/binance/`),
until a second venue justifies the nesting. The prod-streamer town's other
would-be village, `protocol/`, was collapsed back into the town itself
(D18, town `docs/DECISIONS.md`) once it was clear only one transport would
ever live there.

## Scaffold status

`core`, `data_source/wire`, `data_source/source` (town-level + its `venue`
village, now including `FileReplaySource`), `data_source/sink`,
`apps/collector`, and root `tests/` (cross-lib parity) are implemented,
building, and tested — this is no longer folders-only. Trader libs
(`execution`, `risk`, `strategy`, `engine`) remain scaffold-only, arriving
with the trader milestone.
