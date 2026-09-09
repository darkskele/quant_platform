# Honest cost matcher plan

Landing plan for the `CostModel` sub-seam and its first variation, plus the `CostAwareMatcher` that composes it. Parameters sourced from real Binance Vision quote and trade data via a cloud-run summarization step.

## Goal

`CostAwareMatcher` composing a `CostModel` sub-seam, with `HalfSpreadLinearImpact` as the first variation. Parameters live in a per-instrument per-week table sourced from Binance Vision bookTicker and aggTrades, summarized on Colab Pro, committed here as a small parquet.

## Cost decomposition

Three components on a market-order fill.

1. **Taker fee.** Exchange takes a % of notional. Public per venue and tier. Binance USD-M VIP0 taker is 4 bps across all our symbols.
2. **Spread cross.** Buys hit the ask, sells hit the bid. Reference-price strategies (mid or last) eat half the spread every trip.
3. **Market impact.** Larger orders walk resting depth; later units fill worse. Scales with size against available liquidity.

Latency slippage is deferred. Backtest bar granularity has no notion of within-tick arrival.

Funding on the short leg is not a cost we model here. `Portfolio::apply_funding` already settles it against `funding_mark_price_`.

## Why cloud for the data step

Binance Vision bookTicker is the raw event log of every quote change. Around 150 MB per day per BTC-liquidity symbol, tens of thousands of events per second. Full pull across 10 symbols and 3 years is 500 GB to 1 TB. The number we want is the median half-spread per week, tens of KB total. Massive asymmetry between transient input and final artifact.

Colab Pro handles the transient work. Stream-aggregate the raw, discard, keep only the summary. Local disk stays small. Same shape for aggTrades, an order of magnitude smaller but still worth doing there.

## Data (all free on Binance Vision)

| Data | Path | Used for |
|---|---|---|
| Klines 1-min OHLCV | `futures/um/daily/klines/<SYMBOL>/1m/` | Existing pipeline. Not used for cost model. |
| Funding rate | `futures/um/monthly/fundingRate/` | Portfolio funding settlement. Existing. |
| Mark price kline | `futures/um/daily/markPriceKlines/<SYMBOL>/1m/` | Funding mark price. Existing. |
| **Book ticker** | `futures/um/daily/bookTicker/<SYMBOL>/` | Half-spread column. Colab-only. |
| **Aggregate trades** | `futures/um/daily/aggTrades/<SYMBOL>/` | Impact column. Colab-only. |

L2 book depth is not published historically. Would need a paid vendor. Not needed for this scope.

## Cost table shape

One row per (symbol, venue, week_start). Columns.

- `symbol`. Ticker string.
- `venue`. 0 spot, 1 futures, matching the C++ Portfolio layout.
- `week_start`. ISO Monday of the week, `YYYY-MM-DD`.
- `half_spread_bps`. Weekly median from bookTicker.
- `impact_bps_per_unit`. Weekly median from aggTrades, Amihud-style illiquidity in bps per unit qty.
- `taker_fee_bps`. 4.0 constant for VIP0 USD-M perps, sourced from Binance's fee page.
- `n_days_book`, `n_days_trade`. Days that contributed to each column.

Weekly granularity is the stability sweet spot. Monthly is too coarse for our 3-year window. Daily is too noisy.

Table storage. Plain CSV at `data/cost_model/cost_table.csv`. Tens of KB, human-readable, git-diffable. Committed to the repo. Immutable once generated so a fresh clone doesn't have to re-run Colab.

## Research-side pipeline (Colab Pro)

`research/build_cost_table.ipynb`. Notebook. Iterates (symbol, day), streams each daily zip in memory, computes a small daily summary (median half-spread bps for bookTicker, median Amihud coefficient for aggTrades), checkpoints per-day summaries to Google Drive. Session-safe. When both checkpoints are populated, aggregates by ISO week and writes `cost_table.csv` to Drive.

Running instructions live in the notebook itself as its first markdown cell.

Once generated, download the CSV, drop into `data/cost_model/cost_table.csv`, commit.

## `CostModel` concept

Two methods, separate concerns. Static-dispatch concept.

```cpp
template <class T>
concept CostModel = requires(T c, const Order& o, Price ref) {
    { c.fill_price(o, ref) } -> std::same_as<Price>;
    { c.fee(o, Price{})   } -> std::same_as<Notional>;
};
```

Matcher orders the calls. Slippage-adjusted price first, then fee against that price.

## `HalfSpreadLinearImpact`

Formula unchanged from the mathematical shape.

- Buy fill at `ref × (1 + (half_spread + impact × qty) / 1e4)`.
- Sell mirrored.
- Fee `fill_price × qty × taker_fee_bps / 1e4`.

Config is a table, not a fixed triple.

```cpp
struct CostRow {
    SymbolId  symbol;
    VenueId   venue;
    Timestamp week_start;
    double    half_spread_bps;
    double    impact_bps_per_unit;
    double    taker_fee_bps;
};

class HalfSpreadLinearImpact {
    explicit HalfSpreadLinearImpact(std::vector<CostRow> table);
    Price    fill_price(const Order& o, Price ref) const;
    Notional fee(const Order& o, Price fill_price) const;
};
```

No default constructor, no defaults inside `CostRow`. Every parameter is sourced from the table.

Lookup uses (symbol, venue) and the order's ts to find the week bucket via a binary search over the sorted table. Miss is loud, not silent.

## `CostAwareMatcher<Book, CM>`

Same `on_market_event` price tracking as `LastTradeMatcher`, duplicated rather than abstracted. `try_fill` delegates to the `CostModel`. `Reject{NoPriceAvailable}` unchanged when no reference price is set.

## File layout

```
research/
    build_cost_table.ipynb                          (new, Colab notebook)

data/cost_model/
    cost_table.csv                                  (generated artifact, tracked)

include/execution/sim/matcher/
    matcher.hpp                                     (exists)
    last_trade/last_trade_matcher.hpp               (exists, untouched)
    cost_aware/cost_aware_matcher.hpp               (new)
    cost_model/
        cost_model.hpp                              (new, concept)
        half_spread_linear/
            half_spread_linear.hpp                  (new, first variation)
            cost_row.hpp                            (new, table row type)
            table_loader.hpp                        (new, CSV to vector<CostRow>)

tests/execution/sim/matcher/
    test_matcher.cpp                                (exists)
    cost_aware/test_cost_aware_matcher.cpp          (new)
    cost_model/half_spread_linear/test_half_spread_linear.cpp    (new)
    cost_model/half_spread_linear/test_table_loader.cpp          (new)
    scripted/test_round_trip_delta_neutral.cpp      (new, integration)

docs/execution/sim/matcher/
    README.md                                       (add Components on landing)
    DECISIONS.md                                    (append)
    cost_model/README.md                            (new)
    cost_model/DECISIONS.md                         (new)
```

## Test plan

### Unit, `HalfSpreadLinearImpact`

- Table lookup finds the right row for (symbol, venue, ts).
- Buy fill crosses the half-spread and includes impact when non-zero.
- Sell mirrored.
- Fee equals `fill_price × qty × taker_fee_bps / 1e4`.
- Lookup miss (unknown symbol, ts outside the table) fails loudly.
- All expected numbers computed by hand in the test.

### Unit, `CostAwareMatcher`

- No reference price set. `Reject{NoPriceAvailable}` parity with `LastTradeMatcher`.
- Reference set. `Fill` with price from `CostModel::fill_price`, fee from `CostModel::fee`.
- `static_assert(Matcher<CostAwareMatcher<...>>)`.

### Unit, table loader

- Round-trip a small artifact into `vector<CostRow>`. Fields preserved.
- Reject malformed input, missing required columns.

### Integration, scripted round-trip delta-neutral

The truth-test. Small hand-authored cost table with known values.

- Open. Buy 1 spot, sell 1 perp. Both cross known half-spreads, both pay 4 bps fees.
- Funding print at +0.02%, mark price 50000, short position −1. Cash credit exactly `+0.0002 × 50000 × 1 = +10`.
- Close. Reverse both legs.
- Hand-computed total PnL asserted exactly.
- Mirror case with funding −0.02%.

## Doc updates on landing

`matcher/README.md` grows a Components section listing `cost_model/`. Existing milestone struck. `cost_aware` bulleted under Variations.

`matcher/DECISIONS.md` appends the seam decision and the deferred items.

`cost_model/README.md` describes the concept and its variation. `cost_model/DECISIONS.md` records the table shape, the weekly bucket, the Colab-run pipeline, and the deferred items.

## Order of work

1. **Colab run.** Open `research/build_cost_table.ipynb` in Colab Pro, run bookTicker + aggTrades over the full window, aggregate to weekly, download `cost_table.csv` and drop into `data/cost_model/`.
2. **`CostModel` concept.** Header and docs.
3. **Table loader.** CSV to `vector<CostRow>`, unit tests.
4. **`HalfSpreadLinearImpact`.** Header, unit tests, table lookup by binary search.
5. **`CostAwareMatcher`.** Header, unit tests, `static_assert`.
6. **Scripted round-trip test.** Integration proof.
7. **Docs sweep.** Strike milestone, add Components, log DECISIONS.
8. **Sensitivity sweep.** Separate task. Run the frozen v5 predictor as a stub strategy through the sim at (real cost table, zero-cost table) points. Tells us how load-bearing the honesty is. Not part of this merge.

## Deferred, named

Each gets a line in the appropriate `DECISIONS.md` when we ship.

- **Maker fees.** Arrives with limit orders. Taker-only today.
- **Latency between decision and fill.** Not meaningful at bar granularity. Own concern when live.
- **Partial fills.** Whole-fill only today.
- **Margin, min-notional, size-cap rejects.** Only `NoPriceAvailable` today.
- **Book-walking matcher.** Own milestone in the matcher README. Needs L2 depth data, paid vendor territory.
- **Per-session spread variation.** Weekly is stable enough. Hourly or session-of-day is a later refinement if needed.
- **Amihud-to-linear-impact scaling.** Amihud is `|Δlog_price| / qty`. We store it directly and let the matcher apply as `impact × qty`. If a nonlinear (sqrt or power) shape fits better after live data lands, the class name still describes the current linear form.

## What comes after this

Reminders, not scope of this plan.

1. **Pybind `Strategy` adapter.** A concept-satisfying C++ type that forwards `on_event` to a Python callable, plus a pybind entrypoint that runs the engine loop from a notebook. Lets strategy iteration happen in Python while the engine and executor stay in C++.
2. **Strategy design in Python.** Take the frozen v5 predictor (`research/funding_signal_model.json`) and iterate strategy shape against the honest cost sim. Absolute-carry threshold first, then sizing rules, hold horizon, per-symbol vs pooled book. Measure on cost-aware PnL from day one.
3. **Pybind `RiskGate` adapter.** Same trick on the risk seam. Two responsibilities, intent-filter (veto opens) and flatten trigger.
4. **Regime gating in risk.** Rule-based first, keyed on regime features already in `research/features.py`. Second-model idea (a classifier for "bad regime to trade") parked until rules leave real value on the table.
