# quant-platform

A modular C++ platform for medium-frequency crypto trading (Binance USD-M
perpetuals) where **backtest and live execution run the same code**. A backtest
research suite and a live trading bot built as compile-time instantiations of
one engine.

> Not an HFT system. The edge is signal quality, honest cost modeling, and
> uptime — not tick-to-trade speed. See [`MISSION.md`](MISSION.md).

## Documentation
| Doc | What |
|---|---|
| [`MISSION.md`](MISSION.md) | North star, success tiers, non-goals |
| [`docs/architecture-principles.md`](docs/architecture-principles.md) | Seams, the Engine, the rules |
| [`docs/strategy.md`](docs/strategy.md) | Strategy families & sequencing |
| [`docs/data.md`](docs/data.md) | Data sources, collector, storage |
| [`docs/environment.md`](docs/environment.md) | Build, perf, tooling, VCS, secrets |
| [`docs/roadmap.md`](docs/roadmap.md) | Phases & horizon |
| [`docs/decisions.md`](docs/decisions.md) | Decision log |
| [`CLAUDE.md`](CLAUDE.md) | Context + conventions for Claude Code |

## The core idea
The **trader** (live/backtest) is one Engine, swapped by compile-time policies:
```
live.cpp      Engine<LiveWebSocketSource, WallClock, LiveExecution, FileRecorder>
backtest.cpp  Engine<FileReplaySource,    SimClock,  SimExecution,  NullSink>
```
The **collector** is not an Engine — it's a thin `source → recorder` loop that
shares the *source* (book reconstruction), so recorded data is built by the same
code as live data. Same book-building, swapped adapters — that's the whole idea.

## Getting started (local)
```bash
git init
# set up .env / secrets loading BEFORE any exchange key exists
# (Phase 0 in docs/roadmap.md scaffolds CMake presets + vcpkg manifest)
```

Status: **design locked, Phase 0 not yet started.** See
[`docs/roadmap.md`](docs/roadmap.md).
