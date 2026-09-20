# data_source decisions

1. `run_data_source` pairs N sources with N sinks 1:1 by position.
2. A source stamps its own events. The pump only moves them, since only the source can know which instrument a row belongs to.
3. An event a sink refuses is held and retried on the next round rather than dropped, so a full sink backpressures instead of losing data.
4. When no source produced an event it sleeps briefly. It runs until every source reports `Eof` or the control channel signals Stop.
5. The control channel is polled every N rounds, not every round, so a stop check stays off the per-event path.
6. Http and decompression live here rather than inside an exchange source, since every remote source needs both and neither is exchange-specific.
