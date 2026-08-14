# Mission

Build a modular C++ platform for **medium-frequency crypto trading** in which
backtest and live execution run the *same* code, so that a strategy proven in
research behaves identically in production. The goal is genuine, risk-adjusted
profitability from **structural and statistical edges** — not from winning a
latency race the hardware can't win. The platform itself — correct,
deterministic, and trustworthy — is the durable asset; individual strategies
are experiments run on top of it.

## Positioning

The edge is sought in **signal quality, honest cost modeling, and operational
discipline**, at holding periods from minutes to days on **Binance USD-M
perpetuals** — deliberately *not* in tick-to-trade speed. The engineering
ambition (lock-free data paths, compile-time policy dispatch, mechanical
sympathy) is spent on **correctness and throughput, not nanoseconds**.

Strategies are developed in order of increasing sophistication and data
appetite — funding/basis carry first, statistical arbitrage next,
order-book/ML microstructure last — while a market-data collector runs from
**day one** so the data for the later strategies accumulates during the earlier
ones. (See `docs/strategy.md` and `docs/roadmap.md`.)

## What success looks like (in honest tiers)

1. **Determinism proven.** A backtest and a paper-live run of the same strategy
   reconcile to the same decisions.
2. **Unattended carry.** A delta-neutral funding/basis strategy running 24/7 on
   a VM with a working risk kill-switch.
3. **Live and net-positive.** That strategy on small real capital (£1–10k), net
   positive after realistic fees, funding, and slippage.
4. **Research velocity.** A suite fast enough to iterate signals, feeding on a
   deepening archive of self-collected order-book data.

Profitability is the **target, not a promise**. Most strategies will die in
honest backtesting, and that filter working *is* the system succeeding.

## Explicit non-goals

- No HFT, market making, or latency arbitrage.
- No colocated or specialized hardware.
- No strategy that only survives an optimistic backtest.
- No live capital before paper-parity and a risk layer are proven.
- No selling or redistributing collected market data.
