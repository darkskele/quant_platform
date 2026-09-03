# Research log

What we are testing, what we ran, what we found. One entry per hypothesis, newest first. Notebooks are the scratch; this is the memory. A result here is load-bearing only once its costs are honest (see the open caveat under Funding carry).

## How to run
- Env: `conda env create -f environment.yml` then select the `qp-research` kernel.
- Module: build `qp_backtest` (VS Code task "Build: qp_backtest python module (release)"). It is a C extension, so a rebuild needs a kernel restart to take effect.
- Data: 10 symbols, 2022-01-01 to 2024-12-31, under `data/binance_historical/`. Fetch/refresh with `tools/fetch_backtest_data.sh`.

## Funding carry

Delta-neutral: spot long + futures short, harvest funding while it is persistently positive.

### Open caveat (gates every result below)
Sharpe numbers here are cost-blind: no fees, no funding paid on the short leg, no slippage in `SimExecution`. Treat every figure as an upper bound until execution costs are honest. This is the next build.

### H4 signal research, basis leads funding (2026-09-03) - mixed, symbol dependent
Steps 1 and 2 of the handoff plan, run in `research/funding_signal.ipynb`.

- Persistence check. Funding autocorrelation is strong for every symbol, far outside the noise band even a month out. Not fast regime switching, more a slow drifting mean, consistent with H1's known regime split.
- Basis mechanism check. Applying Binance's real formula, interest rate plus clamp, to a spot close proxy for index price reproduces actual funding closely, R2 0.81 against the true formula. Confirms the mechanism, basis genuinely drives funding through the clamp.
- Predictive check, does basis add value beyond funding's own history. For BTC and most symbols, almost nothing. Basis lift over a funding only model, lag plus EWMA plus volatility, is 0.001 to 0.009 in R2, and walk forward testing shows it often hurts out of sample rather than helping.
- SOL is the exception. In sample lift 0.031, and it mostly holds up walk forward, real lift in four of five folds, sometimes large. The one fold where it hurts spans the FTX collapse in November 2022, a genuine regime break, not an artifact.

Read. Funding's own trailing history, not basis, carries almost all the predictive power for calm symbols. Basis adds real value only for volatile, frequently clamped symbols like SOL, and even then is less reliable around extreme dislocations. Not the blanket mechanism backed signal the plan expected going in, narrower and symbol dependent than that.

Next. Decide whether to source Binance's real composite index price, worthwhile for SOL like symbols specifically, not a blanket need. Move toward the plan's step 3 onward for the funding only feature set, since that is carrying the real signal.

### Signal research phase begun (2026-09-03)
Three hypotheses (H1-H3, below) exhausted what config tuning and symbol-count can do to `funding_carry_strategy.hpp` — a stateless, single-direction on/off gate on the instantaneous funding print, with no model of funding itself. Handoff plan at `funding_carry_signal_research_plan.md` (repo root) sets the next phase: pure-stats signal research (autocorrelation, basis-leads-funding) against raw data in pandas, no C++ engine, before any new strategy is built. Working notebook: `research/funding_signal.ipynb`.

### H3 continued: full 10-symbol book dilutes Sharpe (2026-09-03) — CONFIRMED
Same per-symbol carries, all 10 symbols summed into one book.
- Portfolio Sharpe drops from 3.64 (BTC+ETH) to 2.96 across all ten. Per-symbol spread: BTC 3.1, LINK 2.4, ETH 2.6 carry the book; XRP 0.0, AVAX 0.37, BNB 0.68, ADA 0.8 add close to nothing. Correlations stay low across the board (mostly 0.0-0.4) — not a correlation problem, several symbols simply have no funding-carry edge to contribute and dilute the book.
Read: the lift is risk-parity math over whichever symbols actually have edge, not a reason to add symbols indiscriminately. Confirms and closes out H3 (see caveat above: engine-driven iteration is exhausted, moving to signal research).

### H3 diversification lifts Sharpe (2026-09-03) — CONFIRMED, with caveats
Sum independent single-symbol carries into one book.
- BTC+ETH: per-symbol Sharpe 3.1 / 2.6 (avg 2.85), portfolio 3.64, correlation 0.22. Lift matches risk-parity math, not new alpha.
Caveats: daily-sampled Sharpe overstates vs per-minute; the regime is shared, so all legs fall together in 2022-23 and rise in 2024. More symbols smooth the ride, they do not diversify the regime.
Next: rerun on all 10 and read the correlation matrix — which alts actually decorrelate vs which just cluster with BTC.

### H2 config tuning improves carry (2026-09-02) — REJECTED
Optuna over entry/exit funding thresholds and target qty.
- Best train Sharpe ~0.10; validation no better. Tuning moved almost nothing.
Read: the edge is not in the config. Carry is regime-dependent; no threshold fixes a regime. Stop tuning the config.

### H1 baseline single-symbol carry (2026-09-02) — established
First baselines taken per symbol. Regime-dependent: positive in positive-funding bull, idle-to-negative otherwise. Recorded as the reference the above are measured against.
