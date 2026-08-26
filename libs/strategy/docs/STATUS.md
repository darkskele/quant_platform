# strategy status

- ~~G1 — Build the `Strategy` concept~~
- ~~G2 — Funding-rate/basis carry strategy~~

## Last proof

**`FundingCarryStrategy` built** (G2, D45) — `carry/` village:
`Config`-constructed, satisfies `Strategy`; long spot + short perp sized by
`target_qty` once `funding_rate` clears `entry_funding_rate`, flattened at
`exit_funding_rate`, held (reads current `StateView` position) in between.
Placeholder threshold/size defaults, refined by backtest later — not yet
built/run this pass (see CLAUDE.md's build workflow). `qp_carry_tests`:
ignores non-`Funding`/wrong-symbol/wrong-venue events, entry/exit/hold
threshold cases, `on_timer` no-op. `qp_carry_bench` added: entry/hold/reject
paths on `on_event` — not latency-critical live (funding ticks ~8h), but a
backtest sweep calls this once per historical `Funding` event per swept
config, so throughput still matters.

**`Strategy` concept built** (G1, D27/D28) — `on_event`/`on_timer`
signatures, static dispatch, depends only on `MarketEvent`/`StateView`/
`Intent`. `qp_strategy_tests` passing (`NoopStrategy` satisfies the concept,
emits no intents).
