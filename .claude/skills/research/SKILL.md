---
name: research
description: How to run quant signal and strategy research for quant-platform, the notebook and backtest loop, the voice, the analytical discipline, and the landmines. Load before building or editing a research notebook, running a backtest sweep, or reporting a research finding.
---

Research here judges a signal by downstream money on the C++ engine, not by a fit metric. The output is a notebook that reads clean and a plain finding. This is the process, the voice, and the mistakes already made once.

## The loop

Probe, then build, then execute. Three passes, in order.

1. Probe. A throwaway script in the scratchpad that wires the pipeline and prints real numbers. Validate the wiring and get the numbers before writing a word of interpretation. Never write a read from a guess.
2. Build. A python builder script that lists the cells with `qp_research.nb.md` and `nb.code`. Interpretation cells are written from the probe's real numbers.
3. Execute. `nb.build(path, cells)` writes and executes in place, and refuses a notebook with any failed cell. Check the tables landed.

Reuse the package and the walk-forward splits, do not re-derive them. Pickle an out-of-fold set so a rerun skips regeneration.

## The package

`qp_research` under `python/` is the research library. A notebook imports it and writes only the lane's own hypothesis. Code another lane would copy belongs in the package, with a test.

- `data`. `daily_bars`, `klines`, `funding`, `metrics` and `universe`, cached by partition. Always check the cache, fetch what is missing. Never write a loader in a notebook.
- `panel`. `panel.load()` or `panel.bars(...)` then `panel.build(bars, 'W-SUN')`. Returns, PnL, liquidity, vol, funding and halts at any rebalance period.
- `costs`. Every book carries an explicit cost model. `costs.for_market('usdm', size)` off the measured ladder, `Provisional`, `Flat`. Fees come from `costs.FEES`, never a literal.
- `book`. `Book(panel, cost).run(w)` for one rebalance day, `Sliced(bars, cost).run(weights)` for one slice per day. `inverse_vol`, `gated`, `sleeve`, `equal_risk`.
- `condition`. Market conditions, past-only cuts and the in-fold condition search.
- `stats` and `gate`. Every Sharpe, drawdown, year table, IC and clustered error comes from `stats`. `gate.score` runs the statistical checks and stamps any it was not given as open.
- `cv`, `plot`, `nb`. Splits, the house chart style, the notebook builder.
- Lane-specific signals live in the lane folder beside its notebooks, on top of the package.

## Environment

- Kernel and libs, `source ~/miniconda3/etc/profile.d/conda.sh && conda activate qp-research`. `qp_research` is installed there editable, from `python/environment.yml`.
- The engine module, `qp_research.engine.module()`. Data needs the `qp_python_backtest` build.
- Tests, `python -m pytest` from `python/`, and `-m network` for the ones that stream the archive.
- Heavy runs go in the background, `run_in_background`, watched with a Monitor until-grep loop. Foreground has a 2 minute cap.
- Piped or redirected stdout is block-buffered, so a watched file stays empty until exit. Use `stdbuf -oL` or `print(..., flush=True)` when you need to watch progress.

## Notebook voice

Prose follows the `writing` skill, terse, no em dashes, no colons or semicolons, one line per paragraph, no hard wrap.

- Match the voice of the sibling notebooks before writing a new one.
- No cross-references. No see-below, no cell-N, no naming another doc, and never reference the plan from inside a notebook.
- State the finding directly and stay honest about softness, a soft Sharpe is called soft.
- Easy on the jargon, explain the concept once in plain terms before naming it.
- Math in KaTeX with dollar delimiters. Inline is `$x$`, display is `$$x$$`.
- After a display block, break the terms down in plain notes form, one bullet a term, what each symbol is and what it contributes. Read it back so a reader gets the formula without parsing the notation.

## Notebook section pattern

Every section, top to bottom, follows one shape.

1. Intro. What we are about to do. What we hope it shows. How we do it. A short paragraph, before any code.
2. Code. The cells that do the thing.
3. Findings. What the numbers say. Written from the executed outputs, never a prediction.

Do NOT write cells that describe what was just done, and do NOT write "next steps" cells. The intro says what is about to happen, the findings say what did. No section closes with a bridge to the next.

## Analytical discipline

`research/signal/SIGNAL_GATE.md` is the bar every signal result clears before it is quoted as a finding. Report the gate status with the number. The rest of this section is how to get there.

- Judge by net PnL and Sharpe, not IC. IC does not translate to money here, that is a settled finding.
- Coarse first. Answer the binary question, does it move at all. Stop if flat, flat is itself the answer. Go finer only on the axis that moved, never two grids at once.
- A best cell whose neighbours and siblings do not agree with it is noise. Report the spread across the grid, not just the winner.
- Regime-normalize to separate skill from luck. A capture ratio compares across years, PnL does not.
- Persistence is the reference line on every run. Report lift over persistence, and remember persistence has no training, it is the one-print rule.
- Honest costs always. An optimistic fill is how a backtest lies.
- Run the check that would embarrass a number before believing it. Nearly every mistake in the trend lane was a good number whose killer check had not been run yet.
- Calibrate any new bar or metric on simulated books with a known edge before scoring with it. A bar that sounds strict can fail real edges and still pass luck.
- Remove an arbitrary choice rather than picking a value. Rebalance day, start phase, bar build. Stagger or average over it, a seven-slice book rebalancing one slice a day, and quote that. A result that changes sign with an arbitrary choice is noise.
- Evidence comes from independent bets, not from sampling. A 4-week signal read daily carries the same evidence as read weekly. A faster effect earns more bets a year, so a real one passes in fewer years, if it survives its larger cost. Trade on the mechanism's own clock, a calendar rebalance or an event trigger, and recalibrate the gate before scoring a new one. An event book counts its bets in independent events, since triggers cluster in volatile weeks.
- When time cannot be bought, test the idea on a market the lane never loaded. A fail there says the result was probably fitted. A pass says the idea travels, it does not pass the original signal.

## Signals, strategies and regimes

Signal research proves an edge inside a stated market condition. It does not build the regime detector. Regime detection and switching is a separate initiative with its own data path, understanding what condition a signal works in is part of signal research, detecting that condition live is not.

- A thin edge that only holds in a slim condition is still a strategy, as long as the edge inside the condition is real and the condition is known before the trade. The target is many condition-scoped signals, switched on and off by regime detection and sized by the risk gate. Nothing survives all regimes, so do not throw a signal away for failing pooled.
- Judge a conditional signal on its in-condition record, the sleeve, since that is what runs live. Do not dilute it with the flat weeks a separate regime detector would remove. The always-on book is a capital-efficiency question, not the signal's verdict.
- The search still counts. The conditional edge faces the multiple-testing correction over every condition and cut tried, and a short in-condition sample is the usual binding constraint. A promising sleeve that is under-powered is parked for more data or a mechanism, not passed and not killed.
- Explore a lane to the ends of the data, then write it up, before opening a coupled one. Independent lanes run in parallel. Coupled lanes that share a universe, a cadence and a structure run one at a time, since together they repeat the same pipeline and findings. Trend ran to the end and was written up before mean reversion, its mirror, began on top of it. Evidence that a rival effect is stronger in some condition, reversal against trend, is banked for that coupled lane, it does not close or pull forward the lane that turned it up. A lane closes on its own stop condition, never because a sibling looked better.

## Opening a lane

A lane opens with `spec.md` in its folder, committed before any return of the signal is computed. Describing the data comes first and may shape the spec. Measuring the signal may not.

- Hypothesis. What pays, in which condition, on which universe and cadence.
- Mechanism. Why it should pay, and who is on the other side.
- Primary test. One book exactly as it will be coded, and its setting grid, which is the trial family S1 deflates by.
- Kill rule. The result that closes the lane, and the result that parks it.
- Data and span, and every trial already spent on them by coupled lanes.

Everything outside the primary test is exploratory from the start. Amending the spec after a result is a new spec, its trials counted on top.

## Registering a test

A registered test is the only way past the search penalty. One trial, on data the lane never loaded or that does not exist yet.

- The spec names the hypothesis, every book exactly as coded at the registering commit, the data and that it was never loaded, the trial count per dataset, the pass, park and fail rules, and when it is reviewed.
- Commit and push the spec, then run. A run before the commit cannot be shown to have followed it.
- One look per scheduled review. Any other cut of the data is exploratory.

## Closing a lane

A lane closes on its own stop condition, when the data that exists cannot say more.

1. The write-up is the last review. Writing it raises questions the lane never asked. Answer them before closing, as exploratory, on searched data.
2. `<lane>.html` in the lane folder, charts in `img/`. In the order the work was done, each number as it stood on the day with its later correction. Then what went wrong, the verdict and its stamp, what held, the handoffs to regime, strategy and other lanes, what runs on, what was not pursued, lessons, the trial ledger, the notebook and commit map, and what is not modelled.
3. Every lesson lands here as a landmine or discipline line, and in the signal gate where it is a check.
4. Forward registered tests are listed with their review dates.

## Kinds of research

Five kinds, each its own lane.

- Signal. Is there an edge, in which condition, net of cost.
- Cost. What trading costs and how to trade. Impact shape, spread and depth as they move, execution over time, funding as a process. Its model is charged the same in research and in the C++ `CostModel`.
- Regime. Which state the market is in, known from the past alone. Switches signals on and off.
- Risk. How much to hold and how bad it can get. Volatility and correlation forecasts, tails, sizing, the kill switch's limits.
- Strategy. Composes a signal with its regime activation, its cost and its risk into a runnable book.

Volatility models feed cost, regime and risk alike. Build them once.

## Signal research and strat research

Two phases, and the seam between them gates the roadmap.

- Signal research asks is there a predictive edge, what is the mechanism, in which conditions, net of honest cost. A signal is a feature plus a directional hypothesis and an economic reason, not a feature alone. Feature engineering feeds it, the mechanism and the multiple-testing discipline are the core. A signal is not alpha by default, most of what this venue offers is alternative beta harvested conditionally.
- Strat research turns a validated signal into a runnable book. Sizing, weighting, portfolio construction, turnover and cost, execution, the risk gate, regime activation, capacity. Model selection is a sub-task inside signal research when the route is ML, not the definition of strat research.
- Hypothesis before features, never a feature zoo sifted by a model. A large feature set with no per-feature reason manufactures false positives through multiple testing. Simple has beaten complex on every shape and combiner tried here. The technical-indicator zoo is mostly monotone transforms of price you already carry, low prior. Higher moments, autocorrelation, and the cross-sectional positioning features, funding, open interest, dispersion, co-movement, liquidity, depth, carry the independent information and are where this venue's edge lives.
- A feature earns its place on four counts, all in net PnL, never IC. IC does not translate to money here and is at most a triage screen to drop dead candidates. The counts apply to a feature tested as a structural signal, one hypothesis at a time. They are not a licence to start supervised training over a feature set, that stays deferred until the structural signals are exhausted and simple has beaten complex.
  1. Pays. Net Sharpe lift over the baseline, persistence or the plain sign, out of fold. Univariate misses conditional edges, so test in-condition too.
  2. Stable. Positive across years with none carrying it, and the neighbours of the best setting agree.
  3. Adds information. Marginal net lift over the features already held, not a feature correlation threshold. Each new feature also counts as another trial in the multiple-testing correction.
  4. Mechanistic. A hypothesis for why it pays, stated before it is tested. This is a prior that lowers the false-positive penalty, not just a debugging aid.
- Correlation is a screen, not a verdict. On fat-tailed crypto a few extreme weeks hijack Pearson, so rank correlation, Spearman, is the default for predictive and redundancy screens. Use Pearson only where linear covariance is the object, risk and portfolio sizing, since rank does not plug into portfolio variance. Rank also hides a tail-driven edge, a feature that only pays in the big weeks looks weak by rank, which is another reason net PnL decides.
- ML has a narrow place, and it is not learning the directional signal. A model over momentum features is a collinear feature zoo the plain sign already captures, and combiners and crossovers lost out of fold. Its place is meta-labelling, the rule picks the side and a classifier only learns whether to take the trade, and regime detection, both lower-dimensional and better posed. Any ML still has to beat the plain rule net of cost out of fold. Higher-dimensional cross-sectional problems have more room for it than timing one market.
- Order. Signal research surfaces the conditions a signal needs, then regime detection and risk modelling are built to detect and gate those conditions live, then strat research composes the activated signals. Real strat research waits on the regime detector and the risk model, since a strat is a signal plus its activation and its gate. Trend is the least sophisticated lane and is used to harden the practice before sophisticated research.

## Landmines, already hit once

- Verify the engine honors its inputs before trusting any sweep. Change one input and confirm the output changes. A bogus cost-table path must throw, zero and two-times cost must differ. A whole cost sensitivity was run on a stale binary that ignored the passed cost table and undercharged, real fees 380 against the correct 665, because the release build defaulted to `QP_BACKTEST_MATCHER=last_trade`. Identical outputs across varied inputs is a wiring bug, investigate it, never rationalize it.
- Metrics must be invariant to size and magnitude. A capture ratio above one meant a two-unit book measured against a one-unit oracle, not over-capture. Sanity-check any ratio against its theoretical bound.
- Quantify the ceiling before committing to a build. The plan called maker the largest saving, but the optimistic ceiling was about 6 percent for the low-turnover winner, because a low-turnover book pays little in fees. Do not inherit a plan's priority without checking the number.
- Know what cannot be modelled offline and say so. Maker fills, liquidation and the short-spot borrow need tick data that was discarded or live data that does not exist yet. Those are testnet measurements, not notebook cells.
- Count evidence in independent units, not rows. A pooled regression over coin-weeks reported the market term at t 12.7. Coins in one week move together, so the rows are not independent, and with errors clustered by week it was t 1.33. Cluster by period whenever rows share one.
- Never standardise a regressor within a group where it is constant. The market trend is one number per week, so scaling it across that week's coins is zero over zero, and the result was floating-point noise that still printed a t-stat. Scale a per-period term across periods.
- A span that helped choose a result is not out of sample. The map picked its condition on the rule that the earlier and later weeks agree, then the later weeks were quoted as a frozen holdout. When the condition was chosen in fold from the past alone, the search never picked it. Choose in fold, or register the choice before the data exists.
- Run the neighbours of every winning setting before building on it. The lookback grid skipped 3 and 5 weeks, and when they were run 3 weeks earned almost nothing beside 4. A spike whose neighbours were never measured is a lucky cell until shown otherwise.
- A conditional sleeve pays for switching on and off. Slicing the always-on book's returns to the condition's weeks skips every entry and exit, so the sleeve must be the gated book's returns with its exit cost booked to its last week.
- A bar stamped at its open leaks its close one bar early. Every mock test passed the wrong clock. Before trusting a time-driven result, check the engine delivers each event when its information exists, with a test that drives the real source through the real engine.
- Single cells on one rebalance day misread the lookback grid. On Sundays 3 weeks looked like a hole and 4 weeks like possible luck. Across seven days the hole was mostly Sunday and 4 weeks was a solid block.
- Two builds of one book must agree. The market book fell from 0.80 to 0.61 rebuilt from daily data, on one week where the 4-week market return sat within 0.1% of zero. A sign taken on a near-zero input is a coin flip, so report how much of the result rests on such weeks.
- Judge a rule that sits out on the trades it takes and on return per dollar deployed. Flat time lowers a Sharpe by itself.
- Scale a cost to the market it is charged in. 7.5 bps copied to ETFs was several times heavier against funds that move a fifth as much as coins.
- A registered test ran before its spec was committed. The spec was unchanged, but the history cannot show it.
- Every exploratory book enters the trial ledger. A late check ran about 5,700 books and quoted them against the ledger's older count, which is generous to them. Rebuild them as return series and recount the effective trials.
- Hindsight conditions look robust. Nudging the windows of a condition found on the full sample barely moved it, because every nudge was fitted to the same weeks. Robustness to its own settings is not evidence the condition would have been found.
- A default cost hides in a helper. The in-fold condition search ran its books at the provisional rate while the sleeves beside it used the ladder, and quoted 1.38 where the ladder gives 1.37. A book now always carries an explicit cost model.
- A charge booked on a calendar label vanishes when that day did not trade. Slices rebalancing on ETF holidays and the final partial week paid no cost. Book a period's funding and cost on its last bar that exists.

## Working with the repo owner and git

- Never `git commit`, `git add`, or touch the index, however the request is phrased, unless commit permission is given explicitly for that turn. The owner commits, often between turns, and works out of staged changes as pre-commit review.
- In a cloud session the work exists only in the container. On request, commit it to a temporary `claude/` branch and push, so the owner can build and run it, then squash to one commit on the named branch and delete the temp branch.
- Plan docs are never committed, a hard rule. A research plan markdown is a working file only, it never enters a commit and no reference to it does either, not in a tracked file, not in a commit message.
- Push, merge and branch only when prompted. Merge with `--ff-only` so no merge commit is created, which also keeps it inside the no-commit rule.
- Do not edit research notebooks to answer a question, answer in chat unless asked to write it in.
- Give direct technical answers and correct a wrong premise. Size cancels out of the fee breakeven because fees and funding both scale with notional. Persistence is period-based and uses only the last print.
- The owner queues several tasks per message and adds more mid-turn. Exhaust what research can settle before declaring the C++ seam, that handoff point is a deliverable.
