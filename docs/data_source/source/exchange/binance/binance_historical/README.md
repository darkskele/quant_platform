# binance_historical

A `Source` over Binance Vision's public archive of historical CSV files. Builds one stream per subscribed instrument and dataset, fetches and decompresses the files off the pull thread, parses rows on it, and merges every stream into one ascending timestamp order.

## Diagram

```
Subscription x configured datasets
        │  one stream per (market, symbol, dataset, interval)
        ▼
BinanceHistoricalStream
   plan()   bucket listing ─▶ the file keys falling inside the span
   pump()   url + position ─▶ FetchPool ─▶ GET, unzip ─▶ SlotRing at that position
   next()   rows ─▶ Parser ─▶ MarketEvent, stamped with its instrument
        │
        ▼
   merge heap, earliest front event across every stream
        │
        ▼
   PullResult (MarketEvent | NoData | Eof)
```

## Components

- `parsers/`. The `Parser` seam and one parser per dataset.
- `FetchPool` (`fetch_pool.hpp`). The fetch seam. A stream names a URL and a ring position, the pool puts the decompressed file there. Failures arrive too, so a stream never waits on one forever.
- `BinanceHistoricalStream` (`stream.hpp`). One dataset for one symbol. Plans its own files, keeps its own prefetch window full, parses on the calling thread, and counts what it could not read.
- `endpoints.hpp`. The dataset table. Bucket path, cadence support, column count, and which column carries base-asset volume.
- `listing.hpp`. Bucket listing, keys or immediate children under a prefix, paged to exhaustion.

## Variations

- **`HttpFetchPool`** (`http_fetch_pool.hpp`). Worker threads over the shared http client. One GET plus one unzip per task, with retries, byte and failure counters, a recent-failure ring, and a critical callback when a window fails too often.

## Milestones

- [x] ~~Endpoint table and prefix building~~
- [x] ~~Bucket listing, paged~~
- [x] ~~Row parsers for klines, mark price, premium index and funding~~
- [x] ~~Stream with its own prefetch window and gap accounting~~
- [x] ~~Fetch pool with retries, failure accounting and a critical signal~~
- [x] ~~Source merges every stream in ascending `ts` order~~
- [x] ~~Correctness checked against live files~~
- [ ] Trades and book depth datasets
- [ ] Resume a planned run without refetching what was already read
