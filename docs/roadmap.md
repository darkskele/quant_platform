# Roadmap to horizon

Calibrated for **5–8 h/week**, **small capital (£1–10k)**, **paper-first**,
**carry-first**. Dates are effort-relative, not calendar-promises.

**The one thing that can't be rushed later:** the collector. Start it
immediately, before anything else in Milestone 1, so L2 history accumulates
while everything else is built.

---

## ~~Milestone 1 — Data tap + backtester + first strategy + risk gate~~
- Production streamer: `LiveWebSocketSource` (socket + book reconstruction,
  snapshot+diff, sequence-number resync, reconnect) + `FileRecorder`
  collector, both legs (futures + spot).
- `FileReplaySource`, `SimClock`, `SimExecution` with the honest cost model
  (fees, funding, slippage — partial fills still a tracked gap, D26).
- Funding/basis carry strategy (`FundingCarryStrategy`) behind the
  `Strategy` interface.
- `BasicRiskGate`: per-instrument exposure cap + equity-drawdown kill
  switch.
- `apps/backtest`: the whole pipeline wired end-to-end (D48).

**Milestone:** collector banking BTCUSDT/ETHUSDT depth+trades 24/7 with
honest gap logging; a working, tested, single-run funding-carry backtest
through a real risk gate. Not yet a walk-forward/equity-curve analysis —
that's Milestone 2.

## Milestone 2 — Analytics
- Walk-forward harness.
- PnL / Sharpe / max-drawdown reporting (`libs/analytics`).

**Milestone:** a defensible, cost-realistic equity curve for funding
carry — the actual "is this strategy any good" answer.

## Milestone 3 — Research infra + tuning
- Python/pybind11 bindings exposing a specific funding-carry backtest
  entrypoint + its `Config` — not the generic `Engine` template (template
  parameters are fixed at C++ compile time; Python can only call an
  already-instantiated binding).
- Parameter sweep tooling (entry/exit funding thresholds, position sizing),
  scored using Milestone 2's analytics.
- Historical funding-rate/mark-price data acquisition (Binance's free REST
  history) — something to actually sweep against.

**Milestone:** `FundingCarryStrategy::Config`'s values are backtested/
tuned, not the placeholder defaults they are today (D45).

## Milestone 4 — Live executor, testnet first
- `LiveExecution` against **Binance testnet**; OMS.
- Run funding carry in **paper, 24/7, on the VM**.
- **Parity check:** same strategy, backtest vs paper — reconcile decisions.

**Milestone:** unattended paper bot; backtest↔live parity demonstrated
(success tier 1 + 2).

## Milestone 5 — Live small + second strategy
- Small **real capital** on funding carry; monitoring + alerting;
  kill-switch exercised live.
- Add **stat-arb pairs** (research layer: cointegration, Kalman hedge
  ratio, portfolio construction).

**Milestone:** live with small real capital, monitored, net-of-cost
tracking; second strategy backtested (success tier 3).

## Milestone 6 — Alpha research & ML
- **Order-book feature engine** on the now-substantial self-collected L2
  archive.
- Non-depth features (trade-flow imbalance, vol, momentum) validated on
  free trade history immediately.
- ML pipeline: **Python training** separate from **C++/ONNX live
  inference**; LightGBM baseline before anything fancier.

**Milestone:** research suite iterating short-horizon signals; first ML
strategy in paper (success tier 4).

---

## Horizon summary
- **~6 months** to live-with-small-real-capital on carry, at 5–8 h/week.
- **12 months+** for the ML chapter — gated on data accumulation, not effort.
- Most strategies die in honest backtest. The **platform is the durable win**;
  profitability is a target, not a schedule.

## Critical path / dependencies
- Collector (M1) → any L2-dependent work (M6). Non-negotiable to start early.
- `SimExecution` cost model (M1) → trustworthy everything downstream.
- Analytics (M2) → a real signal for what Milestone 3 should tune toward.
- Parity check (M4) → permission to deploy real capital (M5).
- Risk kill-switch (M1, live-tested in M4) → precondition for any live capital.
