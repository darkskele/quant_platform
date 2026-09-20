# exchange decisions

1. A source is per exchange and per endpoint, since an exchange's historical archive and its live feed share nothing but the exchange name.
2. One source owns every stream of its exchange and merges them itself. Cross-stream ordering, gap recovery and universe discovery are exchange-specific and stay inside.
3. The symbol universe comes from the `Subscription` at construction, not a compiled table, so adding a symbol is config.
4. A source stamps every event with its own `(exchange, market, symbol)` ids. Nothing above it can know them.
5. There is no shared parser or symbol-table concept across exchanges. Each exchange's wire format is its own problem, and a common seam waits for a third implementation.
