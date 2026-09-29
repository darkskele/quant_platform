---
name: research
description: How to run quant signal and strategy research for quant-platform, the notebook and backtest loop, the voice, the analytical discipline, and the landmines. Load before building or editing a research notebook, running a backtest sweep, or reporting a research finding.
---

Research here judges a signal by downstream money on the C++ engine, not by a fit metric. The output is a notebook that reads clean and a plain finding. This is the process, the voice, and the mistakes already made once.

## The loop

Probe, then build, then execute. Three passes, in order.

1. Probe. A throwaway script in the scratchpad that wires the pipeline and prints real numbers. Validate the wiring and get the numbers before writing a word of interpretation. Never write a read from a guess.
2. Build. A python builder script that emits the `.ipynb` as JSON, one `md`/`code` helper per cell. Interpretation cells are written from the probe's real numbers.
3. Execute. `jupyter nbconvert --to notebook --execute --inplace` to embed outputs. Verify zero errors and that the tables landed.

Reuse the feature pipeline and the walk-forward splits, do not re-derive them. Pickle the out-of-fold set so a rerun skips regeneration, gbm generation is about 20s and each backtest about 30s, they add up.

## Environment

- Kernel and libs, `source ~/miniconda3/etc/profile.d/conda.sh && conda activate qp-research`. lightgbm, sklearn, pyarrow live only there.
- The engine module, glob `build/release/**/qp_python_backtest*.so` and insert its dir on `sys.path`.
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

## Signals, strategies and regimes

Signal research proves an edge inside a stated market condition. It does not build the regime detector. Regime detection and switching is a separate initiative with its own data path, understanding what condition a signal works in is part of signal research, detecting that condition live is not.

- A thin edge that only holds in a slim condition is still a strategy, as long as the edge inside the condition is real and the condition is known before the trade. The target is many condition-scoped signals, switched on and off by regime detection and sized by the risk gate. Nothing survives all regimes, so do not throw a signal away for failing pooled.
- Judge a conditional signal on its in-condition record, the sleeve, since that is what runs live. Do not dilute it with the flat weeks a separate regime detector would remove. The always-on book is a capital-efficiency question, not the signal's verdict.
- The search still counts. The conditional edge faces the multiple-testing correction over every condition and cut tried, and a short in-condition sample is the usual binding constraint. A promising sleeve that is under-powered is parked for more data or a mechanism, not passed and not killed.
- Explore a lane to the ends of the data, then write it up, before opening a coupled one. Independent lanes run in parallel. Coupled lanes that share a universe, a cadence and a structure run one at a time, since together they repeat the same pipeline and findings. Trend runs to the end and is written up before mean reversion, its mirror, begins on top of it. Evidence that a rival effect is stronger in some condition, reversal against trend, is banked for that coupled lane, it does not close or pull forward the lane that turned it up. A lane closes on its own stop condition, never because a sibling looked better.

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

## Working with the repo owner and git

- Never `git commit`, `git add`, or touch the index, however the request is phrased, unless commit permission is given explicitly for that turn. The owner commits, often between turns, and works out of staged changes as pre-commit review.
- In a cloud session the work exists only in the container. On request, commit it to a temporary `claude/` branch and push, so the owner can build and run it, then squash to one commit on the named branch and delete the temp branch.
- Plan docs are never committed, a hard rule. A research plan markdown is a working file only, it never enters a commit and no reference to it does either, not in a tracked file, not in a commit message.
- Push, merge and branch only when prompted. Merge with `--ff-only` so no merge commit is created, which also keeps it inside the no-commit rule.
- Do not edit research notebooks to answer a question, answer in chat unless asked to write it in.
- Give direct technical answers and correct a wrong premise. Size cancels out of the fee breakeven because fees and funding both scale with notional. Persistence is period-based and uses only the last print.
- The owner queues several tasks per message and adds more mid-turn. Exhaust what research can settle before declaring the C++ seam, that handoff point is a deliverable.
