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
