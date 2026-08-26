# tests — cross-lib integration/parity

Composes concrete types from two libs that deliberately never depend on
each other (`data_source/source`'s `FileReplaySource` and
`data_source/sink`'s `FileRecorder`, both depending only on
`data_source/wire` — docs/repo-layout.md's dependency-direction rule). No
single lib can prove they agree with each other; this directory exists
specifically for that, and for future cross-lib parity checks of the same
shape (e.g. Phase 2's backtest-vs-live decision parity, `docs/roadmap.md`).

## Diagram

```
FileRecorder                          FileReplaySource
(libs/data_source/sink)               (libs/data_source/source)
writes MarketEvents to disk            reads them back
       │                                      ▲
       └──────────── {data_dir}/──────────────┘
                (real files, real zstd —
                 not mocked, not shared code
                 under test on either side)
                       │
                       ▼
         test_recorder_replay_parity.cpp
         asserts: replayed == written
         (field-for-field, global ts order)
```

## Generic components

None — this directory names concrete types directly (`FileRecorder`,
`FileReplaySource`), same as `apps/*`: it's a composition point, not a seam.

## High-level implementation

- `test_recorder_replay_parity.cpp` — writes real `MarketEvent`s through a
  real `FileRecorder`, lets it close cleanly, then drains a real
  `FileReplaySource` pointed at the same directory and diffs the result.

## Goals

1. ~~**Recorder → replay round-trip is provably lossless.**~~ Proving
   `FileReplaySource` satisfies the wire-format spec (its own tests,
   `libs/data_source/source/tests/test_file_replay_source.cpp`) isn't
   proving it agrees with what the *real* `FileRecorder` actually
   produces — usable data for a backtest needs the latter.
