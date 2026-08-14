# Strategy families and sequencing

## Why MFT, not HFT

On VMs we are out of the tick-to-trade race by construction (cloud round-trips
are ms, HFT is µs/ns with colo + kernel-bypass + FPGAs). So the target is
**medium-frequency** (seconds → days), where the edge is signal quality and
execution discipline, not speed. The C++ engineering still matters — applied to
**correctness and throughput**, and to a live feature engine later — just not to
nanosecond racing.

Asset class: **crypto perpetuals** — free live L2, no equities/CME data-licensing
hell, 24/7 markets (more data per wall-clock day), cheap VMs near the exchange.

## The families (worst-fit last)

### 1. Funding-rate / basis carry — FIRST
Hold spot + short perp (or reverse), delta-neutral, collect funding. Not a
prediction; a structural risk-premium harvest.
- **Data:** funding + klines (free). No depth.
- **Frequency:** per funding interval (8h) or slower. Latency irrelevant.
- **Small-capital fit:** good; delta-neutral, gentle drawdowns; modest absolute
  returns, fees/borrow matter.
- **Design load:** low — two-leg execution + margin/collateral tracking.
- **Why first:** shortest path to a *complete* system (data→signal→risk→exec→
  live) with the least that can go wrong. Risks: funding flips sign;
  counterparty (holding balances on exchange).

### 2. Statistical arbitrage / mean reversion — SECOND
Trade the spread between cointegrated assets; bet on reversion.
- **Data:** klines/trades (free); no depth unless pushed to second-scale.
- **Frequency:** minutes–days. Forgiving.
- **Small-capital fit:** fine; more instruments = better diversification.
- **Design load:** introduces the **research layer** — cointegration tests,
  rolling hedge ratios (Kalman filter), portfolio construction; more positions
  → first real OMS workout.
- **Trap:** backtest lookahead in rolling stats fabricates edge. Regime breaks
  (correlation stops holding).

### 3. Cross-sectional factor (momentum/carry across alts) — OPTIONAL
Rank a universe each period; long top / short bottom.
- **Data:** klines across many symbols (free); no depth.
- **Frequency:** hourly/daily. Forgiving.
- **Small-capital fit:** *wants breadth*, but many tiny positions → fees dominate
  at £1–10k. Fights the capital constraint.
- **Design load:** universe management, ranking, rebalance scheduler.
  Survivorship/delisting bias is a subtle poison.

### 4. Short-horizon directional ML (microstructure) — LAST
Predict next-N-sec/min move from book shape + trade flow; execute passively.
- **Data:** **self-collected L2 depth** + trades. The one family gated on the
  collector having run for months. (Non-depth features — trade-flow imbalance,
  vol, momentum — can use free trade history *now*.)
- **Frequency:** sec–min; execution quality matters (not HFT).
- **Small-capital fit:** hardest — high turnover, fees/slippage brutal; signal
  must clear ~5–10 bps round-trip.
- **Design load:** biggest — live **feature engine** (C++ shines), training
  pipeline (Python) separate from live inference (C++/ONNX), and the nastiest
  backtest problem (realistic fills against the book).

### AVOID
Top-of-book market making, latency arb, anything where the edge is queue
position. Needs infra we've ruled out; a VM loses by construction.

## Model types (relevant mainly to family 4, lightly to 2)

- **Linear / regularized (Ridge/Lasso):** baseline, interpretable, hard to
  overfit, cheap live. A shockingly strong benchmark.
- **Gradient-boosted trees (LightGBM/XGBoost):** the tabular-finance workhorse;
  strong, fast inference, robust on limited data.
- **Small nets (temporal CNN / small LSTM / tiny transformer):** only once L2
  data is deep and simpler models leave signal on the table; data-hungry,
  overfit-prone.
- **Classical stochastic (Ornstein–Uhlenbeck, Kalman):** the principled backbone
  for family 2.

## The through-line

At £1–10k the enemy is **cost per trade vs edge per trade**. That ranks
everything: low-turnover carry/stat-arb clear fees and suit the capital;
high-turnover ML fights for every bp. Scarce resources are **time (5–8 h/wk)**
and **months of L2 history** (only obtainable by starting the collector now).

Ordering = a sophistication ramp *and* a data-accumulation ramp. By the time
families 1–2 are built (~3–4 months), there's enough L2 to **build and validate
the ML pipeline** — not necessarily enough to *trust* a microstructure signal
(regime coverage wants a year+). Prototype early on free + short history; deploy
for real once the archive is deep.
