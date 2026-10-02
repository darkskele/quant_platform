# parsers decisions

1. A parser is a stateless struct of static functions, not an object, so it costs nothing to hold one per stream.
2. `parse` takes the endpoint entry, so one parser serves every market that publishes its dataset.
3. Rows are parsed out of a `string_view` over the decompressed bytes. Nothing is copied to parse a row.
4. Decimals are read in a single pass into an int64 mantissa and scaled once, exact for the eight places Binance publishes.
5. Anything the digit scan will not take, such as an exponent, falls back to `from_chars` rather than failing.
6. Stamps are scaled to nanoseconds on read. A stamp at or above the microsecond floor is microseconds, everything below is milliseconds, and both appear inside one dataset.
7. A row that fails to parse is rejected and counted, never thrown on. One bad row must not end a sweep.
8. The three kline datasets share one row reader and differ only in which fields reach the event.
9. Kline events are stamped with close time, the first instant the close is known. Open time rides on the payload as the bar's identity. Funding events are stamped with calc time.
10. Datetime stamps are read as a fixed width field rather than through `strptime`. A field longer than the format is a different format, not this one with a tail, so it is rejected instead of truncated.
11. The civil calendar helpers live with the field readers, since the stamp parser and the stream's file window both need them and neither owns the other.
12. A blank optional column parses as NaN rather than rejecting the row. Coin-M metrics leaves three of its four long short ratios empty on every row, so rejecting on blank would drop the dataset entirely.
13. Open interest is mandatory in a metrics row and the four ratios are not, since open interest is the reason the dataset is read.
14. The metrics symbol column is consumed and dropped. The stream already knows the symbol it planned and the parser is not handed it.
15. aggTrades maps onto the existing trade event, with side as the taker's: a buyer who made the market means the taker sold.
16. An aggTrades row is stamped from its sixth field, transact time, since the first is the trade id.
17. aggTrades width comes from the endpoint. Spot carries an eighth column and futures do not, so a row of the wrong width for its market is rejected rather than trimmed.
18. Booleans are accepted in both cases, since futures write lowercase and spot capitalises, and nothing else is.
19. Coin-M trade quantity is contracts. The dataset has no base column and contract sizes are not known to the platform, so it is carried as published and marked on the event.
20. A trade carries the venue's aggregate id and the range of underlying fills it folds, so a gap, a replay or a trade count is visible downstream. They cost no width, since the kline payload already sets the variant's size.
21. Book depth reads the percent as a decimal and accepts the 0.2 bands files carry from 2026-01-14 as a pair or not at all. They are checked but not kept, so the event stays five whole-percent bands a side.
22. A close stamp keeps the unit it was published in. A microsecond spot bar lands up to a millisecond after the millisecond futures bar opening with it, so bars are joined on open time, not on the stamp.
