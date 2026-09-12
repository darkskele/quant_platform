# source

The `Source` concept and the sources that implement it. A source yields the next `MarketEvent`, or a `SourceStatus` (`NoData`/`Eof`), via `next()`.

## Diagram

```
Source::next()      (source.hpp: the concept)
        │   concrete sources plug in here
        ▼
   PullResult (MarketEvent | NoData | Eof)
```

## Components

- `venue/`: the `SymbolTable` and `Parser` concepts a source uses to turn raw venue bytes into `MarketEvent`s.

## Variations

- **`CsvSource`** (`csv_source.hpp`): replays historical CSV, parsing each line through a venue `Parser` and merging N streams in ascending `ts` order.
- **[`remote_store`](remote_store/README.md)** (`remote_store/`): replays partitioned parquet from an object store or its local cache, decoding row groups off the pull thread and merging N streams in ascending `ts` order. Planned.

## Milestones

- [x] ~~`Source` concept~~
- [x] ~~`CsvSource` historical replay~~
- [ ] `RemoteStoreSource` over the store cache
- [ ] `RemoteStoreSource` streaming range reads
