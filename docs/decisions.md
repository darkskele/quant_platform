# Decision log

Lightweight ADRs — the decisions made and why. Append, don't rewrite history.

## D1 — Target medium-frequency (MFT), not HFT
Cloud VMs are ms-latency; HFT needs colo + kernel-bypass + FPGAs. Compete on
signal quality + execution discipline + uptime, not tick-to-trade speed.

## D2 — Asset class: crypto perpetuals, Binance USD-M first
Free live L2 + free historical klines/trades/funding, no equities/CME data
licensing, 24/7 markets, cheap VMs near the exchange. Binance chosen for the
largest free dataset and deepest liquidity ("most room to feed the models").

## D3 — Same code in backtest and live (one code path)
Backtest and live are one Engine template with different policy types
(source/clock/execution/sink). Kills backtest-live divergence — the main way
retail quant projects die.

## D4 — Compile-time policies on hot/fixed seams, virtual on cold/runtime seams
Static dispatch: source, clock, execution, recorder. Virtual: strategy, risk.
Justified by cross-frequency-vs-latency-budget, not dogma.

## D5 — Production streamer first; collector is that streamer + a recorder sink
L2 order-book history isn't free and can't be backfilled — but rather than a
throwaway quick collector, build the *real* `LiveWebSocketSource` (socket + book
reconstruction) properly, and get the collector for free by wiring it to a
`FileRecorder` sink (no strategies). Foundation-first: the streamer is needed
anyway, and the collector falls out of it. Data-accumulation urgency is
deliberately deprioritized vs building the base right (the whole project will
take longer than estimated; do the simple, load-bearing parts well first).

## D6 — Strategy sequencing: carry → stat-arb → (factor) → microstructure ML
Increasing sophistication *and* data appetite. Families 1–3 use free data;
family 4 needs the accumulated L2 archive.

## D7 — Capital scale: small (£1–10k), paper-first
Prove determinism + parity + risk layer in paper/testnet before any real
capital. At this scale, cost-per-trade vs edge-per-trade rules strategy choice.

## D8 — Time budget: 5–8 h/week
Horizon ~6 months to live-with-small-real-capital on carry; ML is a 12-month+
arc gated on data accumulation.

## D9 — Dev on WSL Ubuntu, deploy to VM; storage on R2/B2 + local working set
WSL for dev + relative benchmarks; VM for absolute perf + 24/7 collection.
Object storage as cold archive, pull-slice-to-local for backtest.
