# strategy decisions

1. `Strategy` is a static-dispatch concept depending only on `MarketEvent`/`Portfolio`/`Intent`, never a concrete adapter.
2. `FundingCarryStrategy` is the first variation: long spot, short perp, delta-neutral, collecting funding.
3. `Config` is a runtime struct, not a template param, it is backtest-swept.
4. Entry and exit funding thresholds form a hysteresis band, so funding oscillating at a single cutoff does not churn orders.
5. Stateless: every decision reads position off the `Portfolio`, so the same event yields the same intent on any worker.
6. Reacts only to `Funding` events on the configured futures leg; other kinds and instruments emit nothing.
7. `PythonStrategy` acquires the GIL per call and forwards to two `py::object` callables. Single-threaded backtest scope.
8. A `py::none` callback slot, or a `None` return from a callback, means an empty intent iterable, so a strategy with only `on_event` or only `on_timer` needs no dummy callable.
