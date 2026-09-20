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

- `exchange/`. The sources that speak a specific exchange's protocol, one per exchange and endpoint.

## Variations

- **`EofSource`** (`eof_source.hpp`). Reports `Eof` immediately. Stands in where a composition has no source wired yet.

## Milestones

- [x] ~~`Source` concept~~
- [x] ~~First remote source~~
- [ ] A live source
