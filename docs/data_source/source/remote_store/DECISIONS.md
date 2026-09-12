# remote_store decisions

1. The store format is zstd parquet, Hive partitioned by market, dataset, symbol, year and month. Never CSV, never JSON.
2. The store speaks an S3 API. The local cache mirrors the layout so one reader serves both.
3. A catalog parquet at the store root lists every partition. Requests resolve against it, never by listing the bucket.
4. Decode happens in the producer thread. `next()` only merges.
5. Cache-backed replay first. Streaming range reads are a later variation on the same source.
6. The universe is a catalog query at run start, not a compiled table.
