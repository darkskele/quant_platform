# cost_model

The `CostModel` concept and the concrete cost models. A cost model prices a fill and its fee for an `Order` at a reference price and time. A miss is `nullopt` so the matcher can reject cleanly.

## Diagram

```
Order / Price / Timestamp
      │
      ▼
CostModel::price()             (cost_model.hpp: the concept)
      │   concrete cost models plug in here
      ▼
optional<FillPricing>
```

## Variations

- **`HalfSpreadLinearImpact`** (`half_spread_linear/`). Fill price crosses a half-spread plus a per-unit linear impact term. Fee is a bps rate on filled notional. Parameters come from a per-`(symbol, venue, week)` `CostRow` table. Ships with a cost-row reader for the artifact `research/build_cost_table.ipynb` produces.

## Milestones

- [x] ~~`CostModel` concept~~
- [x] ~~`HalfSpreadLinearImpact` with per-`(symbol, venue, week)` table~~
- [x] ~~Cost-row reader for the finalize step's CSV~~
- [ ] Fitted-from-trades variation
    - slippage regressed against public trade tapes
