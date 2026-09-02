# Mission

Build a modular trading platform where backtest and live run the same code, so a strategy proven in research behaves identically in production. The edge is signal quality, honest cost modeling, and operational discipline. The platform, correct and deterministic and trustworthy, is the durable asset; strategies are experiments run on top of it.

Venue- and asset-agnostic by design. Binance USD-M perpetuals and medium-frequency holding periods are the current target, not a constraint.

## Strategy arc

Developed in order of increasing sophistication and data appetite: funding / basis carry, then statistical arbitrage, then order-book and ML microstructure. A market-data collector runs continuously, so the data for later strategies accumulates while the earlier ones run.

## Success, in honest tiers

1. **Determinism proven.** A backtest and a paper-live run of the same strategy reconcile to the same decisions.
2. **Unattended carry.** A delta-neutral funding / basis strategy running 24/7 on a VM with a working risk kill-switch.
3. **Live and net-positive.** That strategy on small real capital, net positive after realistic fees, funding, and slippage.
4. **Research velocity.** A suite fast enough to iterate signals against a deepening archive of self-collected data.

Profitability is the target, not a promise. Most strategies die in honest backtesting, and that filter working is the system succeeding.

## Non-goals

- No strategy that only survives an optimistic backtest.
- No live capital before paper-parity and a proven risk layer.
- No selling or redistributing collected market data.
