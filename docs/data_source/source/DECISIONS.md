# source decisions

1. `Source` is the pull seam: `next()` returns a `PullResult` carrying the next `MarketEvent`, or `NoData`/`Eof`.
2. `NoData` and `Eof` are distinct, so a caller can tell a source that is waiting from one that is finished. A source that never finishes simply never returns `Eof`.
3. A source stamps each event with its own `(exchange, market, symbol)` ids, resolved from the `Subscription` it was built with.
4. A source that merges several streams merges them itself and hands out one ascending timestamp order. The seam is one event at a time.
5. `next()` does no blocking work. Fetching and decoding belong on the source's own threads.
