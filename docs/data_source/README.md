# data_source

Pulls events from a set of `Source`s and records each into its paired `Sink`. `run_data_source` pairs N sources with N sinks 1:1 by position and stamps each event's venue.

## Diagram

```
run_data_source: source[i] paired with sink[i]

Source[0]::next() ─▶ event (venue = 0) ─▶ Sink[0]::record()
Source[1]::next() ─▶ event (venue = 1) ─▶ Sink[1]::record()
      ...                                       │
                                                ▼
                                            consumers
```

## Components

- `source/`: the `Source` concept, historical replay plus venue parsing.
- `sink/`: the `Sink` concept, in-process fan-out to consumers.

## Milestones

- [x] ~~`run_data_source` pairs sources to sinks and drives the pipeline~~
