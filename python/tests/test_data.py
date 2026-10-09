import pandas as pd
import pytest

from qp_research import cache as cache_, engine
from qp_research.data import binance, universe


def ts(s):
    return pd.Timestamp(s, tz='UTC')


NOW = ts('2026-10-04 10:00')


def test_horizon():
    assert binance.horizon(NOW) == (ts('2026-10-03'), ts('2026-09-01'))
    assert binance.horizon(ts('2026-10-12')) == (ts('2026-10-11'), ts('2026-10-01'))


def test_plan_both_uses_monthly_then_daily():
    windows, through = binance.plan('both', ts('2026-01-01'), ts('2027-01-01'), NOW)
    assert windows == [('Monthly', ts('2026-01-01'), ts('2026-09-01')), ('Daily', ts('2026-09-01'), ts('2026-10-03'))]
    assert through == ts('2026-10-03')


def test_plan_closed_span_is_monthly_only():
    windows, through = binance.plan('both', ts('2024-01-01'), ts('2025-01-01'), NOW)
    assert windows == [('Monthly', ts('2024-01-01'), ts('2025-01-01'))] and through == ts('2025-01-01')


def test_plan_single_cadence_datasets():
    assert binance.plan('monthly', ts('2026-01-01'), ts('2027-01-01'), NOW) == (
        [('Monthly', ts('2026-01-01'), ts('2026-09-01'))], ts('2026-09-01'))
    assert binance.plan('daily', ts('2026-10-01'), ts('2026-11-01'), NOW) == (
        [('Daily', ts('2026-10-01'), ts('2026-10-03'))], ts('2026-10-03'))
    assert binance.plan('daily', ts('2026-11-01'), ts('2026-12-01'), NOW) == ([], ts('2026-11-01'))


def test_runs():
    days = pd.DatetimeIndex([ts('2024-01-02'), ts('2024-01-03'), ts('2024-01-05')])
    assert binance.runs(days) == [(ts('2024-01-02'), ts('2024-01-03')), (ts('2024-01-05'), ts('2024-01-05'))]


def test_universe_filters(monkeypatch):
    listing = {'usdm': ['BTCUSDT', 'BTCUSDT_250328', 'ETHBUSD', 'TSLAUSDT'],
               'spot': ['BTCUSDT', 'USDCUSDT', 'BTCUPUSDT', 'ETHBTC', '币安人生USDT', 'PAXGUSDT']}
    monkeypatch.setattr(universe, 'listed', lambda market='usdm': listing[market])
    monkeypatch.setattr(universe, 'non_crypto', lambda: {'TSLAUSDT'})
    assert universe.perps() == ['BTCUSDT']
    assert universe.perps(crypto_only=False) == ['BTCUSDT', 'TSLAUSDT']
    assert universe.spot_pairs() == ['BTCUSDT']


network = pytest.mark.skipif(not engine.built(), reason='qp_python_backtest not built')


@pytest.mark.network
@network
def test_daily_bars_stream_once_then_cache(tmp_path, monkeypatch):
    c = cache_.Cache(tmp_path)
    calls = []
    real = binance._stream
    monkeypatch.setattr(binance, '_stream', lambda *a: calls.append(a) or real(*a))
    bars = binance.daily_bars(['BTCUSDT', 'ETHUSDT'], '2024-01-01', '2024-04-01', cache=c)
    assert len(bars) == 2 * 91
    assert bars.day.min() == ts('2024-01-01') and bars.day.max() == ts('2024-03-31')
    assert (bars.close > 0).all()
    n = len(calls)
    again = binance.daily_bars(['BTCUSDT', 'ETHUSDT'], '2024-02-01', '2024-03-01', cache=c)
    assert len(calls) == n and len(again) == 2 * 29


@pytest.mark.network
@network
def test_funding_prints_every_eight_hours(tmp_path):
    f = binance.funding(['BTCUSDT'], '2024-01-01', '2024-02-01', cache=cache_.Cache(tmp_path))
    assert len(f) == 93
    assert (f.t.diff().dropna() - pd.Timedelta(hours=8)).abs().max() < pd.Timedelta(seconds=1)


@pytest.mark.network
@network
def test_listing_through_the_engine(tmp_path, monkeypatch):
    monkeypatch.setattr(cache_, '_default', cache_.Cache(tmp_path))
    names = universe.listed('usdm')
    assert len(names) > 1000 and 'BTCUSDT' in names
    assert (tmp_path / 'listings' / 'usdm.txt').exists()
