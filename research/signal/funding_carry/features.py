"""Feature builders for the funding signal PnL loop.

Canonical event frame:
    columns [symbol, ts, realized_funding, premium]
    one row per (symbol, 8h funding timestamp).

    realized_funding at row t is the rate that pays over the interval
    starting at ts_t (i.e. what the position held at row t earns, this
    is also the label).

    premium at row t is the average premium index over the just completed
    interval ending at ts_t (observable at ts_t, safe to use as a feature).

Every feature is causal: at row t, no value depends on anything observed at
ts >= ts_t. Lags and EWMAs shift the label series by 1 first.

Each `add_*` returns the input frame with a new column, so calls chain.
"""

from __future__ import annotations

import numpy as np
import pandas as pd


BINANCE_INTEREST_RATE_8H = 0.0001  # 0.01%, the constant in the funding formula for USDT-M perps


def _by_symbol(events: pd.DataFrame, col: str) -> pd.core.groupby.SeriesGroupBy:
    return events.sort_values(["symbol", "ts"]).groupby("symbol", sort=False)[col]


def add_funding_lag(events: pd.DataFrame, k: int, name: str | None = None) -> pd.DataFrame:
    """Lag-k of realized_funding. Lag 1 is the most recently settled print."""
    out = events.copy()
    col = name or f"funding_lag{k}"
    out[col] = _by_symbol(out, "realized_funding").shift(k)
    return out


def add_funding_ewma(events: pd.DataFrame, halflife: float, name: str | None = None) -> pd.DataFrame:
    """EWMA over funding shifted by 1 (so row t only uses funding observed by ts_t)."""
    out = events.copy()
    col = name or f"funding_ewma_h{halflife:g}"
    shifted = _by_symbol(out, "realized_funding").shift(1)
    out[col] = shifted.groupby(out.sort_values(["symbol", "ts"])["symbol"], sort=False).transform(
        lambda s: s.ewm(halflife=halflife, adjust=False).mean()
    )
    return out


def add_funding_vol(events: pd.DataFrame, window: int, name: str | None = None) -> pd.DataFrame:
    """Rolling std of funding shifted by 1."""
    out = events.copy()
    col = name or f"funding_vol_w{window}"
    shifted = _by_symbol(out, "realized_funding").shift(1)
    out[col] = shifted.groupby(out.sort_values(["symbol", "ts"])["symbol"], sort=False).transform(
        lambda s: s.rolling(window, min_periods=max(2, window // 2)).std()
    )
    return out


def add_funding_mean_window(events: pd.DataFrame, window: int, name: str | None = None) -> pd.DataFrame:
    """Rolling mean of funding shifted by 1, over `window` intervals. Longer
    windows (90 intervals ~= 30 days) function as regime indicators."""
    out = events.copy()
    col = name or f"funding_mean_w{window}"
    shifted = _by_symbol(out, "realized_funding").shift(1)
    out[col] = shifted.groupby(out.sort_values(["symbol", "ts"])["symbol"], sort=False).transform(
        lambda s: s.rolling(window, min_periods=max(2, window // 4)).mean()
    )
    return out


def add_funding_sign_window(events: pd.DataFrame, window: int, name: str | None = None) -> pd.DataFrame:
    """Sign of the rolling-window funding mean. -1 in negative-funding regimes,
    0 in flat regimes, +1 in positive-funding regimes."""
    out = add_funding_mean_window(events, window, name=f"_tmp_mean_w{window}")
    col = name or f"funding_sign_w{window}"
    out[col] = np.sign(out[f"_tmp_mean_w{window}"]).astype(float)
    out = out.drop(columns=[f"_tmp_mean_w{window}"])
    return out


def add_funding_vol_rank(
    events: pd.DataFrame,
    vol_window: int,
    rank_window: int,
    name: str | None = None,
) -> pd.DataFrame:
    """Rolling percentile rank of funding volatility. Captures "we are in a
    high-vol regime relative to recent history" without hard thresholds.

    `vol_window` = window for the rolling std of funding.
    `rank_window` = window over which the vol series is percentile-ranked.
    """
    out = events.copy()
    col = name or f"funding_vol_rank_v{vol_window}_r{rank_window}"
    shifted = _by_symbol(out, "realized_funding").shift(1)
    by_sym = out.sort_values(["symbol", "ts"])["symbol"]
    vol = shifted.groupby(by_sym, sort=False).transform(
        lambda s: s.rolling(vol_window, min_periods=max(2, vol_window // 2)).std()
    )
    out[col] = vol.groupby(by_sym, sort=False).transform(
        lambda s: s.rolling(rank_window, min_periods=max(2, rank_window // 4)).rank(pct=True)
    )
    return out


def add_clamp_distance(events: pd.DataFrame, interest_rate: float = BINANCE_INTEREST_RATE_8H) -> pd.DataFrame:
    """premium - interest_rate. Sign and magnitude place us in the clamp band:
    values inside +-0.05% pass through unclamped; outside means the clamp bites."""
    out = events.copy()
    out["clamp_distance"] = out["premium"] - interest_rate
    return out


def add_premium_trend(events: pd.DataFrame, span: int = 3) -> pd.DataFrame:
    """EWMA of first-difference of premium. Positive means premium rising into settlement."""
    out = events.copy()
    diffs = out.sort_values(["symbol", "ts"]).groupby("symbol", sort=False)["premium"].diff()
    out[f"premium_trend_s{span}"] = diffs.groupby(out.sort_values(["symbol", "ts"])["symbol"], sort=False).transform(
        lambda s: s.ewm(span=span, adjust=False).mean()
    )
    return out


def add_cross_symbol_spread(events: pd.DataFrame, reference: str = "BTCUSDT") -> pd.DataFrame:
    """This symbol's most-recent funding minus the reference's, aligned on ts.
    Uses lag-1 funding on both sides so no future data leaks in."""
    out = events.copy()
    if "funding_lag1" not in out.columns:
        out = add_funding_lag(out, 1)
    ref = out.loc[out["symbol"] == reference, ["ts", "funding_lag1"]].rename(
        columns={"funding_lag1": "_ref_lag1"}
    )
    out = out.merge(ref, on="ts", how="left")
    out["cross_spread_vs_" + reference.lower()] = out["funding_lag1"] - out["_ref_lag1"]
    out = out.drop(columns=["_ref_lag1"])
    return out


def add_basket_zscore(events: pd.DataFrame, source: str, name: str | None = None) -> pd.DataFrame:
    """Cross-sectional z-score of `source` at each ts, across symbols in the frame.
    z = (x - cross_mean_at_t) / cross_std_at_t. Captures "this symbol's funding is
    unusually high/low relative to the basket right now" in a scale-free way."""
    out = events.copy()
    col = name or f"basket_z_{source}"
    g = out.groupby("ts")[source]
    out[col] = (out[source] - g.transform("mean")) / g.transform("std")
    return out


def add_basket_rank(events: pd.DataFrame, source: str, name: str | None = None) -> pd.DataFrame:
    """Cross-sectional rank of `source` at each ts (pct, 0-1). Robust to outliers
    where a z-score would blow up."""
    out = events.copy()
    col = name or f"basket_rank_{source}"
    out[col] = out.groupby("ts")[source].rank(pct=True)
    return out


def add_basket_spread(events: pd.DataFrame) -> pd.DataFrame:
    """This symbol's most-recent funding minus the cross-sectional mean across symbols."""
    out = events.copy()
    if "funding_lag1" not in out.columns:
        out = add_funding_lag(out, 1)
    basket = out.groupby("ts")["funding_lag1"].transform("mean")
    out["basket_spread"] = out["funding_lag1"] - basket
    return out


def add_cum_target(events: pd.DataFrame, horizon: int, name: str = "realized_cum") -> pd.DataFrame:
    """Add a forward cumulative funding label: sum of realized_funding over the
    next `horizon` intervals (inclusive of row t). Row t's label = what a
    position held constant from t through t+horizon-1 would earn in funding.

    Rows with fewer than `horizon` future observations are dropped."""
    out = events.sort_values(["symbol", "ts"]).reset_index(drop=True)
    out[name] = (
        out.groupby("symbol")["realized_funding"]
           .transform(lambda s: s.rolling(horizon, min_periods=horizon).sum().shift(-(horizon - 1)))
    )
    return out


def _test_add_cum_target() -> None:
    n = 40
    frame = pd.DataFrame({
        "symbol": ["BTC"] * n,
        "ts": pd.date_range("2024-01-01", periods=n, freq="8h", tz="UTC"),
        "realized_funding": np.linspace(1.0, 4.0, n),
    })
    horizon = 24
    labelled = add_cum_target(frame, horizon=horizon).dropna(subset=["realized_cum"])
    for row in labelled.itertuples(index=True):
        expected = frame["realized_funding"].iloc[row.Index : row.Index + horizon].sum()
        assert np.isclose(row.realized_cum, expected), (row.Index, row.realized_cum, expected)
    assert labelled.iloc[-1].name == n - horizon
    print(f"add_cum_target: {len(labelled)} rows verified against manual sum(next {horizon})")


def add_time_features(events: pd.DataFrame) -> pd.DataFrame:
    """Hour-of-day and day-of-week of the settlement ts."""
    out = events.copy()
    ts = pd.to_datetime(out["ts"], utc=True)
    out["hour_of_day"] = ts.dt.hour.astype(np.int8)
    out["day_of_week"] = ts.dt.dayofweek.astype(np.int8)
    return out


def aggregate_klines_to_intervals(
    klines: pd.DataFrame,
    interval_starts: pd.Series,
    interval_hours: int = 8,
) -> pd.DataFrame:
    """Aggregate 1-min klines into 8h buckets ending at each ts in `interval_starts`.

    `klines` columns expected: [ts_ms, open, high, low, close, volume,
    taker_buy_base]. Buckets cover (ts - interval_hours, ts]. Returns a frame
    indexed on the input interval_starts with taker_imbalance, realized_return,
    realized_vol_1m, high_low_range.
    """
    k = klines.copy()
    k["ts"] = pd.to_datetime(k["ts_ms"], unit="ms", utc=True)
    k = k.drop_duplicates("ts").sort_values("ts").set_index("ts")
    k["close"] = k["close"].astype(float)
    k["ret_1m"] = k["close"].pct_change()

    rule = f"{interval_hours}h"
    agg = k.resample(rule, closed="left", label="right").agg(
        close_first=("close", "first"),
        close_last=("close", "last"),
        high_max=("high", "max"),
        low_min=("low", "min"),
        volume_sum=("volume", "sum"),
        taker_buy_base_sum=("taker_buy_base", "sum"),
        ret_1m_std=("ret_1m", "std"),
    ).reset_index()

    with np.errstate(divide="ignore", invalid="ignore"):
        agg["taker_imbalance"] = np.where(
            agg["volume_sum"] > 0,
            (2.0 * agg["taker_buy_base_sum"] - agg["volume_sum"]) / agg["volume_sum"],
            np.nan,
        )
        agg["realized_return"] = np.where(
            agg["close_first"] > 0,
            agg["close_last"] / agg["close_first"] - 1.0,
            np.nan,
        )
        agg["high_low_range"] = np.where(
            agg["close_first"] > 0,
            (agg["high_max"] - agg["low_min"]) / agg["close_first"],
            np.nan,
        )
    agg["realized_vol_1m"] = agg["ret_1m_std"]

    keep = agg[["ts", "taker_imbalance", "realized_return", "realized_vol_1m", "high_low_range"]]

    # Original funding ts values sit ~milliseconds after the whole-8h boundary
    # (Binance funding settles just after the hour). Floor to the resample grid
    # to align, then restore original ts for downstream joins.
    wanted = pd.DataFrame({"ts_orig": pd.to_datetime(interval_starts, utc=True).to_numpy()})
    wanted["ts"] = pd.to_datetime(wanted["ts_orig"], utc=True).dt.floor(rule)
    merged = wanted.merge(keep, on="ts", how="left")
    merged["ts"] = merged["ts_orig"]
    return merged.drop(columns=["ts_orig"])


def add_kline_features(events: pd.DataFrame, klines_by_symbol: dict[str, pd.DataFrame]) -> pd.DataFrame:
    """Attach per-8h kline aggregates (taker imbalance, realized return / vol / range).

    `klines_by_symbol` maps symbol -> 1-min kline frame with columns
    [ts_ms, open, high, low, close, volume, taker_buy_base]. The aggregate at
    row t covers the just-completed interval ending at ts_t (observable at t)."""
    parts = []
    for symbol, g in events.groupby("symbol", sort=False):
        if symbol not in klines_by_symbol:
            parts.append(g.assign(taker_imbalance=np.nan, realized_return=np.nan,
                                  realized_vol_1m=np.nan, high_low_range=np.nan))
            continue
        agg = aggregate_klines_to_intervals(klines_by_symbol[symbol], g["ts"])
        parts.append(g.merge(agg, on="ts", how="left"))
    return pd.concat(parts, ignore_index=True)

if __name__ == "__main__":
    _test_add_cum_target()
