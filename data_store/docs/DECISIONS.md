# Decisions

1. Store partitioned as `<venue>/<market>/<kind>/symbol=<S>/year=<Y>/month=<M>/part.parquet`, one file per partition.
2. `kind` is the EventKind name, one dataset per kind. All rows in a file are the same MarketEvent variant.
3. Row shape per file is that variant's flat fields, one column per member.
4. Catalog is one row per partition file, held in `catalog.parquet` at the root, rebuilt on every write.
5. Catalog columns are `venue, market, dataset, symbol, year, month, day, first_ts, last_ts, rows, bytes, path, sha256, schema_version, parser, ingested_at`. `dataset` holds the EventKind name.
6. Primary key is `(venue, market, dataset, symbol, year, month, day)`. Path is unique.
7. Month and day are nullable so year-partitioned and day-partitioned datasets share the table.
8. Timestamps are parquet `TIMESTAMP(NANOS, UTC)`, nanoseconds since epoch on the wire.
9. Provenance is encoded in the dataset name and path, not a catalog column. A recorded stream is a distinct dataset from its archive counterpart.
10. Ingest translates every venue's raw or CSV format into MarketEvent variants. One ingest parser per venue per source format.
11. Ingest is C++. Each venue's historical archive is a Source that fetches, unzips in memory, and emits MarketEvents. The pipeline `HistSource -> DataStoreSink` writes straight to the store root, local or bucket, with no intermediate download to disk.
12. The sink is one implementation reused by every ingest run and by the live recorder. Backtest and live share it at the write boundary.
13. The C++ source is one implementation reused across all venues. Venue lives in `VenueId` on each event, not in the reader.
14. Backtest and Python share the on-disk parquet plus the catalog schema. They do not share code.
15. Python side is pure Python on pyarrow and duckdb. No pybind, no C++ dep for Python users.
16. Row-group decode in C++ is bulk column memcpy into MarketEvent PODs. No per-row string parsing.
17. MarketEvent variants grow to cover every kind stored. Consumers that don't handle a kind ignore it.
18. Parquet is the L1 partition format. Layout and catalog are format-agnostic. A `format` column is added if a second format ever earns its place.
19. Each partition file carries a `sha256` in the catalog so integrity does not depend on the store's ETag.
20. L0 raw source bytes are not kept. Binance Vision zips are immutable and always re-fetchable, and the ingest pipeline reads them in memory.
21. Root is a path on the shared local disk while tier 1 fits it, and an S3-API bucket afterwards. Same tree, one root string.
22. Bucket provider is chosen at tier 2, not now. Shortlist is Backblaze B2 and Cloudflare R2. Both speak S3, layout does not care.
23. Catalog schema is defined once in `schema.py` as a pyarrow schema and imported by every writer.
24. Repo layout is `data_store/{python,docs}`. C++ sink and source live under `data_source/` following the existing module convention. `data_store/` holds only the physical layout, the catalog schema, and the Python read side.
25. One Source per venue owns all cross-stream coordination. `BinanceHistoricalSource` and `BinanceLiveSource` each expose only `next() -> MarketEvent`. Book snapshot alignment, WS sequence-gap recovery, universe listings, market-specific resync offsets stay inside the Source.
26. Venue Sources take a config selecting market, endpoints, symbols, and span. Not one Source instance per endpoint.
27. `RecordingFanOutSink` is a fanout that always prepends `DataSourceRecordSink`. Composed, not inherited. Named for intent so a run declares "this is being recorded" at the type level.
