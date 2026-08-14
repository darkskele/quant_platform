# Roadmap to horizon

Calibrated for **5–8 h/week**, **small capital (£1–10k)**, **paper-first**,
**carry-first**. Dates are effort-relative, not calendar-promises.

**The one thing that can't be rushed later:** the collector. Start it in Phase 0
so L2 history accumulates while everything else is built.

---

## Phase 0 — Foundations & data tap (≈ weeks 1–3)
- Repo, `CMakePresets.json`, vcpkg manifest, sanitizer preset, `.gitignore` +
  secrets loading.
- Define the **event schema** and the **seam interfaces** (headers, no impls).
- Skeleton `Engine` template that compiles for all three targets (live /
  backtest / collector) with stub policies.
- **Production streamer first:** build `LiveWebSocketSource` properly — socket +
  book reconstruction (snapshot+diff, sequence-number resync, reconnect). This is
  the real prod data path, not a throwaway.
- **Collector = streamer + `FileRecorder` sink** (binary/zstd/partitioned), no
  strategies attached. Falls out of the streamer. Run on laptop first, then VM.
- Download free Binance kline/funding history.

**Milestone:** collector banking BTCUSDT/ETHUSDT depth+trades 24/7 with honest
gap logging; repo builds the skeleton for all three binaries.

## Phase 1 — Backtester + first strategy (≈ weeks 4–9)
- `FileReplaySource`, `SimClock`, `SimExecution` with the **honest cost model**
  (fees, funding, slippage, partial fills).
- **Funding/basis carry** strategy behind the `Strategy` interface.
- Walk-forward harness; PnL / Sharpe / max-drawdown reporting.

**Milestone:** an honest, cost-realistic backtest of funding carry with a
defensible equity curve (and the discipline to believe it when it's mediocre).

## Phase 2 — Live plumbing on paper (≈ weeks 10–16)
- `LiveExecution` against **Binance testnet**; OMS; `RiskGate` with a working
  **kill-switch** + drawdown flatten.
- Run funding carry in **paper, 24/7, on the VM**.
- **Parity check:** same strategy, backtest vs paper — reconcile decisions.

**Milestone:** unattended paper bot; backtest↔live parity demonstrated (success
tier 1 + 2).

## Phase 3 — Live small + second strategy (≈ weeks 17–24)
- Small **real capital** on funding carry; monitoring + alerting; kill-switch
  exercised live.
- Add **stat-arb pairs** (research layer: cointegration, Kalman hedge ratio,
  portfolio construction).

**Milestone:** live with small real capital, monitored, net-of-cost tracking;
second strategy backtested (success tier 3).

## Phase 4 — Alpha research & ML (≈ months 7–12+)
- **Order-book feature engine** on the now-substantial self-collected L2 archive.
- Non-depth features (trade-flow imbalance, vol, momentum) validated on free
  trade history immediately.
- ML pipeline: **Python training** separate from **C++/ONNX live inference**;
  LightGBM baseline before anything fancier.

**Milestone:** research suite iterating short-horizon signals; first ML strategy
in paper (success tier 4).

---

## Horizon summary
- **~6 months** to live-with-small-real-capital on carry, at 5–8 h/week.
- **12 months+** for the ML chapter — gated on data accumulation, not effort.
- Most strategies die in honest backtest. The **platform is the durable win**;
  profitability is a target, not a schedule.

## Critical path / dependencies
- Collector (P0) → any L2-dependent work (P4). Non-negotiable to start early.
- SimExecution cost model (P1) → trustworthy everything downstream.
- Parity check (P2) → permission to deploy real capital (P3).
- Risk kill-switch (P2) → precondition for any live capital.
