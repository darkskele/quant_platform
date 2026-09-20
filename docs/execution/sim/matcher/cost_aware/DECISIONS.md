# cost_aware decisions

1. `CostAwareMatcher` composes a `CostModel` sub-seam. Static-dispatch, one CM per matcher instance.
2. `on_market_event` price tracking is duplicated rather than shared with `LastTradeMatcher`.
3. A cost-model miss on the order's `(exchange, market, symbol, ts)` produces `Reject{NoCostAvailable}`.
