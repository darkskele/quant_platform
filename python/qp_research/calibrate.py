"""Simulated pass rates for the signal gate's statistical checks.

Returns are Student t with 4 degrees of freedom under clustered volatility,
the shape of crypto returns. Each check runs on a real edge held all the time,
a real edge carried by one year, and no edge picked from a correlated grid. A
check is kept only if it passes the first, fails the second and rarely passes
the third. Pass rates depend on the sample, so every table takes the span and
cadence of the book being judged.

Run with `python -m qp_research.calibrate --span 338 --periods 52`. Seeded, so
the tables reproduce.
"""
from __future__ import annotations

import argparse

import numpy as np
import pandas as pd
from scipy import stats as sps

from . import gate, stats

def returns(rng, sr, n, span, periods, concentrated=False) -> np.ndarray:
    """n paths of span returns with annual Sharpe near sr, fat tails and volatility clusters.

    Volatility persistence and spread are set per year, so clusters last as
    long in time at any cadence.
    """
    t = span
    phi = 0.95 ** (52 / periods)
    h = np.zeros((n, t))
    shocks = rng.normal(0, 0.2 * np.sqrt((1 - phi ** 2) / (1 - 0.95 ** 2)), (n, t))
    for i in range(1, t):
        h[:, i] = phi * h[:, i - 1] + shocks[:, i]
    sigma = np.exp(h)
    sigma /= np.sqrt((sigma ** 2).mean(axis=1, keepdims=True))
    eps = rng.standard_t(4, (n, t)) / np.sqrt(2)
    mu = np.full((n, t), sr / np.sqrt(periods))
    if concentrated:
        # All the edge in one random full year, same total mean.
        mu[:] = 0.0
        pick = rng.integers(0, t // periods, n)
        for k in range(n):
            mu[k, pick[k] * periods:(pick[k] + 1) * periods] = sr / np.sqrt(periods) * t / periods
    return mu + sigma * eps


def grid(rng, k, rho, n, span, periods, sr=0.0) -> np.ndarray:
    """n grids of k books sharing one common shock with correlation rho, as a lookback grid does."""
    common = returns(rng, 0.0, n, span, periods)
    out = np.empty((n, k, span))
    for j in range(k):
        out[:, j] = np.sqrt(rho) * common + np.sqrt(1 - rho) * returns(rng, 0.0, n, span, periods) + sr / np.sqrt(periods)
    return out


def old_bar(x, periods) -> tuple[bool, bool]:
    """The previous bar, every year above 0.5 and Bonferroni over 45 trials."""
    years = [x[i * periods:(i + 1) * periods] for i in range(len(x) // periods)]
    t = x.mean() / x.std() * np.sqrt(len(x))
    return all(stats.sharpe(y, periods) > 0.5 for y in years), t > sps.norm.ppf(1 - 0.05 / 45)


def edges(span, periods, n=4000, seed=7) -> pd.DataFrame:
    """Share of stationary and one-year edges passing S2, S1 at one and three effective trials, and the old bar."""
    rng = np.random.default_rng(seed)
    rows = []
    for sr in (0.0, 0.5, 0.8, 1.0, 1.5):
        for conc in (False, True):
            if sr == 0.0 and conc:
                continue
            x = returns(rng, sr, n, span, periods, concentrated=conc)
            old = np.array([old_bar(r, periods) for r in x])
            rows.append({'true sharpe': sr, 'edge': 'one year' if conc else ('none' if sr == 0 else 'stationary'),
                         'S2': np.mean([gate.year_stability(r, periods)[0] for r in x]),
                         'S1 one test': np.mean([gate.significance(r, 1.0, periods)[0] >= gate.CONFIDENCE for r in x]),
                         'S1 three trials': np.mean([gate.significance(r, 3.0, periods)[0] >= gate.CONFIDENCE for r in x]),
                         'old every year': old[:, 0].mean(), 'old Bonferroni 45': old[:, 1].mean()})
    return pd.DataFrame(rows)


def grids(span, periods, n=1000, seed=7) -> pd.DataFrame:
    """No edge anywhere, the best cell of a correlated grid. Share wrongly passing S1 counted as one test and deflated."""
    rng = np.random.default_rng(seed)
    rows = []
    for k, rho in ((5, 0.85), (20, 0.85), (20, 0.5), (95, 0.6)):
        g = grid(rng, k, rho, n, span, periods)
        picked = g[np.arange(n), g.mean(axis=2).argmax(axis=1)]
        neff = [gate.effective_trials(b) for b in g]
        rows.append({'cells': k, 'correlation': rho, 'effective trials': float(np.median(neff)),
                     'one test': np.mean([gate.significance(p, 1.0, periods)[0] >= gate.CONFIDENCE for p in picked]),
                     'deflated': np.mean([gate.significance(p, m, periods)[0] >= gate.CONFIDENCE for p, m in zip(picked, neff)])})
    return pd.DataFrame(rows)


def plateau(span, periods, n=1000, seed=7) -> pd.Series:
    """Share of S4 passes on three settings correlated 0.85, two flat plateaus and one spike."""
    rng = np.random.default_rng(seed)
    out = {}
    for sr in (0.5, 0.8):
        g = grid(rng, 3, 0.85, n, span, periods, sr=sr)
        out[f'plateau {sr}'] = np.mean([gate.neighbours_agree(b[1], [b[0], b[2]])[0] for b in g])
    g = grid(rng, 3, 0.85, n, span, periods)
    g[:, 1] += 0.8 / np.sqrt(periods)
    out['spike 0.8'] = np.mean([gate.neighbours_agree(b[1], [b[0], b[2]])[0] for b in g])
    return pd.Series(out)


def power(periods) -> pd.Series:
    """Periods a single primary test needs to pass S1 with 80% power."""
    return pd.Series({sr: gate.periods_needed(sr, 1.0, periods) for sr in (0.5, 0.8, 1.0, 1.5)})


if __name__ == '__main__':
    args = argparse.ArgumentParser()
    args.add_argument('--span', type=int, required=True, help='periods in the sample being judged')
    args.add_argument('--periods', type=int, required=True, help='periods a year')
    a = args.parse_args()
    for table in (edges(a.span, a.periods), grids(a.span, a.periods), plateau(a.span, a.periods), power(a.periods)):
        print(table.round(2).to_string(), end='\n\n')
