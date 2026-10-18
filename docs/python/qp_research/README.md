# qp_research

The python research package. Archive data in, panels and books out, every number scored by one set of statistics.

```
engine ─▶ data ─▶ panel ─▶ book, event ─▶ net returns ─▶ stats, gate
           ▲                 ▲
         cache             costs
```

## Components

- `engine`. Locates and imports the built backtest extension.
- `stats`. Sharpe, return, volatility, drawdown, t, IC, per-year tables and clustered OLS on period returns.
- `gate`. The signal gate's statistical checks, the stamp, and `score`, which runs them on one book.
- `calibrate`. Pass rates of each check on simulated books with known edges.
- `cache`. Fetched data on local disk, one parquet file per dataset, symbol and span, evicted least recently read past a size cap. Read lazily with polars.
- `data`. Datasets fetched through `cache`, as pandas or as a lazy polars `scan`.
  - `binance`. Klines, mark and premium klines, funding, metrics and open interest per coin from the Binance archive through the qp source.
  - `universe`. Which symbols count per market. Listings, the non-crypto perps, redenominations.
  - `etf`. Daily bars of a fixed US ETF list.
- `panel`. Bar rules and funding in long form at any bar size, `long`, with trailing vol and liquidity, `rolling`, the market's return and each coin's residual beyond it, `market`, wide bar-level panels built once from it, then period panels of returns, PnL, liquidity, volatility, funding and halts at any rebalance period.
- `costs`. Cost models pricing a weight change in one-way bps through one interface, and the measurement that fits a spread and depth ladder.
- `book`. Weights in, net returns out. One book per panel and cost model, or a sliced book holding one slice per rebalance day.
- `event`. Triggers in, trades and a daily net return series out. Barrier, reversion and time exits, an optional hedge leg, overlap rules over the book, and the effective count of independent trades.
- `condition`. Market conditions and past-only cuts on them, the gated sleeve's inputs, and the in-fold condition search.
- `ledger`. Every book a lane runs, kept as its return series, and the effective trials they are worth.
- `lane`. A lane's folder, spec and ledger. Refuses a primary or registered trial before the spec is committed.
- `cv`. Purged and embargoed walk-forward splits over pooled rows, and the three-fifths split.
- `plot`. The house chart style and colours.
- `nb`. Notebooks built from python cells and executed in place, failing on any cell error.

## Milestones

- [x] ~~Package~~
  - [x] ~~Skeleton, `engine`, `stats`~~
  - [x] ~~`gate` and its calibration~~
  - [x] ~~`cache` and `data`~~
  - [x] ~~`panel`~~
  - [x] ~~`costs`~~
  - [x] ~~`book` and `condition`~~
  - [x] ~~`cv`, `plot`, `nb`~~
- [x] ~~Trend notebooks ported, outputs matched~~
- [x] ~~Trial ledger~~
- [x] ~~Polars data path and long-form bars, trend outputs matched~~
- [x] ~~Event book on long-form bars, gate calibration held on simulated event books~~
- [x] ~~Market residuals, hedged event trades and the reversion exit~~
- [x] ~~Trend late check rebuilt as return series in the trend ledger~~
