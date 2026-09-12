# remote_store

A `Source` that replays partitioned parquet from an object store, or from a local cache of it, as `MarketEvent`s. One store, one layout, whether the bytes come over HTTP or off disk.

## Diagram

```
object store                      local cache
<market>/<dataset>/symbol=/year=/month=/part.parquet   (same layout both sides)
        │  range GET                     │  read
        └──────────────┬─────────────────┘
                       ▼
          Catalog: request (datasets, symbols, span) ─▶ partition list
                       ▼
          producer thread: decode row groups ─▶ per-stream queue
                       ▼
          RemoteStoreSource::next()   merges N streams in ascending ts
                       ▼
          PullResult (MarketEvent | NoData | Eof)
```

## Layout

- Zstd parquet, Hive partitioned by market, dataset, symbol, year, month.
- A catalog parquet at the store root, one row per partition. Symbol, first and last ts, row count, bytes.
- The cache mirrors the layout under the shared data dir. A reader never knows which side it opened.

## Components

- `catalog/`: resolves a request to the partition list and answers what the store holds. Reads the catalog file, never lists the bucket.
- `fetch/`: materializes partitions into the cache ahead of a run, idempotent, verifies size against the catalog.
- `decode/`: turns a parquet row group into `MarketEvent`s inside the producer thread.

## Variations

- **cache-backed**: every partition fetched before the run, the producer thread reads local files only.
- **streaming**: the producer thread range-reads row groups from the store, no local working set.

## Milestones

- [ ] Store layout and catalog
- [ ] Ingest into the layout, catalog rebuilt on every write
- [ ] `fetch` into the cache
- [ ] `decode` row group to `MarketEvent`
- [ ] `RemoteStoreSource` cache-backed, N-stream ts merge
- [ ] Universe resolved from the catalog at run start
- [ ] `RemoteStoreSource` streaming
