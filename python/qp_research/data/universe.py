"""Which symbols count, per market.

Listings come from the archive through the qp source, delisted symbols
included, and are kept for a day beside the cache. Perps drop dated contracts
and, by default, those whose underlying is not crypto. Spot drops stablecoins,
fiat, tokenised gold and leveraged tokens.
"""
from __future__ import annotations

import json
import re
import time
import urllib.error
import urllib.request

from .. import cache, engine

EXCHANGE_INFO = 'https://fapi.binance.com/fapi/v1/exchangeInfo'
FOLDERS = {'usdm': 'futures/um', 'spot': 'spot', 'coinm': 'futures/cm'}
REFRESH = 86_400   # seconds a listing is kept

NOT_COINS = {'USDC', 'TUSD', 'PAX', 'BUSD', 'USDS', 'USDSB', 'USDP', 'DAI', 'FDUSD', 'UST', 'EUR', 'GBP', 'AUD',
             'PAXG', 'XAUT'}
LEVERAGED = re.compile(r'(UP|DOWN|BULL|BEAR)USDT$')

# Symbol and the day trading resumed at a new denomination, so the gap is not a move.
REDENOMINATIONS = {('BNXUSDT', '2023-02-22'), ('VENUSDT', '2018-10-19')}


def _get(url: str) -> bytes:
    """The body at url, retried on a dropped connection. An HTTP error is raised at once."""
    for attempt in range(4):
        try:
            return urllib.request.urlopen(url, timeout=60).read()
        except urllib.error.HTTPError:
            raise
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            if attempt == 3:
                raise
            time.sleep(2 ** attempt)


def _kept(name: str, pull) -> str:
    """pull()'s text, reread from beside the cache while younger than REFRESH."""
    path = cache.default().root / 'listings' / name
    if not path.exists() or time.time() - path.stat().st_mtime > REFRESH:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(pull())
    return path.read_text()


def listed(market='usdm') -> list[str]:
    """Every symbol the archive carries klines for, monthly or daily."""
    folder, children = FOLDERS[market], engine.module().list_children
    pull = lambda: '\n'.join(sorted(set(children(f'data/{folder}/monthly/klines/')) | set(children(f'data/{folder}/daily/klines/'))))
    return _kept(f'{market}.txt', pull).split()


def non_crypto() -> set[str]:
    """USDT perps whose underlying is not crypto, by Binance's own tags.

    TradFi perpetuals cover equities, ETFs, commodities, FX and pre-IPO names.
    RWA tokens are tagged crypto but track a real asset. Every TradFi perp is
    live, so the current exchange info covers them all.
    """
    info = json.loads(_kept('usdm_exchange_info.json', lambda: _get(EXCHANGE_INFO).decode()))
    return {s['symbol'] for s in info['symbols']
            if s.get('contractType') == 'TRADIFI_PERPETUAL' or 'RWA' in s.get('underlyingSubType', [])}


def perps(crypto_only=True) -> list[str]:
    """Every USDT perpetual the archive has carried, delisted included."""
    names = [n for n in listed('usdm') if n.endswith('USDT') and '_' not in n]
    drop = non_crypto() if crypto_only else set()
    return [n for n in names if n not in drop]


def spot_pairs() -> list[str]:
    """Every USDT spot pair the archive has carried, coins only."""
    return [n for n in listed('spot')
            if n.endswith('USDT') and n.isascii() and n[:-4] not in NOT_COINS and not LEVERAGED.search(n)]
