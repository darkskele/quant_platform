# strategy decisions

## D45 — `FundingCarryStrategy` scaffolded: `carry/` village, threshold hysteresis, stateless design
First concrete `Strategy` (`docs/strategy.md` family 1, D6), now buildable
once `Intent`/`Portfolio` carry `venue` (D44) — a two-leg strategy needs
independent spot/futures positions for the same `SymbolId`. `Config`
(`carry/include/funding_carry_strategy.hpp`) is a plain runtime struct, not
a template parameter — an open-ended, backtest-swept value (CLAUDE.md's
compile-time-vs-runtime split), constructed and handed in, not selected at
compile time like `AlignmentRule`/`BinanceMarket`.

Two thresholds, not one: `entry_funding_rate`/`exit_funding_rate` form a
hysteresis band — funding oscillating right at a single cutoff would churn
orders every event; between the two, the strategy holds whatever position
`StateView` already reports rather than re-deciding. Deliberately stateless
(no position field on the class itself): every decision reads
`StateView::position()`, so the same `MarketEvent` always produces the same
`Intent` regardless of which instance/thread runs it — matches D33's
`RoundRobinPool` assumption that a `Strategy` carries no hidden mutable
state across workers. Only reacts to `Funding`-kind events on its own
`futures_venue` (spot has no funding, per `docs/strategy.md`) — `BookDiff`/
`Trade`/other-venue events return no intents, not a no-op default RiskGate
would have to filter itself.

Default threshold/`target_qty` values are placeholders (a starting point
for the backtest sweep this config exists to drive), not tuned — that's the
whole reason they're runtime fields and not baked in.
