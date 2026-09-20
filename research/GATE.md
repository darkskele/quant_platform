# Backtest quality gate

Every item passes before a result is load-bearing, and every item passes before anything goes live. A failure is not a caveat, it is a stop. Fix, then rerun the whole gate, not the failed item alone.

A result quoted anywhere, in the research log, in a notebook read, in a handoff, carries its gate status. `gate pass`, or `gate fail L2.3`, or `gate partial, L1.1 L4.4 open`. A number with no stamp is a probe, not a finding.

## Layer 1, data integrity

| # | check | pass criteria | failure consequence |
|---|---|---|---|
| 1.1 | Coverage | Train plus test spans 5 years and contains one full bull and bear cycle. Tier 0 (2022 to 2024) fails this on span, tier 1 hourly (from 2017 spot, 2020 perp) clears it | Untested in the regime that kills it |
| 1.2 | Contract continuity | Perpetuals, so no roll. Symbol renames and contract-spec changes (tick size, multiplier, margin asset) are handled, not silently spliced | False signals at the splice, inflated returns |
| 1.3 | Survivorship | Delisted and deprecated perps are in the universe for the window they traded. A universe built from currently listed symbols is biased and must be named as such | Returns inflated, the dead symbols were the losses |
| 1.4 | Timestamp alignment | One clock, UTC milliseconds, across spot, perp, funding and open interest. Funding stamps are settlement time, not print time. Bar stamps are open time, consistently | Cross-market signals silently shifted, leakage or lag |
| 1.5 | Gaps and outliers | Missing bars, halts and zero-volume stretches are counted and handled explicitly, not forward-filled into a signal | A halt reads as a flat market and fabricates edge |

## Layer 2, temporal integrity

| # | check | pass criteria | failure consequence |
|---|---|---|---|
| 2.1 | Look-ahead | Signal formed on bar close T acts no earlier than T plus one bar. The engine is the arbiter, an event drives the strategy and the strategy cannot see past it | Backtest inflated 2x to 10x |
| 2.2 | Train and test separation | Strictly temporal. No shuffled split, no fit on pooled history then test inside it | Overfitting becomes undetectable |
| 2.3 | Feature lag | Every feature uses information available at or before its stamp. Rolling stats are trailing. Cross-sectional ranks use the same bar for every symbol | Future information in the feature set |
| 2.4 | Label and purge | Labels use only forward data, and the train and test folds are purged by the full label horizon plus an embargo. A 24 bar cumulative label needs a 24 bar purge, not 1 | Adjacent rows share forward prints, every metric inflated |
| 2.5 | Point-in-time parameters | Universe membership, cost table rows and any fitted artifact are as of the bar, not as of today | The backtest knows which symbols survived |

## Layer 3, overfitting detection

| # | check | pass criteria | failure consequence |
|---|---|---|---|
| 3.1 | Out of sample | OOS net return is above 50 percent of in-sample net return | Severe overfit |
| 3.2 | Parameter stability | Plus or minus 20 percent on every parameter moves return by under 30 percent. Neighbours of the best cell agree with it | A noise cell, not a setting |
| 3.3 | Multiple testing | Report how many configs were tried. Significance threshold is 0.05 divided by n | False positives dressed as strategies |
| 3.4 | Period stability | Each calendar year clears Sharpe 0.5, with no single year carrying the result | Works in one regime only |
| 3.5 | Baseline lift | Beats the do-nothing baseline on the same costs and the same folds. Persistence is the line for a forecast, buy and hold for a directional book | A metric that flatters a rule with no edge |

## Layer 4, cost modelling

| # | check | pass criteria | failure consequence |
|---|---|---|---|
| 4.1 | Fees | Real venue rates, per leg. Binance VIP0 is 4 bps futures taker and 10 bps spot taker. Maker is only claimable with a fill model that earns it | Costs understated by the whole edge |
| 4.2 | Slippage | Half spread from measured data, per symbol and per week, never a flat guess | Anything high turnover flips to a loss |
| 4.3 | Market impact | Size-dependent, square root or measured linear in the cost table. Capacity stated alongside the Sharpe | Capacity fiction, the strategy does not scale |
| 4.4 | Funding and borrow | Funding paid and received on every open perp leg at settlement. Short spot carries a borrow cost, and if it cannot be sourced the strategy cannot be run | Carry and short returns inflated |
| 4.5 | Fill realism | Partial fills, rejects and the fill price the matcher can defend. No fill at a price the book did not show | An optimistic fill is how a backtest lies |

## Layer 5, validation

| # | check | pass criteria | failure consequence |
|---|---|---|---|
| 5.1 | Walk forward | At least 10 rolling folds, purged and embargoed, refit each fold | One split is an anecdote |
| 5.2 | Monte Carlo | Resampled and trade-shuffled paths, 90 percent end above zero. Report the drawdown distribution, not just the mean | Luck read as skill |
| 5.3 | Stress | Runs through the breaks in the sample. May 2021 deleverage, May and June 2022 (LUNA, 3AC), November 2022 (FTX), March 2023, August 2024 unwind | Blows up on the first real one |
| 5.4 | Decay haircut | Half the backtest net return is still acceptable against the capital and the effort | Live expectations set at fantasy |
| 5.5 | Live agreement | Shadow or testnet run reproduces the simulated fills and costs within tolerance before size goes on | The gap only shows up with money on it |

## Quick self-check

Ask after every backtest.

- Did I use information I would not have had. Layer 2.
- Does it hold in another period and at another parameter. Layer 3.
- Is it still there after honest costs. Layer 4.
- How much of this is luck. Layer 5.

Three things are outside a notebook's reach and are measured on testnet, never claimed from a backtest. Maker fill rates, liquidation behaviour, and the short spot borrow.
