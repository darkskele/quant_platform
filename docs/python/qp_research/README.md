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

## Milestones

- [ ] Package
  - [x] ~~Skeleton, `engine`, `stats`~~
  - [ ] `gate`
  - [ ] `cache` and `data`
  - [ ] `panel`
  - [ ] `costs`
  - [ ] `book` and `condition`
  - [ ] `cv`, `plot`, `nb`
- [ ] Trend notebooks ported, outputs matched
- [ ] Trial ledger
