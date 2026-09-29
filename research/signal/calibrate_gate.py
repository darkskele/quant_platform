"""Simulated pass rates for the signal gate's statistical checks.

Weekly returns are Student t with 4 degrees of freedom under clustered
volatility, the shape of crypto weekly returns. Each check is run on three
kinds of book. A real edge that holds all the time, a real edge carried by one
year, and no edge at all picked from a correlated grid. A check is kept only
if it passes the first, fails the second and rarely passes the third.

Run it with `python calibrate_gate.py`. Seeded, so the tables reproduce.
"""
from __future__ import annotations

import numpy as np
from scipy import stats

import gate

WEEKS = 52
YEARS = 6.5
N = 4000
rng = np.random.default_rng(7)


def returns(sr, n=N, t=int(WEEKS * YEARS), concentrated=False):
    """Weekly returns with annual Sharpe near sr, fat tails and volatility clusters."""
    h = np.zeros((n, t))
    shocks = rng.normal(0, 0.2, (n, t))
    for i in range(1, t):
        h[:, i] = 0.95 * h[:, i - 1] + shocks[:, i]
    sigma = np.exp(h)
    sigma /= np.sqrt((sigma ** 2).mean(axis=1, keepdims=True))
    eps = rng.standard_t(4, (n, t)) / np.sqrt(2)
    mu = np.full((n, t), sr / np.sqrt(WEEKS))
    if concentrated:
        # All the edge in one random full year, same total mean.
        mu[:] = 0.0
        years = int(YEARS)
        pick = rng.integers(0, years, n)
        for k in range(n):
            mu[k, pick[k] * WEEKS:(pick[k] + 1) * WEEKS] = sr / np.sqrt(WEEKS) * t / WEEKS
    return mu + sigma * eps


def grid(k, rho, sr=0.0, n=N // 4, t=int(WEEKS * YEARS)):
    """k correlated books sharing one common shock, as a lookback grid does."""
    common = returns(0.0, n, t)
    out = np.empty((n, k, t))
    for j in range(k):
        out[:, j] = np.sqrt(rho) * common + np.sqrt(1 - rho) * returns(0.0, n, t) + sr / np.sqrt(WEEKS)
    return out


def years_of(x):
    full = int(len(x) // WEEKS)
    return [x[i * WEEKS:(i + 1) * WEEKS] for i in range(full)]


def old_gate(x):
    """The previous bar, every year above 0.5 and Bonferroni over 45 trials."""
    every = all(gate.sharpe(y) > 0.5 for y in years_of(x))
    t = x.mean() / x.std() * np.sqrt(len(x))
    return every, t > stats.norm.ppf(1 - 0.05 / 45)


def main():
    print("Stationary and one-year edges, share of books passing each check")
    print(f"{'true SR':>8} {'kind':>13} {'old every yr':>13} {'old Bonf 45':>12} {'drop one yr':>12} {'sig, 1 test':>12} {'sig, Neff 3':>12}")
    for sr in [0.0, 0.5, 0.8, 1.0, 1.5]:
        for conc in [False, True]:
            if sr == 0.0 and conc:
                continue
            x = returns(sr, concentrated=conc)
            old = np.array([old_gate(r) for r in x])
            drop = np.mean([gate.year_stability(r, WEEKS)[0] for r in x])
            s1 = np.mean([gate.significance(r, 1.0)[0] >= gate.CONFIDENCE for r in x])
            s3 = np.mean([gate.significance(r, 3.0)[0] >= gate.CONFIDENCE for r in x])
            kind = 'one year' if conc else 'stationary'
            print(f"{sr:>8.1f} {kind:>13} {old[:, 0].mean():>13.2f} {old[:, 1].mean():>12.2f} {drop:>12.2f} {s1:>12.2f} {s3:>12.2f}")

    print()
    print("No edge anywhere, best cell of a correlated grid, share wrongly passing")
    print(f"{'cells':>6} {'corr':>6} {'Neff':>6} {'naive, 1 test':>14} {'Neff deflated':>14}")
    for k, rho in [(5, 0.85), (20, 0.85), (20, 0.5), (95, 0.6)]:
        g = grid(k, rho)
        best = g.mean(axis=2).argmax(axis=1)
        picked = g[np.arange(len(g)), best]
        neff = np.median([gate.effective_trials(b) for b in g[:50]])
        naive = np.mean([gate.significance(p, 1.0)[0] >= gate.CONFIDENCE for p in picked])
        defl = np.mean([gate.significance(p, gate.effective_trials(b))[0] >= gate.CONFIDENCE for p, b in zip(picked, g)])
        print(f"{k:>6} {rho:>6.2f} {neff:>6.1f} {naive:>14.2f} {defl:>14.2f}")

    print()
    print("Neighbour agreement on a flat plateau, 3 cells correlated 0.85, share passing")
    for sr in [0.5, 0.8]:
        g = grid(3, 0.85, sr=sr)
        ok = np.mean([gate.neighbours_agree(b[1], [b[0], b[2]], WEEKS)[0] for b in g])
        print(f"  true SR {sr:.1f} on all three   {ok:.2f}")
    g = grid(3, 0.85, sr=0.0)
    g[:, 1] += 0.8 / np.sqrt(WEEKS)
    print(f"  spike, 0.8 middle and 0 either side   {np.mean([gate.neighbours_agree(b[1], [b[0], b[2]], WEEKS)[0] for b in g]):.2f}")

    print()
    print("Weeks needed for the significance check at 80% power, one test")
    for sr in [0.5, 0.8, 1.0, 1.5]:
        print(f"  true SR {sr:.1f}   {gate.weeks_needed(sr, 1.0, WEEKS)} weeks")


if __name__ == '__main__':
    main()
