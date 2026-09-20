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

- Each section stands alone, what this is, what you are doing, the code, the results, the interpretation, the next thing.
- No cross-references. No see-below, no cell-N, no naming another doc, and never reference the plan from inside a notebook.
- State the finding directly and stay honest about softness, a soft Sharpe is called soft.
- Match the voice of the sibling notebooks before writing a new one.

## Analytical discipline

`research/GATE.md` is the bar every result clears before it is quoted as a finding, and every item of it before anything goes live. Report the gate status with the number. The rest of this section is how to get there.

- Judge by net PnL and Sharpe, not IC. IC does not translate to money here, that is a settled finding.
- Coarse first. Answer the binary question, does it move at all. Stop if flat, flat is itself the answer. Go finer only on the axis that moved, never two grids at once.
- A best cell whose neighbours and siblings do not agree with it is noise. Report the spread across the grid, not just the winner.
- Regime-normalize to separate skill from luck. A capture ratio compares across years, PnL does not.
- Persistence is the reference line on every run. Report lift over persistence, and remember persistence has no training, it is the one-print rule.
- Honest costs always. An optimistic fill is how a backtest lies.

## Landmines, already hit once

- Verify the engine honors its inputs before trusting any sweep. Change one input and confirm the output changes. A bogus cost-table path must throw, zero and two-times cost must differ. A whole cost sensitivity was run on a stale binary that ignored the passed cost table and undercharged, real fees 380 against the correct 665, because the release build defaulted to `QP_BACKTEST_MATCHER=last_trade`. Identical outputs across varied inputs is a wiring bug, investigate it, never rationalize it.
- Metrics must be invariant to size and magnitude. A capture ratio above one meant a two-unit book measured against a one-unit oracle, not over-capture. Sanity-check any ratio against its theoretical bound.
- Quantify the ceiling before committing to a build. The plan called maker the largest saving, but the optimistic ceiling was about 6 percent for the low-turnover winner, because a low-turnover book pays little in fees. Do not inherit a plan's priority without checking the number.
- Know what cannot be modelled offline and say so. Maker fills, liquidation and the short-spot borrow need tick data that was discarded or live data that does not exist yet. Those are testnet measurements, not notebook cells.

## Working with the repo owner and git

- Never `git commit`, `git add`, or touch the index, however the request is phrased, unless commit permission is given explicitly for that turn. The owner commits, often between turns, and works out of staged changes as pre-commit review.
- Plan docs are never committed, a hard rule. A research plan markdown is a working file only, it never enters a commit and no reference to it does either, not in a tracked file, not in a commit message.
- Push, merge and branch only when prompted. Merge with `--ff-only` so no merge commit is created, which also keeps it inside the no-commit rule.
- Do not edit research notebooks to answer a question, answer in chat unless asked to write it in.
- Give direct technical answers and correct a wrong premise. Size cancels out of the fee breakeven because fees and funding both scale with notional. Persistence is period-based and uses only the last print.
- The owner queues several tasks per message and adds more mid-turn. Exhaust what research can settle before declaring the C++ seam, that handoff point is a deliverable.
