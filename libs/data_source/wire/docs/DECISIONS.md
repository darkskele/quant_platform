# wire decisions

## D12 (wire-format third) — relocated from `libs/data_source/sink/docs/DECISIONS.md`, itself relocated here by D19 (root `docs/decisions.md`)

**Wire format** (`write_event`/`read_event`) is pure, round-trip-tested
functions, no I/O — `FileRecorder` (`sink`) and `FileReplaySource`
(`source`) call the *same* code, so backtest reading can't silently drift
from what the collector wrote. A truncated trailing record on read returns
`nullopt` (end-of-readable-data), not an error — also the crash-safety
contract for the read side.

See `libs/data_source/sink/docs/DECISIONS.md` for the crash-safety (flush
cadence) third and `libs/data_source/source/docs/DECISIONS.md` for the
gap-alerting third — both stayed with their respective libs; only the
format itself moved here.

## D19 — see root `docs/decisions.md`

Split out of `sink` into this lib, specifically so `source` (for
`FileReplaySource`) and `sink` (for `FileRecorder`) both depend downward on
one codec instead of `source` depending on `sink` directly. Full rationale
at root.
