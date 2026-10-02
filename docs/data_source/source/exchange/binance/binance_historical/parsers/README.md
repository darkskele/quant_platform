# parsers

The `Parser` seam and a parser per historical dataset. A parser turns one CSV row, or for a grouped dataset the run of rows making one event, into a timestamp and a payload, and names the dataset and event kind it belongs to.

## Diagram

```
CSV row (string_view)
        │
        ▼
Parser::parse()      (parser.hpp: the concept)
        │   concrete parsers plug in here
        ▼
   bool, with ts and payload filled
```

## Components

- `csv_field.hpp`. Single-pass field readers. Fixed-point decimals, integers, and stamps normalised to nanoseconds.
- `kline_row.hpp`. The seven columns the three kline datasets share, read once.

## Variations

- **`KlineParser`** (`klines.hpp`). Open, high, low, close and base-asset volume.
- **`MarkPriceKlineParser`** (`mark_klines.hpp`). Same shape without volume, which is always zero on this dataset.
- **`PremiumIndexKlineParser`** (`premium_klines.hpp`). Same shape again, values are rates and go negative.
- **`FundingParser`** (`funding.hpp`). Calc time, interval hours, and the rate.
- **`MetricsParser`** (`metrics.hpp`). Open interest and its value, plus four optional long short ratios.
- **`AggTradesParser`** (`agg_trades.hpp`). One aggregate trade, its id range and the taker's side.
- **`BookDepthParser`** (`book_depth.hpp`). A grouped dataset. One sample of five whole-percent depth bands a side from a block of rows.

## Milestones

- [x] ~~`Parser` concept~~
- [x] ~~Single-pass field readers~~
- [x] ~~A parser per published dataset~~
- [x] ~~Trades and book depth rows~~
