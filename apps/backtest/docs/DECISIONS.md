# backtest decisions

## D48 — `apps/backtest` wiring: fixed futures=0/spot=1 venue convention; cross-checked symbol resolution; `Results`, not raw `Portfolio`
First real composition root for `FundingCarryStrategy` (D45) +
`BasicRiskGate` (D46/D47), following the same fix-the-prerequisite-first
sequencing that produced them.

**Venue indices are a hardcoded convention, not user-configurable.**
`kFuturesVenue = 0`/`kSpotVenue = 1` in `backtest.cpp` reuse
`apps/collector`'s own `run_data_source(sources, sinks, ...)` pairing order
(D41/D43: futures source/sink constructed first, spot second) — the
recorded bytes on disk already have `MarketEvent::venue` stamped against
that exact order, so replaying them with any other convention would
silently mislabel every event. Not a `Config` field: it isn't a choice this
app's caller gets to make, it's a fact about how the data was written.

**`resolve_symbol_id` cross-checks both legs' `symbols.manifest`s instead
of trusting one.** `apps/collector`'s D41 has both legs' recorders
independently intern the same `config.symbols` list, in the same order —
so both manifests *should* assign the same `SymbolId` to a given name, but
nothing in the file format enforces that (each leg's `SymbolTable` is
genuinely independent, D41's own deliberate choice). Reading only one
leg's manifest and assuming the other agrees would risk exactly the kind
of silent cross-venue collision D43/D44 already fixed one layer up —
verified here instead of assumed; a mismatch throws immediately rather
than quietly pricing one leg's BTCUSDT against the other's ETHUSDT.

**`run()` returns a `Results` struct, not the `Portfolio` itself.**
`Portfolio` is an internal, flat-array-heavy representation
(`kMaxSymbols * kMaxVenues` arrays) — `cash()`/`equity()`/`position()` are
its real API, not the arrays. `Engine` doesn't expose the `Portfolio`
directly either (only `StateView`, which would dangle once `Engine` goes
out of scope at the end of `run()`). `Results` holds exactly the scalars
meaningful to report today; a real analytics layer (Sharpe, drawdown
curve, equity time series) is later, deliberately deferred work — this
is just enough to prove the wiring and give a test something to assert
against.
