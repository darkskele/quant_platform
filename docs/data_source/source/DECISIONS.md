# source decisions

1. `Source` is the pull seam: `next()` returns a `PullResult` carrying the next `MarketEvent`, or `NoData`/`Eof`.
2. `CsvSource` is templated on a venue `Parser` and merges N line streams in ascending `ts` order with a per-stream lookahead.
