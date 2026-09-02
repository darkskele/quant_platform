# sim decisions

1. `SimExecution` wraps a `Matcher`: it owns the accumulate/drain bookkeeping, the matcher owns fill decisions.
2. Fills and rejects are stored in `ViewablePool`s and drained by span, not a queue.
