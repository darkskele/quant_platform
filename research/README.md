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
