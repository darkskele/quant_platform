import numpy as np
import pandas as pd
import pytest

from qp_research import cache as cache_, costs, engine


def frame(values, cols=('A', 'B')):
    return pd.DataFrame(values, columns=list(cols), dtype=float)


def test_flat_ignores_liquidity_and_size():
    liq = frame([[1e6, 1e9]])
    assert (costs.Flat(7.5).bps(liq, frame([[0.1, 0.5]])).values == 7.5).all()


def test_provisional_splits_on_the_liquidity_floor():
    out = costs.Provisional().bps(frame([[9.9e6, 1e7]]), frame([[0.1, 0.1]]))
    assert list(out.iloc[0]) == [15.0, 7.5]


def test_taker_known_answer():
    ladder = costs.Ladder(spread=(2.0, -0.25), depth=(-1.0, 1.0))
    liq, dw = frame([[1e8, 1e8]]), frame([[0.1, -0.1]])
    out = costs.Taker(5.0, ladder, 1e5).bps(liq, dw)
    half_spread = 10 ** (2.0 - 0.25 * 8)
    depth = 10 ** (-1.0 + 8)
    assert out.iloc[0, 0] == pytest.approx(5.0 + half_spread + 50 * 1e4 / depth)
    assert out.iloc[0, 1] == out.iloc[0, 0]


def test_taker_clips_tiny_liquidity():
    ladder = costs.PERPS_LADDER
    out = costs.Taker(5.0, ladder, 1e5).bps(frame([[1e3, 1e5]]), frame([[0.1, 0.1]]))
    assert out.iloc[0, 0] == out.iloc[0, 1]


def test_taker_matches_the_trend_lane_formula():
    rng = np.random.default_rng(0)
    liq = frame(10 ** rng.uniform(4, 10, (50, 2)))
    dw = frame(rng.uniform(-0.2, 0.2, (50, 2)))
    fit = {'spread': costs.PERPS_LADDER.spread, 'depth': costs.PERPS_LADDER.depth}
    lv = np.log10(liq.clip(lower=1e5))
    old = 5.0 + 10 ** (fit['spread'][0] + fit['spread'][1] * lv) + 50 * (dw.abs() * 1e5) / 10 ** (fit['depth'][0] + fit['depth'][1] * lv)
    pd.testing.assert_frame_equal(costs.Taker(5.0, costs.PERPS_LADDER, 1e5).bps(liq, dw.abs()), old)


def test_for_market_fees():
    assert costs.for_market('usdm', 1e5).fee == 5.0
    assert costs.for_market('spot', 1e5).fee == 10.0
    assert costs.for_market('usdm', 1e6).size == 1e6


def test_more_size_costs_more():
    liq, dw = frame([[1e7, 1e7]]), frame([[0.1, 0.1]])
    small = costs.for_market('usdm', 1e4).bps(liq, dw)
    big = costs.for_market('usdm', 1e6).bps(liq, dw)
    assert (big > small).all().all()


def test_pick_draws_per_band_and_reproduces():
    liq = pd.Series({f'C{i}': v for i, v in enumerate(np.geomspace(4e5, 2e9, 200))})
    a, b = costs.pick(liq, seed=3), costs.pick(liq, seed=3)
    assert a == b and len(a) == sum(n for _, _, n in costs.BUCKETS)
    assert costs.pick(liq, seed=4) != a


def test_fit_recovers_a_known_ladder(monkeypatch):
    monkeypatch.setattr(costs.universe, 'non_crypto', lambda: {'XAUUSDT'})
    liq = np.geomspace(1e6, 1e10, 40)
    sample = pd.DataFrame({'symbol': [f'C{i}USDT' for i in range(40)], 'liquidity': liq,
                           'half_spread_bps': 10 ** (2.0 - 0.27 * np.log10(liq)),
                           'depth_1pct_usd': 10 ** (-0.7 + 0.82 * np.log10(liq))})
    junk = pd.DataFrame({'symbol': ['XAUUSDT', 'DEFIUSDT', 'BADUSDT'], 'liquidity': [1e8] * 3,
                         'half_spread_bps': [50.0, 50.0, -1.0], 'depth_1pct_usd': [1.0] * 3})
    ladder = costs.fit(pd.concat([sample, junk]))
    assert ladder.spread == pytest.approx((2.0, -0.27))
    assert ladder.depth == pytest.approx((-0.7, 0.82))


@pytest.mark.network
@pytest.mark.skipif(not engine.built(), reason='qp_python_backtest not built')
def test_measure_and_sample_a_thin_coin(tmp_path, monkeypatch):
    m = costs.measure(['ZILUSDT'], '2024-03-10', 1, log=lambda s: None)
    row = m.iloc[0]
    assert row.trades > 1000 and 0 < row.half_spread_bps < 50 and row.depth_1pct_usd > 0
    liq = pd.DataFrame({'ZILUSDT': [5e5, 5e5]}, index=pd.DatetimeIndex([cache_.utc('2024-03-03'), cache_.utc('2024-03-10')]))
    c = cache_.Cache(tmp_path)
    first = costs.sample(['2024-03-10'], liq, days=1, cache=c, log=lambda s: None)
    assert list(first.symbol) == ['ZILUSDT'] and first.liquidity.iloc[0] == 5e5
    assert first.half_spread_bps.iloc[0] == row.half_spread_bps
    monkeypatch.setattr(costs, 'measure', lambda *a, **k: pytest.fail('measured twice'))
    costs.sample(['2024-03-10'], liq, days=1, cache=c, log=lambda s: None)
