"""The signal gate's statistical checks, shared by every signal lane.

Each check takes net period returns and answers pass or fail with the number
behind it. Thresholds come from simulated books with known edges.
"""
from __future__ import annotations

import functools
import itertools

import numpy as np
import pandas as pd
from scipy import integrate, stats

CONFIDENCE = 0.95   # significance a signal needs to pass
PARK = 0.5          # below this it is noise, between this and CONFIDENCE it is under-powered
DROP_KEEP = 0.4     # share of the full Sharpe that must survive removing any one year
Z = stats.norm.ppf(CONFIDENCE)


def sharpe(x, periods=52) -> float:
    x = np.asarray(x, dtype=float)
    x = x[~np.isnan(x)]
    return x.mean() / x.std() * np.sqrt(periods) if len(x) > 2 and x.std() > 0 else np.nan


@functools.lru_cache(maxsize=None)
def expected_max(n: float) -> float:
    """Expected maximum of n independent standard normals, for real n from 1."""
    if n <= 1:
        return 0.0
    f = lambda x: x * n * stats.norm.pdf(x) * stats.norm.cdf(x) ** (n - 1)
    return integrate.quad(f, -10, 10)[0]


# Expected maximum against test count, for inverting it without a search.
_NS = np.geomspace(1, 5000, 400)
_EMAX = np.array([expected_max(float(n)) for n in _NS])


def effective_trials(books, extra=0) -> float:
    """How many independent tests a set of correlated trial return series is worth.

    Simulates the best of many draws sharing the books' correlation, then finds
    the count of independent tests with the same expected best. Near copies
    count as one, unrelated books count in full. extra adds trials with no
    return series, counted as independent of everything.
    """
    m = books.values.T if isinstance(books, pd.DataFrame) else np.asarray(books)
    c = np.nan_to_num(pd.DataFrame(m.T).corr().values)
    np.fill_diagonal(c, 1.0)
    if extra:
        k = len(c)
        c = np.block([[c, np.zeros((k, extra))], [np.zeros((extra, k)), np.eye(extra)]])
    w, v = np.linalg.eigh(c)
    root = v * np.sqrt(np.clip(w, 0, None))
    draws = np.random.default_rng(0).standard_normal((20000, len(c))) @ root.T
    target = draws.max(axis=1).mean()
    return float(np.interp(target, _EMAX, _NS))


def significance(x, trials: float, periods=52) -> tuple[float, float]:
    """Probability the Sharpe is real after picking the best of trials tests.

    Probabilistic Sharpe against the luck bar, the Sharpe the best of that many
    no-edge tests reaches by chance. Corrected for skew and fat tails. Returns
    the probability and the annual luck bar.
    """
    x = np.asarray(x, dtype=float)
    x = x[~np.isnan(x)]
    t = len(x)
    sr = x.mean() / x.std()
    bar = expected_max(trials) / np.sqrt(t - 1)
    sk, ku = stats.skew(x), stats.kurtosis(x, fisher=False)
    denom = np.sqrt(max(1 - sk * sr + (ku - 1) / 4 * sr ** 2, 1e-12))
    return float(stats.norm.cdf((sr - bar) * np.sqrt(t - 1) / denom)), float(bar * np.sqrt(periods))


def weeks_needed(sr_annual: float, trials: float, periods=52, power=0.8) -> int:
    """Sample length at which a true Sharpe of sr_annual passes significance with the given power."""
    sr = sr_annual / np.sqrt(periods)
    if sr <= 0:
        return -1
    return int(np.ceil(((Z + stats.norm.ppf(power) + expected_max(trials)) / sr) ** 2)) + 1


def _years(x, periods):
    if isinstance(x, pd.Series) and isinstance(x.index, pd.DatetimeIndex):
        return {y: g.values for y, g in x.groupby(x.index.year)}
    x = np.asarray(x, dtype=float)
    return {i: x[i * periods:(i + 1) * periods] for i in range(len(x) // periods)}


def year_stability(x, periods=52) -> tuple[bool, float]:
    """No single calendar year carries the result.

    Drop each year in turn. The Sharpe of the rest must stay positive and keep
    DROP_KEEP of the full Sharpe. Returns the verdict and the worst ratio kept.
    """
    full = sharpe(x, periods)
    if not full > 0:
        return False, np.nan
    vals = np.asarray(x, dtype=float)
    worst, start = np.inf, 0
    for v in _years(x, periods).values():
        rest = np.concatenate([vals[:start], vals[start + len(v):]])
        start += len(v)
        worst = min(worst, sharpe(rest, periods) / full)
    return bool(worst >= DROP_KEEP), float(worst)


def oos_agree(in_sample, out_sample, periods=52) -> tuple[bool, float]:
    """Out of sample is positive and within sampling error of in sample.

    Fails only when out of sample is lower by more than chance allows. Returns
    the verdict and the z score of the drop.
    """
    a, b = np.asarray(in_sample, float), np.asarray(out_sample, float)
    a, b = a[~np.isnan(a)], b[~np.isnan(b)]
    sa, sb = a.mean() / a.std(), b.mean() / b.std()
    z = (sa - sb) / np.sqrt(1 / (len(a) - 1) + 1 / (len(b) - 1))
    return bool(sb > 0 and z <= Z), float(z)


def neighbours_agree(best, neighbours, periods=52) -> tuple[bool, list[float]]:
    """Adjacent settings earn, and trail the chosen one by no more than chance.

    The allowed gap shrinks as the two books correlate, since near copies should
    earn near the same. Returns the verdict and each neighbour's gap z score.
    """
    b = pd.Series(np.asarray(best, float))
    zs, ok = [], True
    for n in neighbours:
        n = pd.Series(np.asarray(n, float))
        j = pd.concat([b, n], axis=1).dropna()
        rho = j.iloc[:, 0].corr(j.iloc[:, 1])
        s0 = j.iloc[:, 0].mean() / j.iloc[:, 0].std()
        s1 = j.iloc[:, 1].mean() / j.iloc[:, 1].std()
        se = np.sqrt(max(2 - 2 * rho, 1e-12) / (len(j) - 1))
        z = (s0 - s1) / se
        zs.append(float(z))
        ok &= bool(s1 > 0 and z <= Z)
    return ok, zs


def pbo(books: pd.DataFrame, splits=16) -> float:
    """Probability the best setting in sample lands below the median out of sample.

    Combinatorially symmetric cross validation over a periods by settings table.
    Near 0 means the choice carries information, 0.5 or above means it is luck.
    """
    m = books.dropna().values
    blocks = np.array_split(np.arange(len(m)), splits)
    below = []
    for combo in itertools.combinations(range(splits), splits // 2):
        ins = np.concatenate([blocks[i] for i in combo])
        outs = np.concatenate([blocks[i] for i in range(splits) if i not in combo])
        sr_in = m[ins].mean(0) / m[ins].std(0)
        sr_out = m[outs].mean(0) / m[outs].std(0)
        rank = stats.rankdata(sr_out)[sr_in.argmax()] / (m.shape[1] + 1)
        below.append(rank <= 0.5)
    return float(np.mean(below))


def walk_forward(books: pd.DataFrame, folds=6, min_train=104, pick=None) -> tuple[pd.Series, list]:
    """Expanding walk forward that picks a setting on each fold's past only.

    books is periods by settings. Each test fold trades the setting with the best
    Sharpe on everything before it. Returns the stitched out of sample returns
    and the setting picked in each fold.
    """
    b = books.dropna(how='all')
    pick = pick or (lambda past: past.apply(sharpe).idxmax())
    edges = np.linspace(min_train, len(b), folds + 1).astype(int)
    out, picked = [], []
    for lo, hi in zip(edges[:-1], edges[1:]):
        choice = pick(b.iloc[:lo])
        picked.append(choice)
        out.append(b[choice].iloc[lo:hi])
    return pd.concat(out), picked


def bootstrap(x, block=8, n=5000, seed=0) -> dict:
    """Block bootstrap of a return path, keeping short-run dependence intact.

    Returns the Sharpe percentiles, the share of paths ending above zero and the
    drawdown percentiles.
    """
    rng = np.random.default_rng(seed)
    v = np.asarray(x, float)
    v = v[~np.isnan(v)]
    nb = int(np.ceil(len(v) / block))
    starts = rng.integers(0, len(v) - block + 1, size=(n, nb))
    paths = np.stack([np.concatenate([v[a:a + block] for a in s])[:len(v)] for s in starts])
    sr = paths.mean(1) / paths.std(1) * np.sqrt(52)
    cum = paths.cumsum(1)
    dd = (cum - np.maximum.accumulate(cum, axis=1)).min(1)
    return {'sharpe 5%': np.percentile(sr, 5), 'sharpe 50%': np.percentile(sr, 50), 'sharpe 95%': np.percentile(sr, 95),
            'ends above zero': float((cum[:, -1] > 0).mean()),
            'drawdown 50%': np.percentile(dd, 50), 'drawdown 5%': np.percentile(dd, 5)}


def cluster_ols(y: np.ndarray, X: np.ndarray, groups: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """OLS with standard errors clustered by group, so rows sharing a week are not counted as independent."""
    beta, *_ = np.linalg.lstsq(X, y, rcond=None)
    e = y - X @ beta
    bread = np.linalg.inv(X.T @ X)
    meat = np.zeros((X.shape[1], X.shape[1]))
    for g in np.unique(groups):
        s = X[groups == g].T @ e[groups == g]
        meat += np.outer(s, s)
    k, n, c = X.shape[1], len(y), len(np.unique(groups))
    scale = c / (c - 1) * (n - 1) / (n - k)
    return beta, np.sqrt(np.diag(scale * bread @ meat @ bread))


def stamp(failed: list[str], prob: float, sr_annual: float, trials: float, periods=52,
          open_items=(), exploratory=False) -> str:
    """The gate status carried by every quoted result.

    An exploratory result is capped at park. Checks the data cannot settle are
    named as open.
    """
    tail = f', open {" ".join(open_items)}' if open_items else ''
    failed = (['S1'] if prob < PARK else []) + [f for f in failed if f != 'S1']
    if failed:
        return 'gate fail ' + ' '.join(failed) + tail
    if prob >= CONFIDENCE and not exploratory:
        return 'gate pass' + tail
    if prob >= PARK:
        need = 'a forward run' if prob >= CONFIDENCE else f'about {weeks_needed(sr_annual, trials, periods)} periods'
        return f'gate park, needs {need}' + tail
