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
| [`docs/repo-layout.md`](docs/repo-layout.md) | Directory tree, module boundaries, the doc-hierarchy convention |
| [`docs/decisions.md`](docs/decisions.md) | Root decision log (per-directory logs live nearest their code) |
| [`docs/DESIGN.md`](docs/DESIGN.md) / [`docs/STATUS.md`](docs/STATUS.md) | Root goals + current status of the engineering-process initiative |
| [`CLAUDE.md`](CLAUDE.md) | Context + conventions for Claude Code |

Every non-leaf directory has its own `docs/DESIGN.md`/`docs/STATUS.md`
(+ `docs/DECISIONS.md` where relevant) — start at a lib's own doc, not just
the root ones, for anything below the whole-repo level.

## The core idea
The **trader** (live/backtest) is one Engine, swapped by compile-time policies:
```
live.cpp      Engine<LiveWebSocketSource, WallClock, LiveExecution, FileRecorder>
backtest.cpp  Engine<FileReplaySource,    SimClock,  SimExecution,  NullSink>
```
The **collector** is not an Engine — it's a thin `source → recorder` loop.
It shares the *source* with the trader (book reconstruction: resync, gap
detection, reconnect), so live and collected data are produced by the same
code, not a second copy. Same source, swapped adapters — that's the whole
idea.

## Building

```bash
cmake --preset debug         # or release, or tsan (ThreadSanitizer)
cmake --build build/debug -j
```

Debug also carries ASan/UBSan. See [`docs/environment.md`](docs/environment.md)
for the toolchain (vcpkg manifest, GoogleTest, Google Benchmark) and the
WSL-vs-VM performance caveat.

## Testing

```bash
ctest --test-dir build/debug --output-on-failure
```

## Benchmarks

```bash
cmake --preset release
cmake --build build/release --target qp_bench -j
./build/release/qp_bench --benchmark_min_time=0.1s
```

## Running the collector

```bash
./build/debug/apps/collector/qp_collector SYMBOL [SYMBOL...] --data-dir DIR [--duration SECONDS] [--testnet]
```

For a VM deploy, `tools/package_collector.sh` bundles a release binary with
`run.sh`/`stop.sh`/`status.sh` into a self-contained tarball:

```bash
./run.sh SYMBOL [SYMBOL...] [--duration SECONDS] [--testnet]
```
