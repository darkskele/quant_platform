# data_source

Pulls events from a set of `Source`s and records each into its paired `Sink`. `run_data_source` pairs N sources with N sinks 1:1 by position and drives them until the sources run dry or the control channel says stop.

## Diagram

```
run_data_source: source[i] paired with sink[i]

Source[0]::next() ─▶ event ─▶ Sink[0]::record()
Source[1]::next() ─▶ event ─▶ Sink[1]::record()
      ...                          │
                                   ▼
                               consumers
```

## Components

- `source/`. The `Source` concept and the sources that implement it.
- `sink/`. The `Sink` concept, in-process fan-out to consumers.
- `network/http/`. One blocking `get()` over https, with retries and a connection pool per host. What a remote source fetches through.
- `archive/`. Decompression of a fetched file.

## Milestones

- [x] ~~`run_data_source` pairs sources to sinks and drives the pipeline~~
- [x] ~~A source can fetch over the network and decompress~~
- [ ] `http` client on Asio C++20 coroutines, a few io threads holding many requests in flight instead of one blocked thread per request (C++20)
    - retry backoff awaits a `steady_timer` rather than sleeping the thread
- [ ] `run_data_source` walks its source and sink tuples with an expansion statement, no index sequence helper (C++26, GCC 16)
