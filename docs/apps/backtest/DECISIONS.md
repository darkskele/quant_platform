# backtest decisions

1. Backtest and live share one code path; only the component types differ, and they are template parameters rather than macros.
2. `BacktestBase` runs the source pump and the engine on their own threads, joining once the sources run dry.
3. Composition is runtime. A `Subscription` and a source config are constructor arguments, so a sweep changes symbols and spans without a rebuild.
4. `BacktestBase` is CRTP over the variant, so a variant supplies its own sources without a virtual call or a type-erased source.
5. An error on either thread requests stop on the shared control channel and is rethrown after the join. Without it the other thread blocks forever on a queue nobody drains.
6. No CLI entry point; a run is driven by constructing a backtest and calling `run()` (the pybind path).
7. `run()` returns a `Results` struct, not the internal `Portfolio`.
8. The python variants split into a base holding everything but the source and a variant supplying it, so a second source is a new class rather than a new backtest.
9. The python variant holds `py::object` callback slots and constructs fresh adapter instances in `make_engine()`, so between-run callback swaps and repeated `run()` calls do not need adapter mutators.
10. The python module releases the GIL around `plan()` and `run()`, so the engine and source threads can acquire it inside each callback.
11. `FundingCarryBacktest` is on an `EofSource` stub until it is wired to a real source.
