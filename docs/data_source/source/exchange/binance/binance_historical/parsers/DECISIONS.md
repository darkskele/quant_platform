# parsers decisions

1. A parser is a stateless struct of static functions, not an object, so it costs nothing to hold one per stream.
2. `parse` takes the endpoint entry, so one parser serves every market that publishes its dataset.
3. Rows are parsed out of a `string_view` over the decompressed bytes. Nothing is copied to parse a row.
4. Decimals are read in a single pass into an int64 mantissa and scaled once, exact for the eight places Binance publishes.
5. Anything the digit scan will not take, such as an exponent, falls back to `from_chars` rather than failing.
6. Stamps are scaled to nanoseconds on read. A stamp at or above the microsecond floor is microseconds, everything below is milliseconds, and both appear inside one dataset.
7. A row that fails to parse is rejected and counted, never thrown on. One bad row must not end a sweep.
8. The three kline datasets share one row reader and differ only in which fields reach the event.
9. Kline events are stamped with open time, funding events with calc time.
