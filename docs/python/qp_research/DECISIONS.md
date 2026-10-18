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
25. Tests that stream from the live archive are marked `network`. They run by default and `-m 'not network'` skips them.
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
44. Purged splits walk forward over distinct stamps, so rows sharing a stamp always land in the same fold.
45. A built notebook runs on the qp-research kernel and is executed in place with errors captured, then refused if any cell failed.
46. A trial is keyed by its name and settings, or by its returns when unnamed. The latest record of a key replaces earlier ones.
47. A ledger is append-only zstd parquet in the lane folder, committed. Recording identical returns writes nothing.
48. Kinds are primary, exploratory, registered and check. Checks are kept and never counted. A run at other than full cost is a check.
49. A book with a ledger records every run. Runs inside a sleeve, a slice or a by-bar view are not recorded, the composed result is.
50. `gate.score` takes the ledger in place of a trial count.
51. A primary or registered trial needs the lane's spec.md tracked and unchanged since its last commit.
52. The ledger's last buffer is written at exit, single threaded.
53. Above 1,000 trials, effective trials draws only in the directions with variance, since a family larger than its periods has at most as many. Below that the full draw is kept, so smaller counts never move.
54. The cache and data layers read with polars. `scan` returns a lazy frame for work too large to hold, such as hourly bars across the universe. The named loaders collect it into pandas.
55. Bar rules are computed in long form, one row per coin and bar, so they hold at any bar size. The wide bar panels are scattered from it. The bar before a coin's bar is the previous bar any coin has.
56. A copy of the repo without `.git`, as on the build box, checks a lane's spec against `.git-head`, `git ls-tree -r HEAD` written at sync.
57. In long form a funding print is paid on the bar it falls in, and a print inside a halt on the bar trading resumes, where the halt's gap is booked. Prints before a coin's first bar or after its last are dropped. The wide daily panels keep a halted day's print on that day.
58. In long form, vol and liquidity are the spread of signal returns and the median dollar volume over a trailing span of time, as of the coin's bar before. Spans are in time, so a missing bar shortens a window. The spans are the lane's, there are no defaults.
59. Every cost model prices through `bps(liquidity, dw)`, elementwise over a frame, series or array of one shape, so weight books and event books share models and a new model only implements `bps`.
60. An event trade fills at its trigger's close, or delay bars later, and exits at the first close past its profit target or stop, in units of entry vol, or at its time stop. Barriers are read on closes only, so a gapping stop fills at the close.
61. A halted coin exits on the bar trading resumes, a coin that ends on its last bar.
62. Overlap rules run as a walk in time order after every candidate's exit is found. At most per_coin open trades in a coin and slots in the book, stronger triggers first at one close. Each trade holds an equal slot, so the book is at most fully invested.
63. An event book's liquidity is scaled to a day for its cost model, since the ladder is fitted on daily dollar volume.
64. An event book's daily series marks open trades to market. Its effective trade count sums each trade's average uniqueness, one over the trades open in the book beside it on each bar.
65. An event book is scored by the gate on its daily series marked to market, with no change to the checks. Simulated event books with clustered triggers and agreeing sides hold the daily calibration, and the effective trade count is reported, not deflated by.
66. Every python test lives under `python/tests` and runs under pytest, the built module's own tests in `python/tests/engine`. They skip where the module is not built.
