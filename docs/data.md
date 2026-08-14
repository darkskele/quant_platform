# Data: what's free, what isn't, and how we store it

## Verified reality (checked Aug 2026)

Three distinct things; only two are free.

**Free, no API key, download today — historical klines, trades, aggTrades, and
funding rates** for spot and USD-M/COIN-M futures, as daily/monthly CSV dumps
from `data.binance.vision`, back to listing. Confirmed via the official
[binance/binance-public-data](https://github.com/binance/binance-public-data)
repo (curl/wget, no auth).

**Free, no API key, live — the L2 order-book depth WebSocket streams**
(diff-depth, book ticker). Confirmed via Binance's
[Market Data Only](https://developers.binance.com/docs/binance-spot-api-docs/faqs/market_data_only)
doc: market-data streams need no authentication. An API key is only needed to
*trade* or read *your* account.

**NOT free — historical L2/L3 order-book depth.** Binance does not publish
order-book history in the free dumps (only klines/trades). Off-the-shelf book
history is sold by [Tardis.dev](https://tardis.dev/) at roughly $350/mo
(academic) to $1,000+/mo (derivatives L2 tier).

## The move: self-collect L2 for free

Run a collector on a cheap VM that records the free **live** depth stream 24/7
into our own files, starting now. Cost = a small VM. Tradeoffs (honest):

- **Forward-only** — you only get data from when you start; no backfill. This is
  why the collector runs from day one.
- **Diff stream, not a book.** Binance sends "level X → size Y" deltas. To
  reconstruct the book you pull one REST snapshot, then apply diffs, tracking
  sequence numbers so you never silently desync. This is the first real piece
  of correctness-critical engineering.
- **Uptime is the product.** The VM must stay connected, reconnect cleanly
  (the socket *will* drop), re-sync the book after each reconnect, and log gaps
  honestly so we know which minutes are trustworthy.

**Legal/ToS:** recording for our own research/trading is standard and accepted.
The line is **redistribution/resale** — never sell or publish the raw feed. If
we publish anything, we publish signals/returns, not the feed.

## Storage sizing

- **Klines / funding / trades (families 1–3):** tiny. All symbols, minute bars,
  years — single-digit to low-tens of GB. Fits on the VM local disk; no cloud
  needed early.
- **Self-collected L2 depth (family 4):** the whole storage story. One liquid
  perp's full diff stream is ~a few–10 GB/day uncompressed, ~0.5–2 GB/day
  compressed (zstd, ~5–10×). A handful of symbols → low single-digit GB/day
  compressed → roughly **few-hundred-GB to ~1–2 TB/year**.

### Format discipline (get this right from commit #1)

- **Never JSON on disk** — 5–10× bloat, slow replay. Store normalized **binary**
  events (with sequence numbers) or Parquet.
- **Compress on write** (zstd).
- **Partition by symbol + date** so a backtest loads one slice without reading
  everything.
- **Record only what you'll use** — full diff for *target* symbols, coarse
  top-of-book/klines for the broad universe.
- Record **raw/normalized events, not reconstructed snapshots**, so the book can
  be rebuilt later when the logic improves (lossless, future-proof).

## Cloud storage & backtest latency

Backtests **never** read events directly off object storage. Pattern:

1. **Object storage = cold archive.** Collector writes daily compressed
   partitions to e.g. Cloudflare R2 / Backblaze B2 (~$0.006–0.015/GB/mo, low/no
   egress). ~1 TB/yr ≈ **$6–15/mo**. (S3 is fine but charges ~$0.09/GB egress —
   avoid for repeated pulls.)
2. **Local disk = working set.** Pull the slice you need (a week of one symbol)
   to the VM's NVMe/SSD once, then replay **sequentially from local disk**,
   memory-mapped, at GB/s. Network latency is paid once as a bulk transfer, not
   per event.
3. **Block storage** (EBS gp3, ~$0.08–0.10/GB/mo) if the active set outgrows the
   instance disk.

Backtest is **throughput-bound, not latency-bound** — a good binary/partitioned
dataset replays millions of events/sec on one core. All-in storage cost even a
year into L2 collection: **under ~$20–30/mo**.

Sources:
[binance/binance-public-data](https://github.com/binance/binance-public-data),
[Binance Market Data Only](https://developers.binance.com/docs/binance-spot-api-docs/faqs/market_data_only),
[Binance WebSocket depth streams](https://developers.binance.com/docs/derivatives/usds-margined-futures/websocket-market-streams),
[Tardis.dev](https://tardis.dev/).
