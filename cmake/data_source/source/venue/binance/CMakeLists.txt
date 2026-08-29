# source/venue/binance — Binance venue implementations. One subdirectory
# per (exchange, source-type) combination, not per exchange: historical
# needs its own Source (file reader) and its own Parser, genuinely
# different from whatever a future live venue would need, even though
# both trade the same markets (see docs/decisions.md).
add_subdirectory(binance_historical)
