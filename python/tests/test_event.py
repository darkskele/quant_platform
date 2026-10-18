import numpy as np
import pandas as pd
import polars as pl
import pytest

from qp_research import costs, panel
from qp_research.event import EventBook, Exits

T0 = pd.Timestamp('2024-01-01', tz='UTC')


def hour(k):
    return T0 + pd.Timedelta(hours=k)


def bars(closes: dict, prints=None, vol=0.01):
    """Hourly long-form bars from close lists per symbol, NaN for an untraded hour, flat vol and liquidity."""
    rows = [(s, hour(k), 100.0 if np.isnan(c) else c, 0.0 if np.isnan(c) else 1.0)
            for s, cs in closes.items() for k, c in enumerate(cs)]
    rows = pd.DataFrame(rows, columns=['symbol', 'open_time', 'close', 'volume'])
    if prints is None:
        prints = pd.DataFrame({'symbol': pd.Series(dtype=str), 't': pd.Series(dtype='datetime64[ns, UTC]'),
                               'funding_rate': pd.Series(dtype=float)})
    b = panel.long(rows, at='open_time', warmup=0, prints=prints, every='1h')
    return b.with_columns(vol=pl.lit(vol), liquidity=pl.lit(1e6))


def triggers(*rows):
    return pd.DataFrame(rows, columns=['symbol', 'open_time', 'side', 'strength'])


def book(b, **kw):
    return EventBook(b, costs.Flat(0.0), day_scale=24, **kw)


def path(start, moves):
    """Closes from start by log moves."""
    return list(start * np.exp(np.concatenate([[0.0], np.cumsum(moves)])))


def test_take_profit_exits_at_the_first_close_past_the_target():
    b = bars({'A': path(100, [0.004, 0.004, 0.004, 0.004, 0.004, 0.004])})
    t = book(b, slots=1).run(triggers(('A', hour(0), 1, 1.0)), Exits(take=1.0, stop=2.0, hold=5)).trades
    assert t.reason.tolist() == ['take'] and t.exit.tolist() == [hour(3)]
    assert t.gross.iloc[0] == pytest.approx(np.exp(0.012) - 1)


def test_a_short_stops_out_and_the_time_stop_holds_its_bars():
    b = bars({'A': path(100, [0.006, 0.006, 0.006, 0.0, 0.0]), 'B': [100.0] * 6})
    r = book(b, slots=2).run(triggers(('A', hour(0), -1, 1.0), ('B', hour(0), 1, 1.0)), Exits(take=1.0, stop=1.0, hold=4)).trades
    a, c = r.set_index('symbol').loc['A'], r.set_index('symbol').loc['B']
    assert a.reason == 'stop' and a.exit == hour(2) and a.gross == pytest.approx(-(np.exp(0.012) - 1) / 2)
    assert c.reason == 'time' and c.exit == hour(4) and c.bars == 4


def test_a_halted_coin_exits_when_trading_resumes_and_an_ended_one_on_its_last_bar():
    b = bars({'A': [100.0, 100.0, np.nan, np.nan, 110.0, 110.0], 'B': [100.0, 100.0, 100.0]})
    r = book(b, slots=2).run(triggers(('A', hour(0), 1, 1.0), ('B', hour(0), 1, 1.0)), Exits(None, None, hold=2)).trades
    r = r.set_index('symbol')
    assert r.loc['A', 'reason'] == 'halt' and r.loc['A', 'exit'] == hour(4) and r.loc['A', 'gross'] == pytest.approx(0.05)
    assert r.loc['B', 'reason'] == 'time' and r.loc['B', 'exit'] == hour(2)
    ended = book(b, slots=1).run(triggers(('B', hour(1), 1, 1.0)), Exits(None, None, hold=5)).trades
    assert ended.reason.tolist() == ['end'] and ended.exit.tolist() == [hour(2)]


def test_delay_enters_bars_after_the_trigger():
    b = bars({'A': path(100, [0.01, 0.02, 0.0, 0.0])})
    r = book(b, slots=1).run(triggers(('A', hour(0), 1, 1.0)), Exits(None, None, hold=2, delay=1)).trades
    assert r.entry.tolist() == [hour(1)] and r.gross.iloc[0] == pytest.approx(np.exp(0.02) - 1)


def test_overlap_rules_take_stronger_triggers_first_and_respect_coin_and_book_limits():
    b = bars({s: [100.0] * 8 for s in 'ABC'})
    t = triggers(('A', hour(0), 1, 1.0), ('B', hour(0), 1, 3.0), ('C', hour(0), 1, 2.0),
                 ('B', hour(1), 1, 9.0), ('A', hour(3), 1, 1.0))
    r = book(b, slots=2).run(t, Exits(None, None, hold=3)).trades
    # B and C fill both slots, B again is already held, A at 03:00 takes the slot B and C free then.
    assert list(zip(r.symbol, r.entry)) == [('B', hour(0)), ('C', hour(0)), ('A', hour(3))]


def test_daily_book_marks_to_market_and_books_cost_and_funding():
    closes = path(100, [0.01] * 30)
    prints = pd.DataFrame({'symbol': 'A', 't': [hour(8), hour(16)], 'funding_rate': [0.001, 0.002]})
    b = bars({'A': closes}, prints=prints)
    r = EventBook(b, costs.Flat(10.0), day_scale=24, slots=2).run(triggers(('A', hour(20), 1, 1.0)), Exits(None, None, hold=8))
    d, t = r.daily, r.trades.iloc[0]
    # Held from the 20:00 close into the next day, half the book, so both days carry part of the move.
    assert list(d.index) == [T0, T0 + pd.Timedelta(days=1)] and (d.gross > 0).all()
    assert d.gross.sum() == pytest.approx(t.gross) == pytest.approx(0.5 * (np.exp(0.08) - 1))
    assert t.cost == pytest.approx(-0.5 * 10 / 1e4 * (1 + np.exp(0.08)))
    assert d.funding.sum() == 0.0
    early = EventBook(b, costs.Flat(0.0), day_scale=24, slots=1).run(triggers(('A', hour(6), -1, 1.0)), Exits(None, None, hold=12))
    # A short is paid both prints, on its value when each falls.
    assert early.trades.funding.iloc[0] == pytest.approx(0.001 * np.exp(0.01) + 0.002 * np.exp(0.09))


def test_cost_sees_liquidity_scaled_to_a_day():
    seen = []

    class Spy:
        def bps(self, liquidity, dw):
            seen.append(np.asarray(liquidity))
            return np.zeros(np.shape(liquidity))

    b = bars({'A': [100.0] * 6})
    EventBook(b, Spy(), day_scale=24, slots=1).run(triggers(('A', hour(0), 1, 1.0)), Exits(None, None, hold=2))
    assert np.allclose(seen[0], 24e6)


def test_effective_count_is_the_sum_of_uniqueness():
    b = bars({s: [100.0] * 10 for s in 'ABCD'})
    same = book(b, slots=4).run(triggers(('A', hour(0), 1, 1.0), ('B', hour(0), 1, 1.0)), Exits(None, None, hold=4))
    apart = book(b, slots=4).run(triggers(('C', hour(0), 1, 1.0), ('D', hour(4), 1, 1.0)), Exits(None, None, hold=4))
    assert same.effective == pytest.approx(1.0) and apart.effective == pytest.approx(2.0)


def test_a_hedged_trade_earns_the_spread_and_pays_both_legs():
    prints = pd.DataFrame({'symbol': ['A', 'H'], 't': [hour(2), hour(2)], 'funding_rate': [0.001, 0.003]})
    b = bars({'A': path(100, [0.01] * 6), 'H': path(100, [0.004] * 6)}, prints=prints)
    r = EventBook(b, costs.Flat(10.0), day_scale=24, slots=1, hedge='H').run(triggers(('A', hour(0), 1, 1.0)), Exits(None, None, hold=4))
    t = r.trades.iloc[0]
    assert r.trades.symbol.tolist() == ['A'] and t.reason == 'time'
    assert t.gross == pytest.approx((np.exp(0.04) - 1) - (np.exp(0.016) - 1))
    # Long A pays its print, short H is paid its own, each on its value when the print falls.
    assert t.funding == pytest.approx(-0.001 * np.exp(0.01) + 0.003 * np.exp(0.004))
    assert t.cost == pytest.approx(-10 / 1e4 * (1 + np.exp(0.04) + 1 + np.exp(0.016)))


def test_hedged_barriers_read_the_spread():
    b = bars({'A': path(100, [0.01] * 6), 'H': path(100, [0.01] * 6)})
    alone = book(b, slots=1).run(triggers(('A', hour(0), 1, 1.0)), Exits(take=1.0, stop=1.0, hold=5)).trades
    hedged = book(b, slots=1, hedge='H').run(triggers(('A', hour(0), 1, 1.0)), Exits(take=1.0, stop=1.0, hold=5)).trades
    assert alone.reason.tolist() == ['take'] and hedged.reason.tolist() == ['time']
    assert hedged.gross.iloc[0] == pytest.approx(0.0, abs=1e-12)


def test_a_hedged_trade_waits_for_the_hedge_to_trade():
    b = bars({'A': [100.0] * 6, 'H': [np.nan, 100.0, 100.0, 100.0, 100.0, 100.0]})
    t = book(b, slots=1, hedge='H').run(triggers(('A', hour(0), 1, 1.0), ('A', hour(1), 1, 1.0)), Exits(None, None, hold=2)).trades
    assert t.entry.tolist() == [hour(1)]


def test_revert_exits_once_a_share_of_the_gap_is_undone_since_the_trigger():
    # A fell 2 vol into the trigger, the long fills a bar later and the bounce is read from the trigger.
    b = bars({'A': path(100, [-0.01, -0.01, 0.004, 0.004, 0.004, 0.004, 0.004])})
    t = pd.DataFrame({'symbol': ['A'], 'open_time': [hour(2)], 'side': [1], 'strength': [2.0], 'gap': [-2.0]})
    r = book(b, slots=1).run(t, Exits(take=None, stop=None, hold=6, delay=1, revert=0.5)).trades
    # Undone from the 02:00 close is 0.4, 0.8, 1.2 vol, past half the gap at 05:00.
    assert r.reason.tolist() == ['revert'] and r.entry.tolist() == [hour(3)] and r.exit.tolist() == [hour(5)]
