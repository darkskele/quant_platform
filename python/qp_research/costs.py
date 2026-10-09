"""Trading cost models in one-way basis points per trade, and the measurement that fits a ladder.

Every model prices a weight change given the coin's liquidity. A ladder prices
a taker order as the fee, half the spread and the impact of eating into the
book, with spread and depth fitted against daily dollar volume.

Half spread is measured from aggressor trades. In each minute, the mean price
of buyer-initiated trades minus the mean price of seller-initiated trades is
one spread. The median over minutes is the day's spread. Depth is the dollars
resting within 1% of mid on each side.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import pandas as pd

from . import cache as cache_, engine
from .data import universe

# Taker fee per side at the base tier.
FEES = {'usdm': 5.0, 'spot': 10.0}

# Liquidity bands and the coins drawn from each when sampling.
BUCKETS = [(5e8, np.inf, 2), (1e8, 5e8, 3), (3e7, 1e8, 3), (1e7, 3e7, 4), (3e6, 1e7, 4), (1e6, 3e6, 4), (3e5, 1e6, 3)]
# Composite index perps and the stablecoin pair, whose spread is not a coin's.
EXCLUDE = {'DEFIUSDT', 'FOOTBALLUSDT', 'BLUEBIRDUSDT', 'USDCUSDT'}


@dataclass(frozen=True)
class Ladder:
    """Log-log lines of half spread in bps and depth within 1% in dollars against daily dollar volume."""
    spread: tuple[float, float]
    depth: tuple[float, float]

    def half_spread_bps(self, liquidity):
        return 10 ** (self.spread[0] + self.spread[1] * np.log10(np.clip(liquidity, 1e5, None)))

    def depth_usd(self, liquidity):
        return 10 ** (self.depth[0] + self.depth[1] * np.log10(np.clip(liquidity, 1e5, None)))


# 72 USDT perps measured over two days from 2023-03-10, 2024-03-10, 2025-03-10 and 2026-06-10.
PERPS_LADDER = Ladder(spread=(2.0103404763096764, -0.2738586941533421), depth=(-0.7160494356491773, 0.8183787124741141))


@dataclass(frozen=True)
class Flat:
    """The same cost on every trade."""
    one_way: float

    def bps(self, liquidity: pd.DataFrame, dw: pd.DataFrame) -> pd.DataFrame:
        return pd.DataFrame(self.one_way, index=liquidity.index, columns=liquidity.columns)


@dataclass(frozen=True)
class Provisional:
    """A flat cost by liquidity band, liquid coins above floor at one rate and the rest at another."""
    liquid: float = 7.5
    thin: float = 15.0
    floor: float = 1e7

    def bps(self, liquidity: pd.DataFrame, dw: pd.DataFrame) -> pd.DataFrame:
        return pd.DataFrame(np.where(liquidity >= self.floor, self.liquid, self.thin), index=liquidity.index,
                            columns=liquidity.columns)


@dataclass(frozen=True)
class Taker:
    """Fee plus half spread plus impact off a ladder, for a book of size dollars.

    Impact assumes resting size spread evenly across the 1% band, so taking a
    trade moves the average fill by half the trade over the depth.
    """
    fee: float
    ladder: Ladder
    size: float

    def bps(self, liquidity: pd.DataFrame, dw: pd.DataFrame) -> pd.DataFrame:
        trade = dw.abs() * self.size
        return self.fee + self.ladder.half_spread_bps(liquidity) + 50 * trade / self.ladder.depth_usd(liquidity)


def for_market(market: str, size: float, ladder: Ladder = PERPS_LADDER) -> Taker:
    """Taker cost at a market's fee off the ladder, for a book of size dollars."""
    return Taker(FEES[market], ladder, size)


def pick(liquidity: pd.Series, seed: int) -> list[str]:
    """Coins spread across liquidity bands, a fixed count drawn from each band."""
    rng = np.random.default_rng(seed)
    out = []
    for lo, hi, n in BUCKETS:
        band = liquidity[(liquidity >= lo) & (liquidity < hi)].index.tolist()
        out += list(rng.choice(sorted(band), size=min(n, len(band)), replace=False))
    return out


def measure(symbols: list[str], start, days: int, log=print) -> pd.DataFrame:
    """Half spread, depth and traded notional per coin over the days from start, streamed through the qp source."""
    q = engine.module()
    um = int(q.BinanceMarket.UsdM)
    b = q.SubscriptionBuilder()
    for s in symbols:
        b.add(q.ExchangeId.Binance, um, s)
    sub = b.build()
    name = {sub.resolve(q.ExchangeId.Binance, um, s).symbol: s for s in symbols}

    pool = q.FetchPoolConfig()
    pool.workers = 32
    t0 = int(cache_.utc(start).value)
    cfg = q.BinanceHistoricalConfig([q.StreamSpec(q.EndpointKind.AggTrades), q.StreamSpec(q.EndpointKind.BookDepth)],
                                    q.Cadence.Daily, t0, t0 + days * 86_400_000_000_000 - 1, pool)
    src = q.BinanceHistoricalSource(sub, cfg)
    src.plan()

    minutes = days * 1440
    # Per coin, per minute: buy price sum, buy count, sell price sum, sell count.
    acc = {s: [[0.0] * minutes, [0] * minutes, [0.0] * minutes, [0] * minutes] for s in symbols}
    notional = dict.fromkeys(symbols, 0.0)
    depth = {s: [] for s in symbols}
    trade, buy = q.EventKind.Trade, q.Side.Buy
    for e in src:
        sym, p = name[e.base.symbol], e.payload
        if e.base.kind == trade:
            m = (e.base.ts - t0) // 60_000_000_000
            if 0 <= m < minutes:
                a = acc[sym]
                row = 0 if p.side == buy else 2
                a[row][m] += p.price
                a[row + 1][m] += 1
                notional[sym] += p.price * p.qty
        else:
            depth[sym].append((p.bands.bids[0].notional + p.bands.asks[0].notional) / 2)
    failed = src.fetch_stats().completed_failed
    if failed:
        raise RuntimeError(f'{failed} archive fetches failed, nothing measured')

    rows = []
    for sym in symbols:
        sb, nb, ss, ns = (np.array(x, dtype=float) for x in acc[sym])
        spread, width = None, np.nan
        # Widen the window until enough minutes see both sides trade.
        for width in (1, 5, 15):
            k = minutes // width
            SB, NB, SS, NS = (x[:k * width].reshape(k, width).sum(axis=1) for x in (sb, nb, ss, ns))
            ok = (NB >= 2) & (NS >= 2)
            if ok.sum() >= 100:
                bought, sold = SB[ok] / NB[ok], SS[ok] / NS[ok]
                spread = np.median((bought - sold) / ((bought + sold) / 2)) * 1e4
                break
        rows.append({'symbol': sym, 'start': cache_.utc(start), 'window_min': width if spread is not None else np.nan,
                     'half_spread_bps': spread / 2 if spread is not None else np.nan,
                     'depth_1pct_usd': np.median(depth[sym]) if depth[sym] else np.nan,
                     'traded_usd_per_day': notional[sym] / days, 'trades': int(nb.sum() + ns.sum())})
    log(f'{cache_.utc(start).date()}: {len(symbols)} coins measured')
    return pd.DataFrame(rows)


def sample(dates, liquidity: pd.DataFrame, days=2, cache=None, log=print) -> pd.DataFrame:
    """A liquidity-spread sample of coins measured at each date, cached per coin and date.

    liquidity is a period panel. Coins are drawn from the first period at or after each date.
    """
    out = []
    for i, d in enumerate(dates):
        lo = cache_.utc(d)
        liq = liquidity.loc[liquidity.index[liquidity.index >= lo][0]].dropna()
        fetch = lambda syms, a, b: (measure(syms, a, days, log), b)
        m = (cache or cache_.default()).load('binance', 'usdm', f'cost_sample_{days}d', pick(liq, seed=i),
                                             cache_.span(lo, lo + pd.Timedelta(days=days)), lo + pd.Timedelta(days=days),
                                             fetch, 'start')
        m['liquidity'] = m.symbol.map(liq)
        out.append(m)
    return pd.concat(out, ignore_index=True)


def clean(sample: pd.DataFrame) -> pd.DataFrame:
    """Drops composite index perps, the stablecoin pair, non-crypto perps and failed spread estimates."""
    return sample[~sample.symbol.isin(universe.non_crypto() | EXCLUDE) & (sample.half_spread_bps > 0)]


def fit(sample: pd.DataFrame) -> Ladder:
    """The ladder fitted on a cleaned sample, log-log against daily dollar volume."""
    s = clean(sample)
    b1, a1 = np.polyfit(np.log10(s.liquidity), np.log10(s.half_spread_bps), 1)
    d = s.dropna(subset=['depth_1pct_usd'])
    b2, a2 = np.polyfit(np.log10(d.liquidity), np.log10(d.depth_1pct_usd), 1)
    return Ladder(spread=(float(a1), float(b1)), depth=(float(a2), float(b2)))
