# cost_model decisions

1. `CostModel` is a static-dispatch concept with a single `price()` method returning `optional<FillPricing>`. One lookup, one result. 
2. The cost-row reader is a free-function template on the `SymbolTable`. Unknown symbols are skipped, missing required fields throw.
3. Empty `impact_bps_per_unit` in the CSV maps to 0.0 to accommodate weeks that were AR-filled without an accompanying aggTrades summary.
