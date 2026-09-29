"""Measured trading cost for a sample of USDT perps, from trade prints and book depth.

Half spread comes from aggressor trades. In each minute, the mean price of
buyer-initiated trades minus the mean price of seller-initiated trades is one
spread. The median over minutes is the day's spread, and half of it is what a
taker pays to cross. Depth is the dollars resting within 1% of mid on each side.

Results are cached under ~/.cache/qp-research, so a rerun reads them back.
"""
from __future__ import annotations

import numpy as np
import pandas as pd

import stream

BUCKETS = [(5e8, np.inf, 2), (1e8, 5e8, 3), (3e7, 1e8, 3), (1e7, 3e7, 4), (3e6, 1e7, 4), (1e6, 3e6, 4), (3e5, 1e6, 3)]


def pick(liquidity: pd.Series, seed: int) -> list[str]:
    """Coins spread across liquidity bands, a fixed count drawn from each band."""
    rng = np.random.default_rng(seed)
    out = []
    for lo, hi, n in BUCKETS:
        band = liquidity[(liquidity >= lo) & (liquidity < hi)].index.tolist()
        out += list(rng.choice(sorted(band), size=min(n, len(band)), replace=False))
    return out


def measure(symbols: list[str], start: str, days: int, log=print) -> pd.DataFrame:
    """Half spread, depth and traded notional per coin over the days from start."""
    q = stream.module()
    um = int(q.BinanceMarket.UsdM)
    b = q.SubscriptionBuilder()
    for s in symbols:
        b.add(q.ExchangeId.Binance, um, s)
    sub = b.build()
    name = {sub.resolve(q.ExchangeId.Binance, um, s).symbol: s for s in symbols}

    pool = q.FetchPoolConfig()
    pool.workers = 32
    t0 = stream._ns(start)
    cfg = q.BinanceHistoricalConfig([q.StreamSpec(q.EndpointKind.AggTrades), q.StreamSpec(q.EndpointKind.BookDepth)],
                                    q.Cadence.Daily, t0, t0 + days * 86_400_000_000_000 - 1, pool)
    src = q.BinanceHistoricalSource(sub, cfg)
    src.plan()

    minutes = days * 1440
    acc = {s: [[0.0] * minutes, [0] * minutes, [0.0] * minutes, [0] * minutes] for s in symbols}
    notional = {s: 0.0 for s in symbols}
    depth = {s: [] for s in symbols}
    TRADE, BUY = q.EventKind.Trade, q.Side.Buy
    for e in src:
        sym = name[e.base.symbol]
        p = e.payload
        if e.base.kind == TRADE:
            m = (e.base.ts - t0) // 60_000_000_000
            if 0 <= m < minutes:
                a = acc[sym]
                if p.side == BUY:
                    a[0][m] += p.price; a[1][m] += 1
                else:
                    a[2][m] += p.price; a[3][m] += 1
                notional[sym] += p.price * p.qty
        else:
            depth[sym].append((p.bands.bids[0].notional + p.bands.asks[0].notional) / 2)

    for r in src.reports():
        if r.stats.rows_rejected:
            log(f'{start} {r.symbol} {str(r.kind).split(".")[-1]}: {r.stats.rows_rejected} rows rejected by the parser')

    rows = []
    for sym in symbols:
        sb, nb, ss, ns = (np.array(x, dtype=float) for x in acc[sym])
        spread = None
        for width in (1, 5, 15):
            k = minutes // width
            SB, NB, SS, NS = (x[:k * width].reshape(k, width).sum(axis=1) for x in (sb, nb, ss, ns))
            ok = (NB >= 2) & (NS >= 2)
            if ok.sum() >= 100:
                buy, sell = SB[ok] / NB[ok], SS[ok] / NS[ok]
                spread = np.median((buy - sell) / ((buy + sell) / 2)) * 1e4
                break
        rows.append({'symbol': sym, 'start': start, 'window_min': width if spread is not None else np.nan,
                     'half_spread_bps': spread / 2 if spread is not None else np.nan,
                     'depth_1pct_usd': np.median(depth[sym]) if depth[sym] else np.nan,
                     'traded_usd_per_day': notional[sym] / days, 'trades': int(nb.sum() + ns.sum())})
    log(f'{start}: {len(symbols)} coins measured')
    return pd.DataFrame(rows)


def sample(dates, days=2, log=print) -> pd.DataFrame:
    """Measure a liquidity-spread sample of coins at each date, cached."""
    import panel
    path = stream.CACHE / f"costs_{'_'.join(dates)}_{days}d.parquet"
    if path.exists():
        return pd.read_parquet(path)
    p = panel.load()
    out = []
    for i, d in enumerate(dates):
        wk = p.liquidity.index[p.liquidity.index >= pd.Timestamp(d, tz='UTC')][0]
        liq = p.liquidity.loc[wk].dropna()
        syms = pick(liq, seed=i)
        m = measure(syms, d, days, log)
        m['liquidity'] = m.symbol.map(liq)
        out.append(m)
    res = pd.concat(out, ignore_index=True)
    res.to_parquet(path)
    return res


FEE_BPS = 5.0
EXCLUDE = {'DEFIUSDT', 'FOOTBALLUSDT', 'BLUEBIRDUSDT', 'USDCUSDT'}


def clean(sample: pd.DataFrame) -> pd.DataFrame:
    """Drops composite index perps, the stablecoin pair, non-crypto perps and failed spread estimates."""
    return sample[~sample.symbol.isin(stream.non_crypto() | EXCLUDE) & (sample.half_spread_bps > 0)]


def ladder(sample: pd.DataFrame) -> dict:
    """Log-log fits of half spread and depth within 1% against daily dollar volume."""
    s = clean(sample)
    b1, a1 = np.polyfit(np.log10(s.liquidity), np.log10(s.half_spread_bps), 1)
    d = s.dropna(subset=['depth_1pct_usd'])
    b2, a2 = np.polyfit(np.log10(d.liquidity), np.log10(d.depth_1pct_usd), 1)
    return {'spread': (a1, b1), 'depth': (a2, b2)}


def one_way_bps(fit: dict, liquidity: pd.DataFrame, trade_usd: pd.DataFrame, fee_bps=FEE_BPS) -> pd.DataFrame:
    """Fee plus half spread plus impact, for a trade of trade_usd in a coin of that liquidity.

    Impact assumes resting size spread evenly across the 1% band, so taking
    trade_usd moves the average fill by half of trade_usd over the depth.
    """
    lv = np.log10(liquidity.clip(lower=1e5))
    half_spread = 10 ** (fit['spread'][0] + fit['spread'][1] * lv)
    depth = 10 ** (fit['depth'][0] + fit['depth'][1] * lv)
    return fee_bps + half_spread + 50 * trade_usd / depth
