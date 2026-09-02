# data_source decisions

1. `run_data_source` pairs N sources with N sinks 1:1 by position.
2. It stamps each event's `venue` with the source index before recording, so `(symbol, venue)` identifies an instrument once streams are merged.
3. When no source produced an event it sleeps briefly; it runs until the control channel signals Stop.
