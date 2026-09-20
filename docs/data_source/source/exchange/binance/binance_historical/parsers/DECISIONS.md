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
10. Datetime stamps are read as a fixed width field rather than through `strptime`. A field longer than the format is a different format, not this one with a tail, so it is rejected instead of truncated.
11. The civil calendar helpers live with the field readers, since the stamp parser and the stream's file window both need them and neither owns the other.
12. A blank optional column parses as NaN rather than rejecting the row. Coin-M metrics leaves three of its four long short ratios empty on every row, so rejecting on blank would drop the dataset entirely.
13. Open interest is mandatory in a metrics row and the four ratios are not, since open interest is the reason the dataset is read.
14. The metrics symbol column is consumed and dropped. The stream already knows the symbol it planned and the parser is not handed it.
