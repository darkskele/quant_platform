import numpy as np
import pandas as pd
import polars as pl
import pytest

from qp_research import panel


def ts(s):
    return pd.Timestamp(s, tz='UTC')


def long_bars(closes: dict, start='2023-01-02', volume=None) -> pd.DataFrame:
    """Long-form daily bars from per-symbol close lists. NaN close or zero volume is an untraded day."""
    rows = []
    for sym, cs in closes.items():
        for i, c in enumerate(cs):
            day = ts(start) + pd.Timedelta(days=i)
            v = (volume or {}).get(sym, {}).get(i, 0.0 if np.isnan(c) else 1000.0)
            rows.append((sym, day, c, c, 0.0 if np.isnan(c) else c, v))
    out = pd.DataFrame(rows, columns=['symbol', 'day', 'high', 'low', 'close', 'volume'])
    return out


def growth(n, rate=0.01, start=100.0):
    return list(start * np.exp(rate * np.arange(n)))


def test_weekly_return_is_the_log_change_over_traded_days():
    b = panel.bars(long_bars({'A': growth(140)}), warmup=0)
    p = panel.build(b)
    # 2023-01-02 is a Monday, so every W-SUN period after the first holds seven days.
    full = p.returns['A'].iloc[1:-1]
    assert np.allclose(full, 0.07)
    assert p.returns.index[0] == ts('2023-01-08')


def test_warmup_skips_each_coins_first_bars():
    b = panel.bars(long_bars({'A': growth(60)}))
    assert b.returns['A'].iloc[:30].isna().all()
    assert b.returns['A'].iloc[30:].notna().all()


def test_period_needs_minimum_valid_bars():
    closes = growth(28)
    for i in (9, 11, 13):
        closes[i] = np.nan
    p = panel.build(panel.bars(long_bars({'A': closes}), warmup=0))
    # The week of 2023-01-09 to 2023-01-15 loses three days and the returns after them, so too few remain.
    assert np.isnan(p.returns.loc[ts('2023-01-15'), 'A'])
    # The next week's first return would span the halt on 2023-01-15, so six remain.
    assert p.returns.loc[ts('2023-01-22'), 'A'] == pytest.approx(0.06)


def test_halt_gap_books_in_pnl_not_in_signal_returns():
    closes = growth(21)
    closes[10] = np.nan
    closes[11] = 200.0
    b = panel.bars(long_bars({'A': closes}), warmup=0)
    assert np.isnan(b.returns.loc[ts('2023-01-13'), 'A'])
    assert b.held.loc[ts('2023-01-13'), 'A'] == pytest.approx(np.log(200.0 / closes[9]))
    p = panel.build(b, min_bars=1)
    assert p.gaps.loc[ts('2023-01-15'), 'A'] == pytest.approx(np.log(200.0 / closes[9]))
    held = np.log(closes[13] / closes[6])
    assert p.pnl.loc[ts('2023-01-15'), 'A'] == pytest.approx(np.expm1(held))


def test_redenomination_gap_is_dropped(monkeypatch):
    monkeypatch.setattr(panel.universe, 'REDENOMINATIONS', {('A', '2023-01-10')})
    closes = growth(14)
    closes[8:] = [c / 1000 for c in closes[8:]]
    b = panel.bars(long_bars({'A': closes}), warmup=0)
    assert np.isnan(b.held.loc[ts('2023-01-10'), 'A'])


def test_liquidity_and_vol_known_before_the_period():
    rng = np.random.default_rng(0)
    closes = list(100 * np.exp(np.cumsum(rng.normal(0, 0.03, 200))))
    base = panel.build(panel.bars(long_bars({'A': closes}), warmup=0))
    week = base.returns.index[20]
    bumped = closes.copy()
    first = (week - pd.Timedelta(days=6) - ts('2023-01-02')).days
    for i in range(first, len(bumped)):
        bumped[i] *= 3.0
    after = panel.build(panel.bars(long_bars({'A': bumped}), warmup=0))
    assert after.liquidity.loc[week, 'A'] == base.liquidity.loc[week, 'A']
    assert after.vol.loc[week, 'A'] == base.vol.loc[week, 'A']
    before = base.returns.index[base.returns.index < week]
    pd.testing.assert_frame_equal(after.returns.loc[before], base.returns.loc[before])


def test_liquidity_is_the_prior_periods_rolling_median():
    vol = {'A': {i: float(i + 1) for i in range(60)}}
    p = panel.build(panel.bars(long_bars({'A': [100.0] * 60}, volume=vol), warmup=0))
    week = ts('2023-02-05')
    # As of the close of 2023-01-29, day index 27, the last 30 days are indices 0 to 27, 28 of them.
    assert p.liquidity.loc[week, 'A'] == pytest.approx(np.median(np.arange(1, 29) * 100.0))


def test_funding_print_belongs_to_the_bar_it_falls_in():
    b = long_bars({'A': [100.0] * 21})
    prints = pd.DataFrame({'symbol': 'A', 'funding_rate': [0.01, 0.02, 0.04],
                           't': [ts('2023-01-08 16:00'), ts('2023-01-09 00:00'), ts('2023-01-15 16:00')]})
    p = panel.build(panel.bars(b, prints, warmup=0), min_bars=1)
    assert p.funding.loc[ts('2023-01-08'), 'A'] == pytest.approx(0.01)
    assert p.funding.loc[ts('2023-01-15'), 'A'] == pytest.approx(0.06)


def test_halted_only_when_trading_resumes():
    closes = {'A': [100.0] * 13 + [np.nan] * 2 + [100.0] * 6, 'B': [100.0] * 13 + [np.nan] * 8}
    p = panel.build(panel.bars(long_bars(closes), warmup=0), min_bars=1)
    assert p.halted.loc[ts('2023-01-22'), 'A']
    assert not p.halted.loc[ts('2023-01-22'), 'B']
    assert not p.halted.loc[ts('2023-01-15'), 'A']


def test_other_rebalance_days_share_the_bars():
    b = panel.bars(long_bars({'A': growth(70)}), warmup=0)
    wed = panel.build(b, 'W-WED')
    assert (wed.returns.index.dayofweek == 2).all()
    assert np.allclose(wed.returns['A'].iloc[1:-1], 0.07)
    assert wed.bars is b


def test_long_bars_hold_at_any_bar_size():
    hours = pd.date_range(ts('2024-01-01'), periods=6, freq='h')
    rows = pd.DataFrame({'symbol': ['A'] * 5 + ['B'] * 6, 'open_time': [*hours.delete(3), *hours],
                         'close': [100.0, 101.0, 102.0, 104.0, 105.0, 50, 50, 50, 50, 50, 50], 'volume': 1.0})
    out = panel.long(pl.from_pandas(rows).lazy(), at='open_time', warmup=0).collect().to_pandas()
    a = out[out.symbol == 'A'].set_index('open_time')
    # A has no bar at 03:00, which B has, so the 04:00 return would span the gap.
    assert np.isnan(a.returns.iloc[0]) and np.isnan(a.loc[hours[4], 'returns'])
    assert a.loc[hours[2], 'returns'] == pytest.approx(np.log(102 / 101))
    assert a.loc[hours[4], 'held'] == pytest.approx(np.log(104 / 102))
    wide = panel.bars(rows, at='open_time', warmup=0)
    assert wide.returns.index.equals(hours) and np.isnan(wide.close.loc[hours[3], 'A'])


def test_long_funding_is_paid_on_the_bar_it_falls_in_or_where_trading_resumes():
    hours = pd.date_range(ts('2024-01-01'), periods=6, freq='h')
    rows = pd.DataFrame({'symbol': 'A', 'open_time': hours.delete(3), 'close': 100.0, 'volume': 1.0})
    prints = pd.DataFrame({'symbol': 'A', 'funding_rate': [0.5, 0.01, 0.02, 0.04, 0.08, 0.16],
                           't': [ts('2023-12-31 23:00'), ts('2024-01-01 01:00'), ts('2024-01-01 01:59'),
                                 ts('2024-01-01 03:00'), ts('2024-01-01 04:00'), ts('2024-01-01 06:00')]})
    out = panel.long(rows, at='open_time', warmup=0, prints=prints, every='1h').collect().to_pandas()
    paid = out.set_index('open_time').funding
    # Before the first bar and after the last are dropped, the halted 03:00 print lands on 04:00.
    assert paid.to_dict() == {hours[0]: 0.0, hours[1]: pytest.approx(0.03), hours[2]: 0.0,
                              hours[4]: pytest.approx(0.12), hours[5]: 0.0}


def test_rolling_vol_and_liquidity_are_known_the_bar_before():
    days = pd.date_range(ts('2024-01-01'), periods=40, freq='D')
    rng = np.random.default_rng(1)
    close = 100 * np.exp(np.cumsum(rng.normal(0, 0.02, 40)))
    rows = pd.DataFrame({'symbol': 'A', 'day': days, 'close': close, 'volume': np.arange(1.0, 41.0)})
    out = panel.rolling(panel.long(rows, warmup=0), at='day', vol='10d', vol_min=5, liquidity='10d', liquidity_min=5)
    out = out.collect().to_pandas().set_index('day')
    wide = panel.bars(rows, warmup=0)
    vol = wide.returns['A'].rolling(10, min_periods=5).std().shift(1)
    liq = wide.dollar_volume['A'].rolling(10, min_periods=5).median().shift(1)
    assert np.allclose(out.vol, vol, equal_nan=True, rtol=1e-12) and out.vol.notna().sum() == vol.notna().sum()
    assert np.allclose(out.liquidity, liq, equal_nan=True, rtol=0) and np.isnan(out.liquidity.iloc[4])
    assert out.liquidity.iloc[5] == np.median(np.arange(1, 6) * close[:5])
