# Decisions

1. One package, installed editable into `qp-research`, tested with pytest on synthetic data with known answers.
2. The extension is found under `QP_BUILD_DIR` when set, else the release build.
3. Annualisation is read from the index spacing. Weekly 52, daily 365, hourly 8760. Anything else is passed explicitly or refused.
4. Sharpe uses the population standard deviation, the convention the gate's thresholds were calibrated on. The t statistic uses the sample one.
5. A series with no spread or fewer than three values has no Sharpe.
6. Drawdown is measured on summed returns from a starting peak of zero, so an opening loss counts.
7. IC is the cross-sectional rank correlation per period, dropping periods with fewer than 20 names.
8. `score` always runs S1, S2 and V1. S3, S4 and S5 run when their inputs are given and are stamped open when not.
9. V1 fails when the bootstrap's 5th percentile Sharpe is at or below zero.
10. Calibration is a regression test. A change to a check that moves its simulated pass rates fails the suite.
11. Calibration takes the span and cadence of the book being judged. Volatility clusters are set per year, so they last as long in time at any cadence.
12. The cache lives outside the repo. Root `QP_CACHE_DIR`, else `~/.cache/qp-research/store`. Cap `QP_CACHE_MAX_GB`, else 50.
13. A partition is one dataset, one symbol and one span, laid out `<venue>/<market>/<dataset>/symbol=<S>/year=<Y>[/month=<M>]/part.parquet`.
14. Daily and slower klines and funding partition by year. Intraday klines and metrics partition by month.
15. The index is sqlite beside the partitions, so concurrent readers and writers are safe and a read updates its partitions' last read time in place.
16. Each partition records the time it is complete through. A request refetches only partitions not complete as far as it needs, capped at what the archive has published.
17. A partition with no rows is recorded, so a symbol not listed in a span is never refetched.
18. Past the cap the least recently read partitions are evicted. Partitions the current request read are spared.
19. Daily files are taken as published one day after they close, monthly files seven days after.
20. Closed months come from monthly files, later days from daily files. Kline days missing from a monthly file inside a symbol's span are refilled from the daily files.
21. A failed archive fetch raises and caches nothing.
22. A daily bar is dated the day it covers. A kline also carries its close as `t`.
23. Listings come from the qp source's archive listing, union the monthly and daily folders, and are kept for a day. Only the non-crypto tags come from the exchange's REST info in python.
24. An ETF window is fetched and cached whole, since adjusted closes are rewritten whenever a fund pays.
25. Tests that stream from the live archive are marked `network` and run only with `-m network`.
26. Bar-level panels are built once and shared by every rebalance period built from them.
27. A signal return needs the bar and the bar before it traded, and skips each coin's first 30 traded bars. A period needs 5 valid bars.
28. PnL returns span halts and book the gap on the bar trading resumes. Redenomination days are dropped.
29. Liquidity is the 30-bar median dollar volume and volatility the 12-period spread of returns, both as of the prior period's end.
30. A funding print is placed on the bar it falls in, then summed over the period like the bars.
31. A coin idle on a period's last bar is halted only if it trades again later.
32. A cost model prices a weight change in one-way bps from the coin's liquidity. Flat, a provisional band, and a taker off a ladder.
33. Taker cost is the market's fee, half the spread and half the trade over the depth within 1%, with spread and depth read off log-log lines in daily dollar volume.
34. Fees are the base tier taker rate per market, 5 bps on USD-M and 10 on spot.
35. The perps ladder is a named constant at full precision, fitted on 72 perps over two days at four dates from 2023 to 2026.
36. A cost sample is cached per coin and date, since measuring streams every trade and book snapshot of those days.
37. A book always carries an explicit cost model. There is no default cost.
38. A weight set at a period's close earns the next period's return and pays its funding. Cost is charged one way on every weight change.
39. A halted coin keeps its weight until it trades again.
40. A sliced book's slice weights come from a function of the slice's own panel, so each slice signals on its own rebalance day.
41. A slice books moves by bar as weights drift, and funding and cost on its period's last bar. Slices are averaged by bar and summed to the reporting period.
42. A sleeve is the gated book's returns on its on periods, with the exit cost booked to the last on period.
43. Condition cuts are expanding quantiles of the condition's own earlier periods.
