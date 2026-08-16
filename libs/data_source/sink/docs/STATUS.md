# sink status

- ~~G1 — Build the FileRecorder sink~~

## Last proof

`qp_sink_bench`'s `BM_*FullDepthSnapshot` (1000 levels, Binance's max):
write ~864ns, read ~1267ns, round-trip ~2115ns — cold path, proves the
format scales.
- `ctest` → `qp_sink_tests`/`qp_sink_integration_tests` passing.
- As of: working tree (uncommitted).

`qp::record`/`qp_record_*` renamed to `qp::sink`/`qp_sink_*` to match this
lib's actual folder name — see root `docs/decisions.md` D16.
