# sink status

- ~~G1 — Build the FileRecorder sink~~

## Last proof

Wire codec (encode/decode, zstd, day/segment naming) moved to
`libs/data_source/wire` (D19) — this lib now depends on it instead of
owning it; `qp_sink_bench`/`qp_sink_tests` moved with it to
`qp_wire_bench`/`qp_wire_tests`. `qp_sink_integration_tests` (`FileRecorder`
itself, still here) unchanged in behavior, only in what it links against.
- Not rebuilt/tested this pass — pending `/test libs/data_source/sink` (or
  `/test all`) before this is trusted.
- As of: working tree (uncommitted).
