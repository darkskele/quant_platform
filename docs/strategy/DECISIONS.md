# strategy decisions

1. `Strategy` is a static-dispatch concept depending only on `MarketEvent`/`StateView`/`Intent`, never a concrete adapter.
2. `FundingCarryStrategy` is the first variation: long spot, short perp, delta-neutral, collecting funding.
3. `Config` is a runtime struct, not a template param — it is backtest-swept.
4. Entry and exit funding thresholds form a hysteresis band, so funding oscillating at a single cutoff does not churn orders.
5. Stateless: every decision reads `StateView` position, so the same event yields the same intent on any worker.
6. Reacts only to `Funding` events on the futures venue; other kinds and venues emit nothing.
