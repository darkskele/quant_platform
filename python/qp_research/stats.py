"""Return and signal statistics on period returns.

Annualisation is read from a DatetimeIndex's spacing when periods is not
given. Weekly is 52 a year, daily 365, hourly 8760. Any other spacing, or a
bare array, needs periods passed.
"""
from __future__ import annotations

import numpy as np
import pandas as pd

PERIODS = {pd.Timedelta(weeks=1): 52, pd.Timedelta(days=1): 365, pd.Timedelta(hours=1): 8760}


def periods_per_year(x) -> int:
    """Periods a year from the median spacing of x's DatetimeIndex."""
    idx = x.index if isinstance(x, (pd.Series, pd.DataFrame)) else x
    if not isinstance(idx, pd.DatetimeIndex) or len(idx) < 2:
        raise ValueError("annualising needs a DatetimeIndex of two or more stamps, or periods passed")
    step = pd.Series(idx).diff().median()
    if step not in PERIODS:
        raise ValueError(f"no annualisation for a spacing of {step}, pass periods")
    return PERIODS[step]


def _periods(x, periods) -> int:
    return periods if periods is not None else periods_per_year(x)


def _values(x) -> np.ndarray:
    v = np.asarray(x, dtype=float)
    return v[~np.isnan(v)]


def sharpe(x, periods=None) -> float:
    """Annualised mean over standard deviation, NaN dropped. NaN on fewer than three values or no spread."""
    v = _values(x)
    if len(v) <= 2 or np.ptp(v) == 0:
        return np.nan
    return float(v.mean() / v.std() * np.sqrt(_periods(x, periods)))


def annual_return(x, periods=None) -> float:
    """Mean period return times periods a year."""
    return float(_values(x).mean() * _periods(x, periods))


def volatility(x, periods=None) -> float:
    """Annualised standard deviation."""
    return float(_values(x).std() * np.sqrt(_periods(x, periods)))


def drawdown(x) -> float:
    """Deepest fall of summed returns from their running peak, zero or below."""
    c = np.cumsum(_values(x))
    if not len(c):
        return np.nan
    return float((c - np.maximum.accumulate(np.maximum(c, 0.0))).min())


def t_stat(x) -> float:
    """Mean over its standard error, NaN dropped."""
    v = _values(x)
    if len(v) <= 2 or np.ptp(v) == 0:
        return np.nan
    return float(v.mean() / v.std(ddof=1) * np.sqrt(len(v)))


def by_year(x, fn=sharpe, periods=None):
    """fn per calendar year. A series gives a series by year, a frame gives books by years."""
    if isinstance(x, pd.DataFrame):
        return pd.DataFrame({c: by_year(x[c], fn, periods) for c in x.columns}).T
    if fn in (sharpe, annual_return, volatility):
        p = _periods(x, periods)
        return x.groupby(x.index.year).apply(lambda g: fn(g, p))
    return x.groupby(x.index.year).apply(fn)


def summary(x, periods=None):
    """Sharpe, annual return, volatility, drawdown, periods held and the share of years with a positive Sharpe."""
    if isinstance(x, pd.DataFrame):
        return pd.DataFrame({c: summary(x[c], periods) for c in x.columns}).T
    p = _periods(x, periods)
    years = by_year(x.dropna(), sharpe, p)
    return pd.Series({'sharpe': sharpe(x, p), 'annual return': annual_return(x, p), 'volatility': volatility(x, p),
                      'drawdown': drawdown(x), 'periods': int(x.notna().sum()), 'years positive': float((years > 0).mean())})


def ic(signal: pd.DataFrame, forward: pd.DataFrame, method='spearman', min_names=20) -> pd.Series:
    """Cross-sectional correlation of signal with forward return per period.

    Periods with fewer than min_names names carrying both are dropped.
    """
    both = signal.notna() & forward.notna()
    s, f = signal.where(both), forward.where(both)
    out = s.corrwith(f, axis=1, method=method)
    return out[both.sum(axis=1) >= min_names].dropna()


def cluster_ols(y: np.ndarray, X: np.ndarray, groups: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """OLS coefficients and standard errors clustered by group, so rows sharing a period are not counted as independent."""
    beta, *_ = np.linalg.lstsq(X, y, rcond=None)
    e = y - X @ beta
    bread = np.linalg.inv(X.T @ X)
    meat = np.zeros((X.shape[1], X.shape[1]))
    for g in np.unique(groups):
        s = X[groups == g].T @ e[groups == g]
        meat += np.outer(s, s)
    k, n, c = X.shape[1], len(y), len(np.unique(groups))
    # Small-sample correction for few clusters and fitted parameters.
    scale = c / (c - 1) * (n - 1) / (n - k)
    return beta, np.sqrt(np.diag(scale * bread @ meat @ bread))
