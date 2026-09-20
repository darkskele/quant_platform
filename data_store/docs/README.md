# Data store

One store, one canonical event shape, two readers.

Records are MarketEvent PODs. On disk they are zstd parquet, partitioned by venue, market, event kind, symbol, and time bucket. A catalog file at the root names what exists.

```
sink  -->  partitions + catalog  -->  source
                                 -->  loader
```

## Components

- `catalog`. Index over partitions, one row per file, at the root.
- `partitions`. Parquet files laid out as `<venue>/<market>/<kind>/symbol=<S>/year=<Y>/month=<M>/`. Row shape is a MarketEvent variant's flat fields.
- `sink`. Writes MarketEvents into partitions. Used by every ingest run and by the live recorder.
- `source`. Reads partitions back into MarketEvents, near-memcpy per column.
- `loader`. Python read API, returns Arrow tables.

## Milestones

- [x] Catalog schema locked
- [ ] Per-EventKind row schemas locked
- [ ] Python catalog and loader over synthetic partitions
- [ ] Python ingest converts the 30 GB of tagged CSVs into partitions
- [ ] Round-trip proof, one partition matches the source rows
- [ ] Bucket set up, tree pushed
- [ ] C++ sink, live recorder wired on it
- [ ] C++ source, backtest reads through it
