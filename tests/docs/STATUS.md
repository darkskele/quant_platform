# tests (cross-lib parity) status

- ~~G1 — Recorder → replay round-trip is provably lossless~~

## Last proof

`qp_parity_tests` added (`test_recorder_replay_parity.cpp`): single-symbol
round-trip (BookDiff with/without levels, Trade, Funding — field-for-field),
multi-symbol global-timestamp-order reassembly from per-symbol partitions,
and a day-partition-boundary crossing. Real `FileRecorder`/`FileReplaySource`
composed directly — no shared test helpers or mocks between them.
