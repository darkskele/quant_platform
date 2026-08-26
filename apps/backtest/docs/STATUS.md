# backtest status

- G1 — Wire `FundingCarryStrategy` through a real `Engine` composition, end to end
  Built, not yet run this pass (see CLAUDE.md's build workflow).
- G2 — A real backtest run against real recorded data
  Not started — gated on the collector having actually accumulated
  `futures/`/`spot/` history (`apps/collector/docs/STATUS.md`'s G5).

## Last proof

**`apps/backtest` built** (G1, D48) — `FileReplaySource` (both legs,
filtered to one symbol) → `CombinedTransport` → `Engine<..., BasicRiskGate,
1, FundingCarryStrategy>`. `resolve_symbol_id` cross-checks the futures/spot
`symbols.manifest`s agree on the traded symbol's id before wiring anything
— apps/collector's D41 has both legs independently intern the same
`config.symbols` list in the same order, so they *should* agree, but
nothing in the file format enforces it; a mismatch throws rather than
silently reading one leg's BTCUSDT as the other's ETHUSDT.

`qp_backtest_integration_tests`: writes synthetic futures+spot data via a
real `FileRecorder` (not a mock — matches the collector integration test's
"synthetic writer, real reader" shape), replays it through the full
composition, and asserts the resulting `Results` — a `Funding` event
clearing the entry threshold produces `final_spot_position == 1`,
`final_futures_position == -1`, and `final_cash`/`final_equity` matching
the two fills' taker fees exactly (both legs fill at the same price, so
there's no PnL to net out, just the two 0.04 fees); a `Funding` event
*below* the entry threshold produces no fills at all.
