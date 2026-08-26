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

## ~~D4 — Compile-time policies on hot/fixed seams, virtual on cold/runtime seams~~
~~Static dispatch: source, clock, execution, recorder. Virtual: strategy,~~
~~risk. Justified by cross-frequency-vs-latency-budget, not dogma.~~
**Superseded by D27** — the "huge budget" premise was wrong; strategy/risk
move to static dispatch too.

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

## ~~D10 — LiveWebSocketSource resync: real concurrency, not an OS-buffer shortcut~~
**Relocated to `libs/data_source/source/docs/DECISIONS.md`** (the town-level "prod
streamer" doc — this is generic resync design, not root-scoped).

## ~~D11 — FileRecorder: local disk only in Phase 0, R2 sync deferred~~
**Relocated to `libs/data_source/sink/docs/DECISIONS.md`** (the "sinks" city doc).

## ~~D12 — Collector/recorder: shared wire format, retry-driven gap alerting, flush-based crash safety~~
**Split and relocated**: wire-format + crash-safety thirds to
`libs/data_source/sink/docs/DECISIONS.md`; gap-alerting third to
`libs/data_source/source/docs/DECISIONS.md` (originally
`protocol/docs/DECISIONS.md`, folded in when `protocol/websocket/`
flattened to `protocol/` and lost its own `docs/`).

## ~~D13 — Resync alignment was using spot's +1 rule, not futures'; found by live smoke-testing~~
**Relocated to `libs/data_source/source/docs/DECISIONS.md`** (the code it's about —
`resync.hpp` — lives there).

## ~~D14 — Resync snapshots are forwarded as a BookSnapshot event, not discarded after alignment~~
**Relocated to `libs/data_source/source/docs/DECISIONS.md`** (same file as D13 —
`resync_coordinator.hpp`).

## D15 — `libs/marketdata`/`libs/record` regrouped as `libs/data_source/{source,sink}`
Reverses the physical (not logical) side of the earlier "source/sinks are
coequal cities, not one combined data source" split: `MarketDataSource` and
`Sink` stay independent seams with independent consumers — that logic didn't
change — but the two now live as sibling directories under `libs/data_source/`
for discoverability, matching how the collector/live/backtest apps are all
"what does the data source connect to" questions. No C++ namespace or CMake
target renamed (`qp::marketdata`, `qp_marketdata_*`, `qp::record`,
`qp_record_*` all unchanged) — purely a directory move plus every path
reference (CMakeLists.txt, docs, `.vscode/`, skill tables) updated to match.
Verified with a full clean rebuild + `ctest` (7/7) + TSan on the two
concurrent integration suites, all unchanged from pre-move.

## D16 — Every lib's `include/` flattened; namespaces and CMake/test targets renamed to match, superseding D15's "unchanged"
D15 deliberately left C++ namespaces and CMake target names alone, moving
only directories. Follow-up push went further: no lib gets a `qp/`-wrapper
folder in its `include/` (every header sits directly in `include/`, included
by bare filename — `"types.hpp"`, `"wire.hpp"`, `"binance.hpp"`, ...), and
every namespace/target name that only matched the *old* `libs/marketdata`/
`libs/record` names now matches the *current* one instead: `qp::record` →
`qp::sink` (`qp_record*` → `qp_sink*`), town-level `libs/data_source/source`
content (`backoff`/`gap_detector`/`parser`/`resync`/`resync_coordinator`/
`source`/`venue_types`) moved from bare `qp` into `qp::source`
(`qp_marketdata_pure`/`qp_marketdata_tests`/`qp_marketdata_bench` →
`qp_source_*`), and the `protocol` village's own library (previously
confusingly named bare `qp_marketdata`, sharing no name with the town's
`_pure`/`_tests` targets) is now `qp_protocol`/`qp_protocol_test_support`/
`qp_protocol_integration_tests`. `venue` (`qp::venue::binance`, `qp_venue*`)
and `collector` (`qp::collector`, `qp_collector*`) already matched and are
unchanged; `core` (bare `qp`, `qp_core*`) is the deliberate exception — it's
the root/lingua-franca namespace, not a domain village. Every `.vscode/`
task/launch entry, the `/test` and `/bench` skill tables, and CMakeLists.txt
comments updated to match. Verified: full rebuild + `ctest`, 7/7 in both
debug and release, plus the combined `qp_bench` binary links clean.

## D17 — `MarketDataSource` concept renamed `Source`
Stuttered as `qp::source::MarketDataSource` once its content lived fully
under the `source` namespace/folder/`source.hpp` file — `Source` matches
this repo's convention of type name mirroring namespace/folder (`sink`'s
`Sink`). Repo-wide references updated (`CLAUDE.md`,
`docs/architecture-principles.md`, `docs/DESIGN.md`, `docs/repo-layout.md`,
and the code itself). Same pass also collapsed `libs/data_source/source`'s
`protocol/` village back into the town directly — town-scoped detail in
`libs/data_source/source/docs/DECISIONS.md` D18.

## D19 — `wire` split out of `sink` into its own lib; `source`/`sink` depend on it instead of on each other
`FileReplaySource`'s read side needs the same wire format/zstd codec/day-
segment naming `FileRecorder` already had, but `source` depending on `sink`
directly would violate "seams depend on core only, never each other."
`wire.hpp`/`zstd_stream.hpp`/`partition.hpp` moved into `libs/data_source/wire`
(`qp::wire`, `qp_wire` target) — core-like substrate scoped to
`data_source`'s two cities, not itself a city. `source` and `sink` both
depend downward on it now. `zstd_stream.hpp` gained `ZstdDecompressor`
(read-side mirror of `ZstdCompressor`) for `FileReplaySource` to use.

## D20 — `FileRecorder` writes `data_dir/symbols.manifest`; `FileReplaySource` reads it instead of taking its own symbol list
Passing `symbol_names` independently to both `FileRecorder` (write) and
`FileReplaySource` (read) meant nothing enforced the two orderings agreed —
a replay using a different order than record time would silently mislabel
every event's `SymbolId`. `FileRecorder` now writes a plain-text
`symbols.manifest` (one name per line, index == `SymbolId`) at
construction; `FileReplaySource`'s constructor is now `(data_dir,
first_day, last_day, wanted = nullopt)` and reads that file as the sole
canonical source of the mapping. `wanted` optionally filters to a subset
without renumbering `SymbolId` (still the manifest's global index); every
`wanted` name is validated against the manifest before any segment file is
touched, so a bad name fails immediately rather than after paying for
whatever other symbols were already read. No `wire` involvement — the
format (newline-delimited names) is too trivial to carry the drift risk
`write_event`/`read_event`'s binary encoding does.

## D22 — Engine's `Source` concept renamed `Transport`; `Sink` dropped from Engine's template params
`Source` collided with the `source`/`sink` `data_source` city names once an
adapter reading off a `Sink`'s fan-out ring also needed to satisfy it —
renamed the Engine-facing `next() -> optional<MarketEvent>` concept to
`Transport`; the two cities keep their own names. `Sink` also dropped from
`Engine<Transport, Clock, ExecutionGateway>`'s params — recording is
orthogonal to the decision loop, not something backtest should have to
satisfy with a no-op; live recording wraps the `Transport` or runs the
collector separately. Docs-only for now (`docs/architecture-principles.md`,
`docs/repo-layout.md`) — `libs/data_source/source/include/source.hpp`'s
`Source` concept keeps its current name until Engine/the ring-reading
adapter are actually built, to avoid doc/code drift in the meantime.

## D23 — `Clock` gets its own `libs/clock`, not folded into `core`
`docs/repo-layout.md` originally scoped "Clock concept" to `core`, but
every other seam in this repo keeps its concept and concrete adapters
together in one lib (`Source`/its adapters in `source`; `Sink`/its
adapters in `sink`) — splitting `Clock`'s concept into `core` while its
adapters lived elsewhere would be the odd one out. `core` stays scoped to
the zero-dep lingua franca + the SPSC queue; `libs/clock` holds `Clock` +
`SimClock`, depending on `core` only for `Timestamp` — same shape as `wire`
being a substrate scoped to `source`/`sink` rather than folded into either.
`WallClock` deliberately not built yet: a live wall clock that's safe
against `system_clock`'s non-monotonicity (NTP sync, manual changes) wants
an anchor-plus-`steady_clock` design, not a bare `system_clock::now()` —
designing that now, ahead of Phase 2's actual live requirements, is
guessing. `SimClock` is all Phase 1 (backtest) needs.

## D25 — `ExecutionGateway`/`Matcher` split; `SimExecution<M>` generic over fill sophistication now, not deferred to a second implementation
`SimExecution` and `LiveExecution` don't share a template: `SimExecution`'s
`submit()` computes inline, `LiveExecution`'s is inherently async (fires an
order, a background thread fills the outcome queue whenever the exchange
responds) — forcing both through one generic shell would wrap a synchronous
abstraction around an async reality. They're two independent
`ExecutionGateway` implementations, matching `LiveWebSocketSource`/
`FileReplaySource` under `Source`. Within `SimExecution<M>`, fill
sophistication (market-state tracking + fill computation) is templated on
one bundled `Matcher` policy now, with only one implementation
(`LastTradeMatcher`) — deliberately not deferred to "abstract on the third
implementation": unlike most seams in this repo, this one is being built as
a genuine compile-time-swappable policy from the start rather than
concretely-then-generalized, because getting the seam boundary right
matters here independent of whether a second `Matcher` exists yet. Named
`Matcher`, not `FillModel` — considered and rejected splitting market-state
tracking out into its own lib (mirroring `wire`'s D19 extraction, shared
substrate for two consumers instead of one depending on the other): `wire`
was extracted once `FileReplaySource` *concretely* needed the same codec
`FileRecorder` already had, not in anticipation of it. No second concrete
consumer of market-state tracking exists yet (a live feature engine is a
distant Phase 4 concern) — bundled and renamed is right-sized for now;
revisit the extraction if/when one is actually being built. `Fill`/`Reject`
come back through one poll, `next_outcome() -> optional<variant<Fill,
Reject>>`, not two separate queues — two channels would lose the true order
outcomes happened in (fill, reject, fill — draining one queue then the
other scrambles that). `Order`/`Fill`/`Reject`/`RejectReason`/`OrderId`/
`Notional` added to `core/types.hpp` (lingua-franca plain data, same tier as
`MarketEvent`, per `docs/repo-layout.md`'s original scoping of `core`).
`LastTradeMatcher`: no book (funding carry needs no depth,
`docs/strategy.md`), no slippage, no partials — full fill at the last-seen
`Trade` price, `NoPriceAvailable` reject if none seen yet.

## D26 — `Price`/`Qty` staying `double` for now is a tracked gap, not a settled decision
Both have the same exactness problem: Binance defines price and quantity in
fixed decimal increments per symbol (`tickSize`/`stepSize`), and `double`
drifts across repeated arithmetic (fee calc, PnL summation, `price * qty`
not matching the exchange's own integer arithmetic exactly). `Price`
already carried a fixed-point TODO; `Qty` silently didn't — same problem,
inconsistently flagged, now aligned. Not fixed here: `PriceLevel` is
`memcpy`'d directly into the `wire` binary format
(`is_trivially_copyable`), so changing either's representation touches the
on-disk format, the parser converting Binance's string-decimals into it,
and every already-tested record/replay path — a dedicated pass (real
representation choice: fixed-point scale, per-symbol precision from
`exchangeInfo`, etc.), not a rushed redefinition as a side effect of
unrelated work.

## D27 — Every seam we control gets static dispatch, not just source/clock/execution/recorder; D4's "virtual: strategy, risk" was wrong
D4 justified virtual dispatch on `Strategy`/`RiskGate` by "the strategy
seam is crossed rarely against a huge budget"
(`docs/architecture-principles.md`) — wrong. This project isn't opting out
of latency sensitivity; D1's "not HFT" rules out *hardware* we can't
afford (colo, kernel-bypass, FPGAs), it doesn't say we stop caring about
latency on the software we do control. A vtable indirection and a
`unique_ptr` allocation per strategy are both self-inflicted, avoidable
costs on the one part of the budget we actually own — "the budget is huge
so it doesn't matter" is the same optimism this repo's honest-cost
discipline (`SimExecution`, D25/D26) exists to reject elsewhere. `Strategy`
and `RiskGate` become C++20 concepts (like `Clock`/`ExecutionGateway`), not
virtual base classes. `Engine` dispatches `Strategy`s through a shared
`RoundRobinPool` (D33, `libs/core`) rather than a vtable/heap per strategy
— still one `RiskGate`/`Portfolio` shared across all of them (needed so a
kill-switch sees aggregate drawdown, not per-strategy). Non-goals unchanged
(D1/D2, `MISSION.md`): no market making, no latency arbitrage, no
colocated/specialized hardware, holding periods stay minutes-to-days —
this is about not wasting the latency we already have, not about entering
a race we can't win.

## D28 — `Intent`/`Portfolio`/`StateView` shapes; `Strategy`/`RiskGate` concept signatures; `Engine`'s member layout
`Intent` (`core/types.hpp`): a target position, not a delta or a venue
order ("be +2 BTC", not "buy 2 BTC") — `RiskGate` computes the delta itself
against current `StateView`. No `ts` field, matching `Order`'s own
precedent — timestamps are call-site parameters where actually consumed,
not struct fields. `Portfolio`/`StateView` (`core/portfolio.hpp`, not a new
`libs/portfolio`): unlike `Clock`/`ExecutionGateway`, `Portfolio` has no
backtest/live variant to swap between — exactly one implementation, ever —
so it isn't a compile-time-swappable seam, it's shared state;
`docs/repo-layout.md`'s tree comment already pre-declared it living in
`core`. `StateView` v1 exposes only `position(SymbolId) -> Qty`: no PnL, no
open-order tracking — every `Order` today fills or rejects synchronously
(`SimExecution`/`LastTradeMatcher`), so "open orders" would always be
empty, and PnL wants a real consumer (a real kill-switch `RiskGate`, or
analytics reporting — both still later milestones) before its shape (cost
basis? mark-to-market against which price?) is guessed at.

`RiskDecision` (`libs/risk`) is a tag (`RiskOutcome`) + `optional<Order>`,
not a `variant`: `Approved`/`Resized` share one payload shape (an `Order`)
— only `Rejected` differs — so a tag says *why*, not *what shape*, unlike
`Fill`/`Reject` which genuinely differ in fields. `OrderId` minting is each
concrete `RiskGate`'s own responsibility (a private counter), not part of
the interface. `strategy` stays bare namespace `qp` (one concept, no
supporting types, matching `Clock`'s precedent); `risk` gets `qp::risk`
(multiple types, matching `qp::execution`'s precedent).

`Engine::step()` calls `RiskGate::on_tick` once per processed event — the
only well-defined, non-speculative cadence available in an event-driven
core with no idle-time polling. `Strategy::on_timer` is deliberately wired
nowhere yet: inventing a timer-scheduling mechanism now, ahead of any real
caller, would be exactly the speculative machinery this repo's "seams
first, generality later" principle warns against. Scope, matching D25's
precedent: only the interfaces + `Engine` skeleton + a minimal passing loop
test land now — no concrete funding-carry `Strategy`, no real kill-switch
`RiskGate`, no `WallClock`, no `apps/backtest` wiring.

## D31 — `Portfolio` stores positions in a fixed `std::array<Qty, kMaxSymbols>`, direct-indexed by `SymbolId`, not an `unordered_map`
`SymbolId` is already a dense id ("index into venue symbol table",
`core/types.hpp`) assigned by the run's own `SymbolTable::intern()` from a
config/manifest-provided symbol list (`FileRecorder`'s `symbols.manifest`,
D20; the collector's CLI `--symbols`) — known and bounded *before* `Engine`
ever starts, not discovered mid-run. An `unordered_map` was hashing a key
space that was never sparse or unbounded to begin with; direct array
indexing is the structurally correct fit, not a premature optimization.
`kMaxSymbols = 64` is sized for a solo retail portfolio's own subscribed
set (funding-carry majors + stat-arb pairs, `docs/strategy.md`), not
Binance's full several-hundred-symbol catalog — a generous, cheap (512
bytes) bound, not a guess dressed up as one; bump it if a real strategy
set needs more. `position()`/`apply_fill()` assert `symbol < kMaxSymbols`
in debug builds — an out-of-range `SymbolId` here is an internal
config/venue mismatch, not user input to validate against (CLAUDE.md:
trust internal guarantees, validate only at system boundaries). No
allocation, no hashing, on either call — same D27 discipline applied to
`Portfolio`'s own hot path.

## D32 — `Fill`/`Reject`/`MarketEvent` field order changed to be alignment-driven, not declaration-order-by-accident
A structural review (prompted by D28's new `Portfolio`/`Intent` additions)
found the existing (pre-this-branch) `Fill`/`Reject`/`MarketEvent` structs
paying real, avoidable padding from small fields (`SymbolId`, `Side`/
`RejectReason`) being interleaved between 8-byte fields instead of grouped:
`Fill` 56 -> 48 bytes, `Reject` 32 -> 24 bytes, `MarketEvent` ~128 -> ~112
bytes, by grouping the sub-8-byte fields adjacently instead of scattering
them at their "logical" position. Safe to do: `wire.hpp` encodes
`MarketEvent` field-by-field (already asserted `!is_trivially_copyable`),
never `memcpy`s it, so reordering declaration order doesn't touch the
on-disk format — unlike `PriceLevel`, which *is* `memcpy`'d and so cannot
be reordered this freely. `Fill`/`Reject` gain
`static_assert(sizeof(...) == N)` guards matching `PriceLevel`'s existing
convention, to catch a future regression; `MarketEvent` doesn't (it holds
`std::vector`s — its size isn't a portable constant across standard
library implementations the way an all-fixed-width struct's is).
Call sites using designated initializers (C++20 requires designator order
to match declaration order) updated to match:
`libs/execution/include/last_trade_matcher.hpp`,
`libs/core/tests/test_portfolio.cpp`.

## D33 — `Engine` dispatches `Strategy`s across a generic `RoundRobinPool` (`libs/core`), not one thread per strategy
Cross-strategy ordering only matters once something downstream is
order-sensitive: a `RiskGate` with capacity shared across strategies
(an exposure cap, a kill-switch budget — whoever's checked first wins the
remaining room), or a book-depth-aware `Matcher` (D25's stated future
direction for `Matcher` — whoever fills first gets the better price, the
next order eats the slippage). Neither exists yet — `LastTradeMatcher`
fixes its price before any strategy runs, and no `RiskGate` shares state
across strategies — so today this can't change an outcome. It's cheap
insurance for when one does exist, not a fix for a live bug: `Engine`
risk-checks/submits each `Intent` **single-threaded, in fixed
strategy-index order**, never the order results happen to arrive in.

The first design (thread-per-`Strategy`, two `std::barrier`s) was wrong on
its own terms, independent of the ordering question: `sizeof...(Strategies)`
persistent OS threads double-barrier-synced on every event doesn't scale —
20 strategies means 20 threads, most idle, cycling wake/sleep every single
event. Replaced with `RoundRobinPool<NumWorkers, Context, Result, Tasks...>`
(`libs/core`): a **shared**, compile-time-sized pool (`NumWorkers`, an
explicit `Engine` template parameter — not defaulted, since a following
`Strategies...` pack means a default before it can't be omitted positionally
anyway; not queried from `hardware_concurrency()`, since everything else on
this path already resolves at compile time). Strategies are assigned
round-robin (`strategy i → worker i % NumWorkers`, resolved via `if
constexpr` over an index sequence — zero runtime branching for indices a
worker doesn't own), so `NumWorkers` can be smaller than the strategy count.

Built as a generic primitive in `core`, not inline in `Engine` — the
mechanics (round-robin assignment, wake, ordered collection) have nothing
to do with `MarketEvent`/`Strategy`; `core` already holds exactly this
category of thing (`SpscQueue`, `SpmcRing`). Wake/collect reuses
`SpscQueue` (one per task, producer = the owning worker, consumer = the
caller of `run_round()`) instead of a new `std::barrier`-based handshake:
draining queue `i` **is** both the wait for task `i` and, done strictly in
index order, the deterministic collection — one mechanism, not two. `Engine`
adapts each `Strategy`'s 2-arg `on_event(event, state)` to the pool's 1-arg
`Result operator()(const Context&)` shape via a small `StrategyTask<S>`
wrapper holding a pointer to the strategy (not a copy) — `Context` bundles
`{const MarketEvent&, StateView}`.

Verified outside the normal `ctest` path (per CLAUDE.md's build policy, no
`ctest`/`ctest`-suite run was triggered for this): a standalone compile
against the real headers — single-strategy, 2 strategies over 1 worker
(fewer workers than tasks), 2 strategies over 2 workers, and `run()` over
50 events — clean under ThreadSanitizer (ASLR disabled per
`docs/environment.md`'s documented WSL2 TSan workaround). `qp_core_tests`
gains `test_round_robin_pool.cpp` (pool mechanics in isolation, no
`Strategy`/`Engine` scaffolding needed); `qp_engine_tests` gains a
2-strategy/1-worker case proving `Engine`'s own commit loop aggregates both
strategies' fills correctly.

## D36 — `MarketEvent` gains `mark_price`; live source now subscribes to `@markPrice`
Funding-carry's return *is* the funding payment, and honestly modeling one
needs the mark price it settles against (Binance computes
`payment = position × mark_price × funding_rate` at each interval) — last-trade
price isn't it, and nothing carried mark price before this. `mark_price`
added to `MarketEvent` next to `funding_rate` (`core/types.hpp`) — both come
off Binance's single `markPriceUpdate` message, not two separate events, so
one `Funding`-kind event carries both, matching what the exchange actually
sends. `wire.hpp`'s fixed trailing field group grows by one `double`
(on-disk format change — no compat shim, nothing has recorded `Funding`
events yet since the parser never emitted them before this). Live source:
`build_stream_path` subscribes to `<symbol>@markPrice` per symbol (default
~3s cadence — carry's decision cadence is hours, no reason to reach for the
faster `@markPrice@1s` variant); `parse_message` gains a `markPriceUpdate`
branch, additive to the existing `depthUpdate`/`aggTrade` dispatch, no
resync/gap-detection involvement (not a sequenced diff stream). Backtest
replay needs no new mechanism — `FileReplaySource`'s existing per-symbol
timestamp merge already handles arbitrary event kinds; sourcing *historical*
mark price/funding rate (Binance's free `markPriceKlines`/`fundingRate` REST
history) is separate, not-yet-built tooling (`tools/`), not a replay-path
change.

Still not built: any P&L/cash-balance dimension to actually apply a computed
funding payment against — `Portfolio` only tracks position (D31/D28). That's
the next real gap for backtesting carry honestly, not this change.

## D37 — `Portfolio` gains a cash balance; still no margin/leverage/liquidation modeling
Fills already carry `price`/`qty`/`fee`; `apply_fill` now debits/credits
`cash_` alongside the existing position update (buying costs `qty*price +
fee`, selling credits `qty*price - fee`, same `delta` already computed for
the position, no new state needed to do it). Funding gets a second write
path, `apply_funding(const MarketEvent&)`: `cash_ -= position(symbol) *
mark_price * funding_rate` — positive funding means longs pay shorts, so a
positive (long) position debits, negative (short) credits. `Engine::step()`
calls it right after `exec_.on_market_event()`, *before* the strategy pool
runs on that same event — settlement uses the position as it stood before
any decision made *because of* this funding rate, not after.

Deliberately not modeled: margin, leverage, liquidation price. Those answer
"could this position get forced closed," a risk question for a future
`RiskGate`, not a backtest-P&L one — guessing at leverage/liquidation rules
now wouldn't change whether a strategy's PnL curve is honest, only add
unforced assumptions. Equity isn't stored either — `Portfolio` doesn't track
prices, so `cash + Σ(position_i * current_price_i)` is a derived quantity
for whoever has those prices (a future analytics layer), not this class.

## D44 — `Intent`/`Order`/`Fill`/`Reject` gain `venue`; `Portfolio` indexes by `(symbol, venue)`, not `symbol` alone
D43 fixed the `SymbolId` collision (two venues interning "BTCUSDT" to the
same id) at the `MarketEvent` layer only — `Intent`/`Order`/`Fill`/`Reject`
and `Portfolio::positions_` still keyed on bare `SymbolId`, so a carry
strategy's long-spot/short-perp BTCUSDT legs would still net into one
`Portfolio` slot, silently reading flat instead of delta-neutral. Found
before any `FundingCarryStrategy` code was written; fixed first.
`VenueId venue{}` added to all four structs, placed to consume existing
padding (`static_assert(sizeof(...) == N)` unchanged on each).
`Portfolio::positions_` becomes `std::array<Qty, kMaxSymbols * kMaxVenues>`
(extends D31's fixed-array reasoning to the composite key);
`StateView::position(symbol, venue)` takes `venue` with no default, same
convention as `AlignmentRule`/`BinanceMarket`. Caught a real bug in the
process: `LastTradeMatcher::last_price_` was keyed on `symbol` alone, so
two venues' trades for the same instrument would silently overwrite each
other's last-traded price — now keyed on `(symbol, venue)`. `cash_` stays
a single aggregate (no per-venue split) — same "no margin modeling yet"
boundary as D37, not a gap this pass closes.

## D46 — `Portfolio` gains `equity()`, fed by a new `apply_mark_price(MarketEvent)` write path
A real `RiskGate`'s kill-switch (CLAUDE.md: "autonomous authority —
kill-switch / drawdown flatten") needs equity to detect an actual drawdown;
`cash()` alone can't — buying an asset drops cash immediately without being
a loss, and a profitable open position is invisible to it. D37 explicitly
punted on this ("a derived quantity for whoever has those prices... not
this class") rather than guess. `apply_mark_price` marks `(symbol, venue)`
at whichever scalar price a `MarketEvent` actually carries — `Trade.price`
or `Funding.mark_price`; `BookDiff`/`BookSnapshot` have no single scalar
price and are ignored. `Engine::step()` calls it on every event, same
cadence as `apply_funding`. `equity() = cash() + Σ(position_i ×
mark_price_i)`, a plain `O(kMaxSymbols × kMaxVenues)` scan — not tracked
incrementally, since it's only read once per `step()` (`RiskGate::on_tick`),
far less often than the two write paths that would have to maintain a
running total. `mark_price_` starts at 0 like `positions_`/`cash_` — a
position held before its first `Trade`/`Funding` event understates
`equity()` until one arrives, same "no never-touched-vs-zero" tradeoff
already made elsewhere in this class.

## D49 — Roadmap restructured: analytics and research infra split out of the old Phase 1 into their own milestones
Old Phase 1 bundled "backtest mechanically works" with "walk-forward +
PnL/Sharpe/max-drawdown reporting" as one phase — understated how much
work the reporting side actually is, and gave no separate checkpoint for
it. `docs/roadmap.md` renumbered Phase 0-4 to Milestone 1-6: M1 (data tap +
backtester + first strategy + risk gate, done this session) absorbs old
Phase 0 plus old Phase 1's backtest mechanics plus the `RiskGate` slice
pulled forward from old Phase 2; M2 is analytics alone; M3 is research
infra (Python/pybind11 sweeps) + actual parameter tuning, now explicitly
gated on M2's analytics existing to score against; M4-M6 are old Phase
2-4, renumbered, contents unchanged.
