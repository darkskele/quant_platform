# exchange

The `Source` implementations that speak a specific exchange's protocol. An exchange source owns everything exchange-specific behind `next()`: its endpoints, its symbol universe, its file or stream layout, and its own recovery. Nothing above it knows which exchange it is.

## Diagram

```
one directory per exchange, one source per exchange and endpoint

<exchange>/<endpoint>/
        │   Subscription decides the markets and symbols
        ▼
   Source::next()
        │
        ▼
   PullResult (MarketEvent | NoData | Eof)
```

## Variations

- **[`binance_historical`](binance/binance_historical/README.md)** (`binance/binance_historical/`). Binance Vision's historical CSV archive, fetched, decompressed and merged into one timestamp order.

## Milestones

- [x] ~~First exchange source, historical~~
- [ ] Binance live, websocket plus REST resync
- [ ] A second exchange, to find what actually generalises
