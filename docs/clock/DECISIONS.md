# clock decisions

1. `Clock` is the injected time seam: strategy and risk code call it, never the system clock, so backtest and live share one code path.
2. `SimClock` advances only when fed an event timestamp, with no wall-clock reads, so a replay is deterministic.
