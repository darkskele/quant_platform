# Benchmark methodology

## Framework

Stock Google Benchmark only — no custom per-iteration timing/percentile
sampler. A prototyped one was tried and rejected: on this dev box (WSL2),
per-iteration `chrono` calls cost more than the sub-10ns operations being
measured, and the tail it did surface turned out to be Hyper-V scheduling
noise, not the algorithm. See `docs/decisions.md` D51.

## Reaching private internals: public API only, no `friend`

Sometimes the thing worth measuring is private, and the public API only
reaches it indirectly. `FileRecorder::record()` just enqueues onto an
internal `SpscQueue` — the real cost (encode + zstd compress + `fwrite`)
happens in the private `handle_event()`, on a separate writer thread.
Benchmarking `record()` alone measures the enqueue, not the I/O (and at
back-to-back call rates, most calls just get dropped before reaching the
writer thread at all — see `bench_file_recorder.cpp`'s `BM_FileRecorder_Record`
comment).

A narrow, named `friend` grant (the `FRIEND_TEST`-style pattern GTest
uses) was tried here and rejected: it puts a benchmark-only declaration in
a production header, coupling the header's contents to which private
method a benchmark happens to want. Rejected in favor of measuring
through the public API only — `bench_file_recorder.cpp`'s
`BM_FileRecorder_RecordAndDrain` gets the real end-to-end cost (encode +
compress + write, not just enqueue) by letting the destructor's drain loop
run *inside* the timed region instead: construct, push N events, let the
destructor block until the writer thread has actually processed all of
them. No production header needs to know a benchmark exists. Where the
public API genuinely can't reach the thing worth measuring at all (not
just indirectly, but not-at-all), that's a sign that either the benchmark
belongs at a different scope, or the abstraction reaching it needs a real
seam — not a `friend` shortcut.

## Three tiers, applied where they make sense

1. **Isolation** — each operation benchmarked alone (e.g. `push` alone,
   `pop` alone). For an op that mutates shared internal state (a queue/ring
   filling or draining), the loop periodically resets that state via
   `state.PauseTiming()`/`ResumeTiming()` — **batched** (e.g. once every
   ~1000 iterations when a bounded buffer would otherwise fill/empty), not
   every iteration. `PauseTiming`/`ResumeTiming` are themselves slow
   (measured: ~270ns/call on this box, a real syscall) — calling them every
   iteration doesn't isolate the op, it measures timer-restart overhead
   leaking across the boundary instead. Batching amortizes that cost to a
   negligible fraction per operation.
2. **Tandem** — both operations together, one thread, no real concurrency.
   The plain round-trip cost. Isolation and tandem can disagree
   surprisingly: `SpscQueue`'s `pop()` alone measured ~7x the cost of
   `push()` alone, a real asymmetry (`std::optional<T>` move-construct +
   `destroy_at` vs. a plain `construct_at`) the combined round-trip number
   alone would have hidden.
3. **Contention** — only for interfaces actually built to be shared across
   real threads. Real OS threads (Google Benchmark's `->Threads(N)`) share
   *one* instance via a `static` local, with `state.thread_index()`
   assigning roles (producer/consumer). This is different from wrapping a
   single-threaded benchmark in `->Threads(N)` without a shared `static` —
   that gives every thread its own private instance, which only measures
   cache/memory-bandwidth pressure from unrelated concurrent work, not real
   contention on a shared instance.

   **Caution, from a real deadlock hit building this**: every thread in a
   `->Threads(N)` group runs the *same* iteration count per repetition.
   For a 1:1 producer:consumer shape (`SpscQueue`) or a broadcast
   producer:N-consumers shape (`SpmcRing`, where every consumer
   independently sees every item), that's naturally balanced. For an
   N-producers:1-consumer shape (`MpscQueue`), it is **not** —
   `N` producer threads each pushing once per iteration outproduces a
   consumer doing one `try_pop()` per iteration by a factor of `N`. Once
   the queue fills, the consumer finishes its iteration count and blocks at
   Google Benchmark's loop-exit barrier (which waits for every thread),
   while the producers' remaining iterations block forever on a full queue
   nobody is draining. The fix is for the consumer to do `N` pops per
   iteration, not 1 — see `bench_mpsc_queue.cpp`'s
   `BM_Mpsc_PushPopContended`. If a new contended benchmark hangs, this
   supply/demand mismatch is the first thing to check.

Not every interface gets all three tiers. Pure functions with no shared
mutable state (the wire codec, the Binance parser) and single-threaded-by-
design components (`FileReplaySource`, `FundingCarryStrategy::on_event`,
`BasicRiskGate::check`) get isolation only — there's no second op to pair
into a tandem number, and no genuine shared-instance concurrency exists for
them in production (a `Strategy`/`RiskGate` instance is never called from
two threads at once — D33's `RoundRobinPool` assigns each strategy
instance to one fixed worker; `RoundRobinPool`'s own dispatch is where
their real concurrency cost lives, and that's benched directly in
`bench_round_robin_pool.cpp` and `bench_engine.cpp`).

## Running it

`./build/release/qp_bench` — no flags needed. `benchmarks/qp_bench_main.cpp`
provides its own `main()` (not Google Benchmark's stock `benchmark_main`)
that bakes in this doc's variance recipe as defaults and regenerates the
checked-in root `BENCHMARKS.md` on every run via `tools/bench/bench_to_md.py`
— one launch, no separate conversion step. `--benchmark_filter=...` (or any
other Google Benchmark flag) still works normally and still regenerates the
MD file, just for whatever subset ran. Per-lib `*_bench` targets
(`.claude/skills/bench/SKILL.md`'s table) stay stock `benchmark_main` for
fast targeted iteration during dev — the defaults/MD-regeneration behavior
is specific to the one aggregate binary meant to be run standalone.

## Variance

`--benchmark_repetitions=5 --benchmark_report_aggregates_only=true`
— Google Benchmark's own mean/median/stddev/cv across 5 independent runs,
not a custom stat. 5, not higher: the inner loop already runs enough
iterations to average out ordinary noise; repetitions exist to catch
run-to-run variance (thermal state, scheduler mood), and standard error
only shrinks with `1/√N` — going 10→100 reps measured *worse* cv on a
sub-nanosecond op (D51), not better. Diminishing returns kick in fast.

`cv` (coefficient of variation) = `stddev / mean`, as a percentage — how
noisy a result is relative to its own size, comparable across benchmarks of
very different magnitudes.

## Naming, and output

Every benchmark function is named `BM_<Class>_<Description>` — e.g.
`BM_Spsc_PushInt`, `BM_Wire_WriteRealBookDiff`,
`BM_BasicRiskGate_ApprovesWhenFlat`. This isn't just a style preference:
`tools/bench/bench_to_md.py` groups the
rendered table into one section per class by parsing the text between
`BM_` and the first underscore, since Google Benchmark's JSON carries no
source-file or category field to group by otherwise. A benchmark that
skips the underscore becomes its own single-row "Other" section rather
than silently vanishing.

This convention also closes a real bug: three files each once had their
own `BM_PushInt` (SpscQueue's, SpmcRing's, MpscQueue's) — same display
name, different `family_index`. Grouping by name alone silently merged
them into one row, dropping two of the three. Every name is unique
repo-wide now, and the script additionally groups by `family_index` (not
name) and warns on stderr if a future name collision ever recurs.

`tools/bench/bench_to_md.py <report.json> --title "..." -o BENCHMARKS.md`
renders a `--benchmark_out_format=json` report as one Markdown row per
benchmark (mean/CPU as the headline columns, median/stddev/cv alongside),
pivoting Google Benchmark's separate mean/median/stddev/cv JSON entries
into columns rather than separate rows — grouped into a `### ClassName`
section per class, in first-seen order.

## What's deliberately not here yet

**p99/tail latency.** Real per-op latency percentiles need per-iteration
timestamping, which this repo tried and rejected (D51) — not because the
idea is wrong, but because WSL2 makes it non-deterministic here: clock-call
overhead swamps sub-100ns ops, and the tail is dominated by Hyper-V
scheduling noise invisible to the guest's own scheduler accounting (0
involuntary context switches recorded alongside a 0.7ms outlier). Once
benchmarking moves to the bare-metal VM (`docs/environment.md`'s documented
dev path), that noise floor drops out and per-op percentiles become worth
attempting again. No point chasing a number the environment can't measure
deterministically.

**Contention benchmarks for every transport wrapper.** `SpscQueue`,
`SpmcRing`, `MpscQueue`, and `RoundRobinPool` (the primitives literally
designed to be shared across threads) all have real contention
benchmarks. `engine/transport`'s `BacktestInProcessTransport` (two rings
merged in timestamp order, fed by two producer threads plus a
`ControlChannel` in production) does not yet — it's isolation+tandem only.
Given the producer:consumer balance bug above, a contended version of a
two-ring merge deserves its own careful pass, not a rushed add-on.

**`apps/backtest` and `apps/collector`: no benchmarks, by design.** Both are
thin composition wiring — a loop over already-benched components, not a
thing with its own behavior to measure. `bench_backtest.cpp`/
`bench_collector.cpp` existed at one point (real, working end-to-end
benchmarks) and were deleted: a full-pipeline number is just a slower,
noisier re-measurement of the same components' costs bundled together (see
`docs/decisions.md` D53). If a component's own row looks fine but the app
is slow in practice, that's a composition/scheduling question, not
something a bundled bench would have caught either.

**`GenericLiveWebSocketSource::on_message()`, benched for real.**
`on_message()` is private, and the class only ever calls it from its own
`io_thread_` reading a real (or, per `tests/support/mock_binance_server.hpp`,
locally mocked) socket — there's no way to reach it through the public API
without standing up that mock server inside a benchmark harness. A first
attempt (`bench_live_websocket_source.cpp`) did exactly that — stood up a
fresh `MockWsServer`/`MockHttpServer` pair inside every timed iteration —
and was deleted (D53): the number it produced was dominated by server
startup/socket/handshake cost, not the parse/gap-check/queue-push logic
actually worth measuring. A real version needs a throughput-style harness
(one long-lived mock server, wall-clock time over N messages) rather than
GBench's per-iteration model — a redesign, not a patch on the existing
shape, and worth its own pass rather than a rushed fit.

## Coverage

Every concrete class/free-function with real behavior has at least an
isolation-tier benchmark, one file per public interface, mirroring the
source tree (`benchmarks/<path>/bench_<name>.cpp` next to
`include/<path>/<name>.hpp`). Pure `concept`s (`Source`, `Sink`, `Clock`,
`ExecutionGateway`, `Matcher`, `RiskGate`, `Strategy`, `Parser`,
`AlignmentRule`'s own concept declaration, `Transport`) have nothing of
their own to benchmark — only concrete implementers do, and those are
covered individually. `SimClock` is the one deliberate exception worth
naming: `sim_clock.hpp` and `cmake/clock/CMakeLists.txt` both document
`now()`/`advance()` as "nothing to benchmark" (a single scalar load/store,
inlines away entirely) — `bench_sim_clock.cpp` exists anyway, for suite-
wide consistency, and empirically confirms that judgment (0.16ns) rather
than overriding it.

`data_source/transport` (a separate, older `Transport`/`InProcessTransport`
scaffold, predating the current `engine/transport` one) has no benchmark on
purpose — it's dead code, not wired into the build at all
(`cmake/data_source/CMakeLists.txt` never `add_subdirectory`s it), flagged
separately for cleanup rather than benchmarked as if it were live.
