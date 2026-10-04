# Signal gate

The bar a signal result clears before it is quoted as a finding. A signal is a feature, a direction and a reason, judged on net money out of sample.

Scope is the signal question only. Is there an edge, in which market condition, net of honest cost. Capacity, fill realism, live decay and live agreement belong to strategy research. Detecting a condition live belongs to regime research.

The statistical checks and the stamp have a function in `gate.py`. Their thresholds come from `calibrate_gate.py`, which runs each check on simulated books with a known edge.

## Outcomes

Every quoted result carries one stamp. A number with no stamp is a probe, not a finding.

| stamp | when |
|---|---|
| `gate pass` | every check passes and S1 is at or above 0.95 |
| `gate park, needs about N weeks` | every check passes and S1 sits between 0.5 and 0.95. Under-powered, not refuted. N is the sample at which the observed Sharpe would pass S1 with 80% power |
| `gate fail` plus the failed ids | any check fails, or S1 is below 0.5 |

A failed check is a stop. Fix it, then rerun the whole gate, not the failed item alone.

A check the data at hand cannot settle is named as open in the stamp, for example `gate park, needs about 500 weeks, open T5`. Open is not a pass. It says what data would close it.

## What a stamp allows

The stamp measures evidence. It is not a verdict on whether to trade. Six and a half years of weekly data passes a true 0.8 Sharpe only about two times in three on a single clean test, so a real, modest edge is expected to park.

- `gate pass`. Proven on data it was not chosen on. Eligible for full size.
- `gate park`. Promising, not proven. With a mechanism stated before the test, it goes to strategy research and trades at small size, paper or live, which collects the forward data a pass needs. It scales as the forward record agrees with the backtest and is cut when it diverges.
- `gate fail`. A specific check failed. Not traded until the cause is fixed and the whole gate rerun.

## Primary and exploratory

The stamp measures evidence. It is not a verdict on whether to trade. Six and a half years of weekly data passes a true 0.8 Sharpe only about two times in three on a single clean test, so a real, modest edge is expected to park.

- `gate pass`. Proven on data it was not chosen on. Eligible for full size.
- `gate park`. Promising, not proven. With a mechanism stated before the test, it goes to strategy research and trades at small size, paper or live, which collects the forward data a pass needs. It scales as the forward record agrees with the backtest and is cut when it diverges.
- `gate fail`. A specific check failed. Not traded until the cause is fixed and the whole gate rerun.

## Named and exploratory

- Each hypothesis names one test in advance, before any run. Its setting grid is the trial family S1 deflates by.
- Everything else is exploratory. It is deflated by every trial run in the lane, old grids included. Trials do not reset when the data widens.
- An exploratory result can at best park. It passes only on data it was not chosen on, a forward run or a span registered before it was looked at.
- A registered test is committed before it is run. The spec names the hypothesis, the books as coded, the data, the trials, the pass, park and fail rules, and the reviews.
- A setting with no reason to prefer one value, a rebalance day or a start phase, is staggered or averaged over, never chosen.
- A mechanism stated before the test is what earns a test its place as the named one. It is the prior, and it is why a pre-registered test pays a smaller penalty than a search.

## Conditional signals

A signal may earn only inside a market condition. It is judged on its sleeve, the periods the condition is on.

- The condition is built from past data only, with cut points from earlier periods.
- The condition is named before its results are seen, or it is chosen inside each walk forward fold. A condition picked from a full-sample map is exploratory.
- The sleeve pays its own cost of switching on and off.
- A rule that sits out part of the universe is also reported per dollar deployed.
- The sleeve's baseline is the basket held long and held short over the same periods. Both are reported.
- Every check below runs on the sleeve's periods.

## Checks

### Data

| # | check | pass criteria |
|---|---|---|
| D1 | Coverage | The span covers 5 years and one full bull and bear cycle |
| D2 | Contract continuity | Renames, redenominations and contract-spec changes come from a named list and are handled one by one. Never by a blanket rule that also hides real moves |
| D3 | Survivorship | Delisted instruments are in the universe for the span they traded. A universe of currently listed names is named as biased |
| D4 | Timestamps | One clock, UTC. A bar is labelled by its open time and delivered at its close time, after every event inside it. Funding is stamped at settlement |
| D5 | Gaps and outliers | Halts and zero-volume stretches are counted and handled, never forward-filled into a signal. A position held across a halt books the gap in its PnL even where the signal skips it |

### Time

| # | check | pass criteria |
|---|---|---|
| T1 | Look-ahead | A signal acts no earlier than the period after its inputs close. The same signal one period older does no better |
| T2 | Selection in fold | Every choice made from results, a setting, a condition, a cut, is made on data before the period it is scored on. A span used to choose is never quoted as out of sample |
| T3 | Feature lag | Features are trailing. Cross-sectional ranks use one bar for every name. Cut points come from earlier periods |
| T4 | Label and purge | A fitted model's train and test folds are purged by the full label horizon plus an embargo |
| T5 | Point in time | Universe membership, liquidity filters and cost rows are as of the bar |

### Statistics

| # | check | pass criteria |
|---|---|---|
| S1 | Significance | Probability the Sharpe is real, against the luck bar for the effective number of trials, is at least 0.95 to pass and 0.5 to park. The effective count comes from the correlation of the trials' returns, not the raw count |
| S2 | Year stability | Dropping any one calendar year leaves the Sharpe positive and at least 40% of the full Sharpe |
| S3 | Out of sample | Walk forward out of sample Sharpe is positive and not below in sample by more than chance allows, one-sided 95% |
| S4 | Neighbours | The settings one step either side of the chosen one, and 20% either side for a continuous setting, earn and trail it by no more than chance allows given their correlation |
| S5 | Selection overfit | Probability of backtest overfit across the grid is below 0.5. At 0.5 or above the grid's median setting is quoted, not the winner |
| S6 | Baseline lift | Beats doing nothing on the same costs and periods. Persistence for a forecast. The long and the short basket for a directional book |
| S7 | Dependence | Standard errors are clustered by period wherever rows share a period. The sample is counted in independent bets, not rows |

### Cost

| # | check | pass criteria |
|---|---|---|
| C1 | Fees | The venue's current taker rate per leg, checked against that market's own fee schedule |
| C2 | Spread and impact | Taken from a cost ladder measured on the tape and book depth of the market it is charged in, at a stated research book size |
| C3 | Funding | Paid and received on every open perp leg at settlement |
| C4 | Cost sensitivity | The sign survives twice the cost |

### Validation

| # | check | pass criteria |
|---|---|---|
| V1 | Bootstrap | Block bootstrap of the net path. The 5th percentile Sharpe is above zero. The share of paths ending above zero and the drawdown distribution are reported |
| V2 | Stress | The path through the sample's breaks is reported. May 2021 deleverage, May and June 2022, November 2022, March 2023, August 2024 |

## Calibration

Six and a half years of weekly returns, fat tailed with clustered volatility. Share of books passing each check.

| true Sharpe | edge | S2 year stability | S1, one named test | S1, 3 effective trials | old bar, every year above 0.5 | old bar, Bonferroni over 45 |
|---|---|---|---|---|---|---|
| 0.0 | none | 0.18 | 0.06 | 0.01 | 0.00 | 0.00 |
| 0.5 | stationary | 0.64 | 0.37 | 0.13 | 0.03 | 0.04 |
| 0.5 | one year | 0.23 | 0.36 | 0.12 | 0.00 | 0.04 |
| 0.8 | stationary | 0.86 | 0.64 | 0.35 | 0.11 | 0.17 |
| 0.8 | one year | 0.14 | 0.63 | 0.33 | 0.00 | 0.15 |
| 1.0 | stationary | 0.94 | 0.80 | 0.54 | 0.18 | 0.33 |
| 1.0 | one year | 0.10 | 0.78 | 0.50 | 0.00 | 0.29 |
| 1.5 | stationary | 1.00 | 0.96 | 0.87 | 0.43 | 0.78 |
| 1.5 | one year | 0.04 | 0.96 | 0.83 | 0.00 | 0.67 |

No edge anywhere, the best cell picked from a correlated grid. Share wrongly passing S1.

| cells | correlation | effective trials | counted as one test | deflated by effective trials |
|---|---|---|---|---|
| 5 | 0.85 | 1.7 | 0.13 | 0.07 |
| 20 | 0.85 | 2.5 | 0.17 | 0.05 |
| 20 | 0.50 | 6.7 | 0.38 | 0.03 |
| 95 | 0.60 | 11.4 | 0.47 | 0.03 |

S4 on three settings correlated 0.85 passes a flat plateau at Sharpe 0.5 81% of the time and at 0.8 87%. It fails a 0.8 spike between two flat neighbours every time.

Weeks a single named test needs to pass S1 with 80% power. 1287 at Sharpe 0.5, 504 at 0.8, 323 at 1.0, 144 at 1.5.

What it means for a signal.

- Six and a half years of weekly data cannot prove a real 0.8 Sharpe on its own. S1 passes it about two times in three on one named test. Park is the expected outcome for a real but modest edge, not a failure.
- S2 is what catches an edge carried by one year. S1 cannot tell the two apart.
- Deflating by effective trials holds wrong passes at 3 to 7% on correlated grids. Counting the grid as one test lets 13 to 47% through.
- The previous bar passed a real 0.8 edge about one time in ten, and a real 0.5 edge almost never.
