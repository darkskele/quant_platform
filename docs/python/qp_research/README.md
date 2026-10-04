# qp_research

The python research package. Archive data in, panels and books out, every number scored by one set of statistics.

```
engine ─▶ data ─▶ panel ─▶ book ─▶ net returns ─▶ stats, gate
                            ▲
                          costs
```

## Components

- `engine`. Locates and imports the built backtest extension.
- `stats`. Sharpe, return, volatility, drawdown, t, IC, per-year tables and clustered OLS on period returns.
- `gate`. The signal gate's statistical checks, the stamp, and `score`, which runs them on one book.
- `calibrate`. Pass rates of each check on simulated books with known edges.

## Milestones

- [ ] Package
  - [x] ~~Skeleton, `engine`, `stats`~~
  - [x] ~~`gate` and its calibration~~
  - [ ] `cache` and `data`
  - [ ] `panel`
  - [ ] `costs`
  - [ ] `book` and `condition`
  - [ ] `cv`, `plot`, `nb`
- [ ] Trend notebooks ported, outputs matched
- [ ] Trial ledger
