# recorder decisions

1. `Recorder` is a compile-time policy defaulting to a no-op, so the observation seam adds no vtable and costs nothing when it records nothing.
2. `sample()` receives the portfolio, not a precomputed metric, so a no-op recorder computes nothing.
3. `EquitySeriesRecorder` records off the hot path: the record thread does a bounded, allocation-free push, and a drain thread owns the allocation and the growing series. No per-event allocation, no series-length cap.
4. Order is preserved by the single-producer queue, so the recorded series is deterministic regardless of thread timing.
