"""The signal gate's statistical checks and its stamp.

Each check takes net period returns and answers pass or fail with the number
behind it. Thresholds come from simulated books with known edges.
"""
from __future__ import annotations

import functools
import itertools
from dataclasses import dataclass, field

import numpy as np
import pandas as pd
from scipy import integrate, stats as sps

from . import stats

CONFIDENCE = 0.95   # significance a signal needs to pass
PARK = 0.5          # below this it is noise, between this and CONFIDENCE it is under-powered
DROP_KEEP = 0.4     # share of the full Sharpe that must survive removing any one year
Z = sps.norm.ppf(CONFIDENCE)


@functools.lru_cache(maxsize=None)
def expected_max(n: float) -> float:
    """Expected maximum of n independent standard normals, for real n from 1."""
    if n <= 1:
        return 0.0
    f = lambda x: x * n * sps.norm.pdf(x) * sps.norm.cdf(x) ** (n - 1)
    return integrate.quad(f, -10, 10)[0]


@functools.lru_cache(maxsize=1)
def _emax_table() -> tuple[np.ndarray, np.ndarray]:
    """Expected maximum against test count, for inverting it without a search."""
    ns = np.geomspace(1, 5000, 400)
    return ns, np.array([expected_max(float(n)) for n in ns])


def _values(x) -> np.ndarray:
    v = np.asarray(x, dtype=float)
    return v[~np.isnan(v)]


def effective_trials(books, extra=0) -> float:
    """How many independent tests a set of correlated trial return series is worth.

    Simulates the best of many draws sharing the books' correlation, then finds
    the count of independent tests with the same expected best. extra adds
    trials with no return series, independent of everything.
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
    ns, emax = _emax_table()
    return float(np.interp(draws.max(axis=1).mean(), emax, ns))


def significance(x, trials: float, periods=None) -> tuple[float, float]:
    """Probability the Sharpe is real after picking the best of trials tests, and the annual luck bar.

    Probabilistic Sharpe against the Sharpe the best of that many no-edge tests
    reaches by chance, corrected for skew and fat tails.
    """
    v = _values(x)
    t = len(v)
    sr = v.mean() / v.std()
    bar = expected_max(trials) / np.sqrt(t - 1)
    sk, ku = sps.skew(v), sps.kurtosis(v, fisher=False)
    denom = np.sqrt(max(1 - sk * sr + (ku - 1) / 4 * sr ** 2, 1e-12))
    p = periods if periods is not None else stats.periods_per_year(x)
    return float(sps.norm.cdf((sr - bar) * np.sqrt(t - 1) / denom)), float(bar * np.sqrt(p))


def periods_needed(sr_annual: float, trials: float, periods: int, power=0.8) -> int:
    """Sample length at which a true Sharpe of sr_annual passes significance with the given power. -1 for no edge."""
    sr = sr_annual / np.sqrt(periods)
    if sr <= 0:
        return -1
    return int(np.ceil(((Z + sps.norm.ppf(power) + expected_max(trials)) / sr) ** 2)) + 1


def _years(x, periods) -> list[np.ndarray]:
    """Calendar years of a dated series, else consecutive blocks of periods."""
    if isinstance(x, pd.Series) and isinstance(x.index, pd.DatetimeIndex):
        return [g.values for _, g in x.groupby(x.index.year)]
    v = np.asarray(x, dtype=float)
    return [v[i * periods:(i + 1) * periods] for i in range(len(v) // periods)]


def year_stability(x, periods=None) -> tuple[bool, float]:
    """No single year carries the result, and the worst share of the full Sharpe kept.

    Drop each year in turn. The rest must stay positive and keep DROP_KEEP of
    the full Sharpe.
    """
    p = periods if periods is not None else stats.periods_per_year(x)
    full = stats.sharpe(x, p)
    if not full > 0:
        return False, np.nan
    vals = np.asarray(x, dtype=float)
    worst, start = np.inf, 0
    for v in _years(x, p):
        rest = np.concatenate([vals[:start], vals[start + len(v):]])
        start += len(v)
        worst = min(worst, stats.sharpe(rest, p) / full)
    return bool(worst >= DROP_KEEP), float(worst)


def oos_agree(in_sample, out_sample) -> tuple[bool, float]:
    """Out of sample is positive and not below in sample by more than chance allows, and the drop's z score."""
    a, b = _values(in_sample), _values(out_sample)
    sa, sb = a.mean() / a.std(), b.mean() / b.std()
    z = (sa - sb) / np.sqrt(1 / (len(a) - 1) + 1 / (len(b) - 1))
    return bool(sb > 0 and z <= Z), float(z)


def neighbours_agree(best, neighbours) -> tuple[bool, list[float]]:
    """Adjacent settings earn and trail the chosen one by no more than chance, and each gap's z score.

    The allowed gap shrinks as the two books correlate.
    """
    b = pd.Series(np.asarray(best, float))
    zs, ok = [], True
    for n in neighbours:
        j = pd.concat([b, pd.Series(np.asarray(n, float))], axis=1).dropna()
        rho = j.iloc[:, 0].corr(j.iloc[:, 1])
        s0 = j.iloc[:, 0].mean() / j.iloc[:, 0].std()
        s1 = j.iloc[:, 1].mean() / j.iloc[:, 1].std()
        z = (s0 - s1) / np.sqrt(max(2 - 2 * rho, 1e-12) / (len(j) - 1))
        zs.append(float(z))
        ok &= bool(s1 > 0 and z <= Z)
    return ok, zs


def pbo(books: pd.DataFrame, splits=16) -> float:
    """Probability the best setting in sample lands at or below the median out of sample.

    Combinatorially symmetric cross validation over a periods by settings
    table. Near 0 the choice carries information, 0.5 or above it is luck.
    """
    m = books.dropna().values
    blocks = np.array_split(np.arange(len(m)), splits)
    below = []
    for combo in itertools.combinations(range(splits), splits // 2):
        ins = np.concatenate([blocks[i] for i in combo])
        outs = np.concatenate([blocks[i] for i in range(splits) if i not in combo])
        sr_in = m[ins].mean(0) / m[ins].std(0)
        sr_out = m[outs].mean(0) / m[outs].std(0)
        rank = sps.rankdata(sr_out)[sr_in.argmax()] / (m.shape[1] + 1)
        below.append(rank <= 0.5)
    return float(np.mean(below))


def walk_forward(books: pd.DataFrame, folds=6, min_train=104, pick=None) -> tuple[pd.Series, list]:
    """Expanding walk forward that picks a setting on each fold's past only.

    books is periods by settings. Each fold trades the setting with the best
    Sharpe before it unless pick says otherwise. Returns the stitched out of
    sample returns and the setting picked per fold.
    """
    b = books.dropna(how='all')
    pick = pick or (lambda past: past.apply(lambda c: stats.sharpe(c, 1)).idxmax())
    edges = np.linspace(min_train, len(b), folds + 1).astype(int)
    out, picked = [], []
    for lo, hi in zip(edges[:-1], edges[1:]):
        choice = pick(b.iloc[:lo])
        picked.append(choice)
        out.append(b[choice].iloc[lo:hi])
    return pd.concat(out), picked


def bootstrap(x, block=8, n=5000, seed=0, periods=None) -> dict:
    """Block bootstrap of a return path, keeping short-run dependence.

    Sharpe percentiles, the share of paths ending above zero and drawdown
    percentiles.
    """
    p = periods if periods is not None else stats.periods_per_year(x)
    rng = np.random.default_rng(seed)
    v = _values(x)
    nb = int(np.ceil(len(v) / block))
    starts = rng.integers(0, len(v) - block + 1, size=(n, nb))
    paths = v[(starts[:, :, None] + np.arange(block)).reshape(n, -1)[:, :len(v)]]
    sr = paths.mean(1) / paths.std(1) * np.sqrt(p)
    cum = paths.cumsum(1)
    dd = (cum - np.maximum.accumulate(cum, axis=1)).min(1)
    return {'sharpe 5%': np.percentile(sr, 5), 'sharpe 50%': np.percentile(sr, 50), 'sharpe 95%': np.percentile(sr, 95),
            'ends above zero': float((cum[:, -1] > 0).mean()),
            'drawdown 50%': np.percentile(dd, 50), 'drawdown 5%': np.percentile(dd, 5)}


def stamp(failed: list[str], prob: float, sr_annual: float, trials: float, periods: int,
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
    need = 'a forward run' if prob >= CONFIDENCE else f'about {periods_needed(sr_annual, trials, periods)} periods'
    return f'gate park, needs {need}' + tail


@dataclass
class Verdict:
    """Every statistical check run on one book, the number behind each, and the stamp."""
    stamp: str
    sharpe: float
    probability: float
    luck_bar: float
    failed: list[str]
    open: list[str]
    numbers: dict = field(default_factory=dict)


def score(net: pd.Series, trials: float, periods=None, exploratory=False, oos=None, neighbours=None,
          grid=None, open_items=()) -> Verdict:
    """Runs S1, S2 and V1 always, S3 to S5 when their inputs are given, and stamps the result.

    oos is an (in sample, out of sample) pair, neighbours the books one step
    either side, grid the periods by settings table the choice came from. A
    statistical check without its input is named open.
    """
    p = periods if periods is not None else stats.periods_per_year(net)
    prob, bar = significance(net, trials, p)
    stable, kept = year_stability(net, p)
    boot = bootstrap(net, periods=p)
    numbers = {'S1': prob, 'S2': kept, 'V1': boot['sharpe 5%']}
    failed = [k for k, ok in (('S2', stable), ('V1', boot['sharpe 5%'] > 0)) if not ok]
    unrun = []
    if oos is None:
        unrun.append('S3')
    else:
        ok, numbers['S3'] = oos_agree(*oos)
        failed += [] if ok else ['S3']
    if neighbours is None:
        unrun.append('S4')
    else:
        ok, numbers['S4'] = neighbours_agree(net, neighbours)
        failed += [] if ok else ['S4']
    if grid is None:
        unrun.append('S5')
    else:
        numbers['S5'] = pbo(grid)
        failed += [] if numbers['S5'] < 0.5 else ['S5']
    opened = [*unrun, *open_items]
    sr = stats.sharpe(net, p)
    return Verdict(stamp(failed, prob, sr, trials, p, opened, exploratory), sr, prob, bar, failed, opened, numbers)
