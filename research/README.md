# Research log

What we are testing, what we ran, what we found. One entry per hypothesis, newest first. Notebooks are the scratch, this is the memory.

`GATE.md` is the bar. A result is load-bearing only once it passes, and nothing goes live until it passes in full. Every entry from here on carries its gate status, and a number with no stamp is a probe, not a finding.

## How to run
- Env: `conda env create -f environment.yml` then select the `qp-research` kernel.
- Module: build `qp_backtest` (VS Code task "Build: qp_backtest python module (release)"). It is a C extension, so a rebuild needs a kernel restart to take effect.
- Layout: shared modules and the cost table notebook at this level, one folder per strategy below it, `funding_carry/` first.
- Data: 10 symbols, 2022-01-01 to 2024-12-31, under `data/binance_historical/`. Fetch/refresh with `tools/fetch_backtest_data.sh`.

## Roadmap

Strategies ordered by the data they need, least first. Each tier is a step change in data and nothing in a tier waits on the next.

| tier | data | size and cost | unlocks |
|---|---|---|---|
| 0 | on disk now, 10 symbols, 1m spot and perp, 2022 to 2024, cost table | 0 | trend on 10 perps, basis-dislocation entry for carry |
| 1 | free bars, every perp and spot pair, 1h from 2020 and 2017, funding, 5m open interest and positioning | 1 to 10 GB, free, local | wide-universe trend, cross-sectional stat arb, cross-sectional funding, positioning signals, regime features |
| 1.5 | live recorder collecting bookTicker, forceOrder, open interest from day one | grows about 1 GB a day, free | future maker, liquidation and spread research |
| 2 | free depth and aggTrades, all markets | 2.3 TB, $15 to $40 a month, remote store | per-symbol cost table, book-walking matcher, trade-flow signals, intraday reversal |
| 3 | free bookTicker, May 2023 to April 2024 only | plus 1.4 TB | maker fill model, spread capture, adverse selection |
| 4 | paid L2 increments, liquidations, options | $700 a month and up | book-imbalance, liquidation cascades, vol premium |

The remote store is a tier 2 need. Tiers 0 and 1 live in the shared data dir.

Strategy order within tiers 0 and 1.

1. Time-series trend. Vol-scaled, weekly rebalance, per perp. Tier 0 now, tier 1 later.
2. Cross-sectional stat arb. Residual reversal, momentum and funding, one sort framework. Cointegrated pairs only if the residual axis moves. Tier 1.
3. Open interest and positioning signals, standalone and as regime features. Tier 1.
4. Basis-dislocation entry for carry. Tier 0.
5. Intraday flow and reversal. Tier 2.
6. Maker and market making. Tier 3.
7. L2, liquidation and options strategies. Tier 4.

Lanes are git worktrees, one per chat. Main is carry. `alpha-research` is trend. `xs-research` is cross-sectional stat arb. `data-store` builds the store layout on tier 1 data so the bucket is a config change later. `live-path` is the C++ live lane.

## Funding carry

Delta-neutral: spot long + futures short, harvest funding while it is persistently positive.

### Open caveat, gate fail L4 (gates every result below)
Sharpe numbers here are cost-blind. No fees, no funding paid on the short leg, no slippage in `SimExecution`. Treat every figure as an upper bound until execution costs are honest. This is the next build.

### Pooled vs per-symbol, funding_signal 1-row purge (2026-09-07) - pooling wins decisively, signal-notebook read stands

Followups to the same review, with matching markdown-cell rewrites so both notebooks read cleanly against the current numbers.

Pooled v5 elasticnet (fit once with symbol dummies + basket features) vs a private elasticnet per symbol on v5 features, per-symbol view. Pooled wins on every symbol.

- Mean pooled rank IC 0.786 vs per-symbol 0.677, +0.11.
- Mean pooled R2 0.530 vs per-symbol 0.069, +0.46.
- dir_acc is a wash (0.883 vs 0.891).

SOL is the extreme case, per-symbol R2 collapses to -1.5 (its ~500-row train window straddles FTX and the private fit cannot pool through it). Pooling is not just sample-size, the basket features (`basket_spread`, `basket_z_*`, `basket_rank_*`) carry real cross-symbol information the per-symbol fit throws away.

`funding_signal.ipynb`'s local `walk_forward_r2` gained a 1-row purge (target is `funding_next=shift(-1)`, so the last train row's label sat at the first test row). Numbers moved by less than 0.01 R2 anywhere; the H4 read (basis matters for SOL, not for BTC or most symbols) stands unchanged.

### Horizon-fixed rerun and LightGBM re-eval (2026-09-07) - GBM wins on rank IC and dir_acc, ties on R2

`funding_model_experiment.ipynb` rerun with `walk_forward_splits(..., horizon=HORIZON=24)` at every call site (previously default 1). New GBM cell with per-fold early stopping and a 5-config sweep on v5 features.

Horizon fix moved almost nothing. v5 elasticnet_l1r0.7 went from prior 0.47 R2, 0.72 rank IC to 0.4545 R2, 0.7199 rank IC. Purge added roughly 7% of rows per fold, not enough to shift the winner. Reviewer overpredicted the damage. The fix is still correct methodology.

LightGBM with proper early stopping is genuinely competitive on v5. Best config gbm_lr0.02_l15 (`num_leaves=15`, `min_data_in_leaf=500`) converges at ~380 boost rounds:

- rank IC 0.7577 vs elasticnet's 0.7199, +0.038
- dir_acc 0.9163 vs 0.8831, +0.033
- R2 0.4275 vs 0.4545, -0.027

Rank IC and dir_acc are what a signal-gated carry strategy actually cares about (ordering, sign). R2 is scale fit and trees underperform there by design. Old three-config GBM sweep was under-regularized and had no early stopping, hence the earlier "trees do not help" read.

Read. LightGBM is a live option for the final v5 signal, not a curiosity. The scale-fit loss is small; the rank IC and dir_acc lift is real. Two open items before adoption. Rerun on `net_funding_after_costs` once the honest cost matcher lands (this is the load-bearing final check). Sort export path, `booster.dump_model()` codegens to nested if/else or we bind libLightGBM directly.

Housekeeping done in the same pass. `research/funding_carry/models.py:94` dead `if False else` deleted. `research/funding_carry/features.py` gained a `_test_add_cum_target` self-check runnable via `python research/funding_carry/features.py` (verifies row-t label = sum of next 24 realized). See the follow-up entry above for the `funding_signal.ipynb` 1-row purge and the pooled-vs-per-symbol side-by-side.

### External review of the signal-research pipeline (2026-09-07) - one real bug, one deferred, rest is polish

Independent read of `funding_signal.ipynb`, `funding_model_experiment.ipynb`, `research/cv.py`, `features.py`, `models.py`.

Load-bearing items.
- Horizon/purge mismatch. `add_cum_target(horizon=24)` labels but `run_registry` calls `walk_forward_splits(..., embargo=5)` without passing `horizon`, defaulting to `1`. The purge is 23 rows too narrow. Adjacent train rows share up to 23/24 forward prints with test rows, inflating R2 and rank IC on every model. Fix at the call site, re-run the frozen-winner selection, log the new numbers. Fixes the cross-symbol pooling concern too (purge is by ts-index, was just wrong-sized).
- Cost-blind objective. Every headline number is on gross cumulative funding, not `net_funding_after_costs`. This is what the honest cost matcher build addresses. Final signal check before v5 becomes a strategy input is: same features, same CV, target swapped to cost-aware PnL.

Worth doing before re-freezing v5.
- Export-pipeline sanity check. Train on folds 1..N-1 with the exact `Pipeline(StandardScaler, Ridge(1.0))` used in the export, evaluate on fold N. One number. If it matches the CV mean within noise, ship. If not, the artifact's live behavior is not what CV measured.
- Drift diagnostic. PSI on the top few features between the fold-1 train window and each test fold. Catches scaler miscalibration silently ruining post-FTX folds.
- GBM re-run. LightGBM with `early_stopping_rounds` and per-fold hyperparameter search on the same features and CV. Three configs no-early-stopping was thin evidence to dismiss trees. If it wins on cost-aware PnL, adopt it. Export path is `dump_model()` code-gen or a libLightGBM bind, not a blocker.

Polish.
- Persistence-baseline framing. R2 -0.27 vs 0.47 headline overstates the win. Rank IC 0.664 vs 0.724 is the honest lift (0.06). Reprose only.
- `research/funding_carry/models.py:94` has a dead `if False else` branch. Delete.
- Assert `add_cum_target` row-t label equals `sum(realized_funding[t..t+23])` on a known symbol. Cheap insurance.
- `research/funding_carry/features.py:41` groupby-of-a-re-sorted-parent pattern is legible, refactor is optional.

Not urgent, no action.
- Cross-symbol pooled vs per-symbol side-by-side comparison. Nice-to-have, not a bug.

Read. Methodology is sound in shape. The horizon bug is the one likely to move the headline numbers. Everything else is scope or write-up. The move to honest costs before locking in v5 is exactly the right ordering.

### H4 signal research, basis leads funding (2026-09-03) - mixed, symbol dependent
Steps 1 and 2 of the handoff plan, run in `research/funding_carry/funding_signal.ipynb`.

- Persistence check. Funding autocorrelation is strong for every symbol, far outside the noise band even a month out. Not fast regime switching, more a slow drifting mean, consistent with H1's known regime split.
- Basis mechanism check. Applying Binance's real formula, interest rate plus clamp, to a spot close proxy for index price reproduces actual funding closely, R2 0.81 against the true formula. Confirms the mechanism, basis genuinely drives funding through the clamp.
- Predictive check, does basis add value beyond funding's own history. For BTC and most symbols, almost nothing. Basis lift over a funding only model, lag plus EWMA plus volatility, is 0.001 to 0.009 in R2, and walk forward testing shows it often hurts out of sample rather than helping.
- SOL is the exception. In sample lift 0.031, and it mostly holds up walk forward, real lift in four of five folds, sometimes large. The one fold where it hurts spans the FTX collapse in November 2022, a genuine regime break, not an artifact.

Read. Funding's own trailing history, not basis, carries almost all the predictive power for calm symbols. Basis adds real value only for volatile, frequently clamped symbols like SOL, and even then is less reliable around extreme dislocations. Not the blanket mechanism backed signal the plan expected going in, narrower and symbol dependent than that.

Next. Decide whether to source Binance's real composite index price, worthwhile for SOL like symbols specifically, not a blanket need. Move toward the plan's step 3 onward for the funding only feature set, since that is carrying the real signal.

### Signal research phase begun (2026-09-03)
Three hypotheses (H1-H3, below) exhausted what config tuning and symbol-count can do to `funding_carry_strategy.hpp` — a stateless, single-direction on/off gate on the instantaneous funding print, with no model of funding itself. Handoff plan at `funding_carry_signal_research_plan.md` (repo root) sets the next phase: pure-stats signal research (autocorrelation, basis-leads-funding) against raw data in pandas, no C++ engine, before any new strategy is built. Working notebook: `research/funding_carry/funding_signal.ipynb`.

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
