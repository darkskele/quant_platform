# qp_research

The python research package. Archive data in, panels and books out, every number scored by one set of statistics.

```
engine ─▶ data ─▶ panel ─▶ book ─▶ net returns ─▶ stats, gate
           ▲                 ▲
         cache             costs
```

## Components

- `engine`. Locates and imports the built backtest extension.
- `stats`. Sharpe, return, volatility, drawdown, t, IC, per-year tables and clustered OLS on period returns.
- `gate`. The signal gate's statistical checks, the stamp, and `score`, which runs them on one book.
- `calibrate`. Pass rates of each check on simulated books with known edges.
- `cache`. Fetched data on local disk, one parquet file per dataset, symbol and span, evicted least recently read past a size cap.
- `data`. Datasets fetched through `cache`.
  - `binance`. Klines, mark and premium klines, funding and metrics from the Binance archive through the qp source.
  - `universe`. Which symbols count per market. Listings, the non-crypto perps, redenominations.
  - `etf`. Daily bars of a fixed US ETF list.
- `costs`. Cost models pricing a weight change in one-way bps, and the measurement that fits a spread and depth ladder.
- `book`. Weights in, net returns out. One book per panel and cost model, or a sliced book holding one slice per rebalance day.
- `condition`. Market conditions and past-only cuts on them, the gated sleeve's inputs, and the in-fold condition search.
- `panel`. Bar-level panels built once, then period panels of returns, PnL, liquidity, volatility, funding and halts at any rebalance period.

## Milestones

- [ ] Package
  - [x] ~~Skeleton, `engine`, `stats`~~
  - [x] ~~`gate` and its calibration~~
  - [x] ~~`cache` and `data`~~
  - [x] ~~`panel`~~
  - [x] ~~`costs`~~
  - [x] ~~`book` and `condition`~~
  - [ ] `cv`, `plot`, `nb`
- [ ] Trend notebooks ported, outputs matched
- [ ] Trial ledger
