#pragma once
#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "binance.hpp"
#include "parser.hpp"
#include "resync_coordinator.hpp"
#include "resync_policy.hpp"
#include "spsc_queue.hpp"
#include "types.hpp"
#include "venue_types.hpp"

namespace qp::source {

// Live Source: connects to a combined depth-diff + aggTrade stream,
// normalizes each message to a MarketEvent on a dedicated I/O thread, and
// hands them to next() via an internal SPSC queue. Reconnects with
// exponential backoff on any drop.
//
// Templated on `Parser` (see parser.hpp) — the wire-protocol glue (message
// parsing, stream-URL construction, snapshot fetch/parse) is the only part
// that varies by venue; the transport (this file) and the resync/gap-
// detection state machine (ResyncCoordinator, resync.hpp) are already
// venue-agnostic in their own signatures and were never Binance-specific to
// begin with — see DESIGN.md G2. `venue::binance::BinanceParser` is the
// sole implementation today (LiveWebSocketSource, declared below, is a
// type alias to exactly that specialization — every existing call site
// keeps working unchanged).
//
// Thread picture: three threads touch an instance, not two. io_thread_ and
// resync_thread_ are the two D10 talks about — the ones this class owns and
// spawns. The third is implicit and not owned by this class at all: whatever
// thread calls next() is queue_'s one and only consumer (io_thread_ is its
// one producer). That's not a violation of anything, it's SpscQueue's
// contract doing its job — but it does mean next() must only ever be called
// from a single, consistent thread for the lifetime of this object. The
// metric accessors (dropped_count() etc.) are plain atomic loads and are
// fine to call from any thread, any number of them, concurrently.
//
// Templated on Rule (resync_policy.hpp, D13) in addition to Parser — the
// resync alignment check differs by market. No default: every instantiation
// names its rule explicitly, so a new venue can't silently inherit whatever
// rule happened to be default (the exact class of mistake D13 already was).
//
// Resync (docs/decisions.md D10): a fresh connection or a detected sequence
// gap puts that symbol into Buffering — its diffs are held in a small
// per-symbol ring rather than forwarded, while a *second* thread fetches a
// REST depth snapshot and the venue's align rule (find_resync_point, see
// resync.hpp) finds where to resume. Real concurrency (two threads, two
// SpscQueue channels), not an "immediately block on the REST call and hope
// the OS TCP buffer covers the gap" shortcut — see D10 for why. Per-symbol:
// one symbol resyncing doesn't pause the others sharing this connection.
//
// Seam note: next() is non-blocking and returns nullopt when the queue is
// simply empty right now, NOT when the stream has ended (there is no "end"
// for a live source). This differs from FileReplaySource, where nullopt
// means end-of-file. Both satisfy Source syntactically; a caller that
// treats nullopt as "done" is only correct for replay.
template <Parser P, AlignmentRule Rule>
class GenericLiveWebSocketSource {
   public:
    explicit GenericLiveWebSocketSource(
        std::vector<std::string> symbols,
        WsEndpoint               ws_endpoint   = venue::binance::kFuturesWsProduction,
        RestEndpoint             rest_endpoint = venue::binance::kFuturesRestProduction);
    ~GenericLiveWebSocketSource();

    // Rule of 5, explicitly: not just uncopyable but unmovable too, and both
    // deleted rather than left implicit. Two independent reasons a move
    // could never be safe here even if written: queue_ (and the resync
    // queues) hold std::atomic members (neither copyable nor movable —
    // moving an atomic while another thread might be touching it isn't
    // meaningful), and both threads' lambdas capture `this` — a moved-to
    // object would leave them running against a stale address regardless.
    GenericLiveWebSocketSource(const GenericLiveWebSocketSource&)            = delete;
    GenericLiveWebSocketSource& operator=(const GenericLiveWebSocketSource&) = delete;
    GenericLiveWebSocketSource(GenericLiveWebSocketSource&&)                 = delete;
    GenericLiveWebSocketSource& operator=(GenericLiveWebSocketSource&&)      = delete;

    // SPSC: must only ever be called from a single, consistent thread for
    // this object's lifetime — that thread is queue_'s one consumer,
    // io_thread_ is its one producer. See the class comment.
    std::optional<MarketEvent> next();

    // SymbolId -> name for every subscribed symbol, index == SymbolId. Safe
    // to call from any thread, any time after construction — see the
    // constructor: the full symbol set is pre-interned before either thread
    // starts, so symbol_table_ is logically immutable from that point on,
    // despite being otherwise I/O-thread-only.
    const std::vector<std::string>& symbol_names() const noexcept;

    // Events dropped because the output queue was full (consumer too slow).
    std::size_t dropped_count() const noexcept;

    // Sequence gaps detected since start (each one triggers a resync).
    std::size_t gap_count() const noexcept;

    // Resyncs started (initial-connect + gap-triggered + retries all count).
    std::size_t resync_count() const noexcept;

    // Resyncs that had to restart: buffer overflowed before the snapshot
    // arrived, the snapshot didn't cover what's buffered (a hole), or the
    // REST fetch itself failed. Non-zero here under normal load is a real
    // signal — either the resync thread is falling behind or the buffer
    // (ResyncCoordinator::kBufferCapacity) is undersized for how long fetches are taking.
    std::size_t resync_retry_count() const noexcept;

    // resync_retry_count() broken down by which of the three distinct
    // failure modes actually happened — the aggregate alone can't tell you
    // whether the REST fetch itself is failing, the buffer is overflowing
    // before a snapshot arrives (round trip too slow vs. arrival rate), or
    // snapshots are arriving but never bracketing what's buffered (stale
    // relative to it, or a hole). Each is a different problem with a
    // different fix.
    std::size_t resync_rest_failure_count() const noexcept;
    std::size_t resync_buffer_overflow_count() const noexcept;
    std::size_t resync_no_alignment_count() const noexcept;

    // Times a resync request couldn't be enqueued because resync_requests_
    // was full. Should be structurally ~impossible given its sizing
    // (kMaxSymbols requests can never all be in flight at once in practice)
    // — non-zero means the resync thread is stuck, not just slow.
    std::size_t resync_request_dropped_count() const noexcept;

    // Max observed depth of the output queue since start. Approaching
    // SpscQueue's capacity is the early warning for "the consumer of
    // next() isn't keeping up" — before it starts actually dropping.
    std::size_t output_queue_high_water_mark() const noexcept;

    // Max observed depth of the I/O-thread -> resync-thread request queue.
    // This one *shouldn't* ever be more than a few — it only grows past
    // "a handful" if the resync thread itself is falling behind (slow/stuck
    // REST calls), a different failure mode than the output queue backing up.
    std::size_t resync_request_queue_high_water_mark() const noexcept;

   private:
    void run();         // WS I/O thread body: connect, read loop, reconnect-with-backoff
    void resync_run();  // REST resync thread body: serve requests, sample queue depths while idle

    void on_message(std::string_view msg);
    void begin_resync(SymbolId symbol);
    void drain_resync_responses();
    void sample_queue_depths();

    static constexpr std::size_t kMaxSymbols = ResyncCoordinator<Rule>::kMaxSymbols;

    // Sized for jitter, not outages. This queue's actual job is a short-term
    // handoff to a sink that's continuously draining it (next() called in a
    // tight loop, per docs/architecture-principles.md's Sink concept) — not
    // to stand in for one that isn't. A truly stalled/absent consumer is an
    // operational failure that dropped_count()/output_queue_high_water_mark()
    // should surface quickly, not something a bigger buffer should quietly
    // paper over for tens of seconds. 2048 slots covers ordinary scheduling
    // jitter plus a plausible startup burst (several symbols' resync buffers
    // — up to ResyncCoordinator::kBufferCapacity each — replaying at once) at the ~600
    // msgs/sec peak estimated below (~10 symbols x (10 depth-diffs/sec
    // @100ms + ~50 aggTrades/sec in a volatile burst)) — about 3.4s of
    // continuous peak load, comfortably past normal jitter without implying
    // it's fine for a consumer to be gone for a minute-plus.
    static constexpr std::size_t kQueueCapacity = 1 << 11;  // power of 2

    static constexpr std::size_t kResyncChannelCapacity = 128;  // power of 2, > kMaxSymbols

    struct ResyncRequest {
        SymbolId    symbol;
        std::string symbol_name;  // captured on the I/O thread: the resync
                                  // thread must never touch symbol_table_,
                                  // which is documented I/O-thread-only
    };

    static_assert(std::is_standard_layout_v<ResyncRequest>);
    static_assert(
        !std::is_trivially_copyable_v<ResyncRequest>);  // owns std::string; SpscQueue
                                                        // moves/destroys via construct_at/
                                                        // destroy_at, never memcpy — same
                                                        // contract as MarketEvent

    struct ResyncResult {
        SymbolId                     symbol;
        std::optional<DepthSnapshot> snapshot;  // nullopt = fetch failed
    };

    static_assert(std::is_standard_layout_v<ResyncResult>);
    static_assert(!std::is_trivially_copyable_v<ResyncResult>);  // owns DepthSnapshot's vectors,
                                                                 // same reasoning

    std::vector<std::string> symbols_;
    WsEndpoint               ws_endpoint_;
    RestEndpoint             rest_endpoint_;
    SymbolTable              symbol_table_;  // I/O-thread only

    // Pure buffering/gap/resync-alignment state machine, extracted so it can
    // be driven and proven correct directly (tests/test_resync_coordinator.cpp)
    // rather than only through real sockets and real thread timing. I/O-thread
    // only — same as symbol_table_.
    ResyncCoordinator<Rule> coordinator_;

    // Embedded, not heap-allocated: at sizeof(MarketEvent) == 128 and
    // kQueueCapacity == 2048, this is 256KB — a real but small fraction of
    // a typical 8MB stack budget, unlike the original 1<<16 (8MB) capacity
    // that forced this behind a unique_ptr in the first place (embedding an
    // 8MB queue made LiveWebSocketSource itself an 8MB object — an
    // immediate stack overflow the moment anyone declared one as a local,
    // the normal way to use it). Once kQueueCapacity was sized from an
    // actual load estimate instead of picked arbitrarily, the indirection
    // stopped earning its cost — one less allocation, no pointer hop on
    // every push()/pop().
    SpscQueue<MarketEvent, kQueueCapacity>           queue_;
    SpscQueue<ResyncRequest, kResyncChannelCapacity> resync_requests_;   // I/O -> resync thread
    SpscQueue<ResyncResult, kResyncChannelCapacity>  resync_responses_;  // resync -> I/O thread

    std::atomic<bool>        running_{true};
    std::atomic<std::size_t> dropped_{0};
    std::atomic<std::size_t> gaps_{0};
    std::atomic<std::size_t> resync_count_{0};
    std::atomic<std::size_t> resync_retry_count_{0};
    std::atomic<std::size_t> resync_rest_failure_count_{0};
    std::atomic<std::size_t> resync_buffer_overflow_count_{0};
    std::atomic<std::size_t> resync_no_alignment_count_{0};
    std::atomic<std::size_t> resync_request_dropped_{0};
    std::atomic<std::size_t> output_queue_high_water_{0};
    std::atomic<std::size_t> resync_request_queue_high_water_{0};

    std::thread io_thread_;
    std::thread resync_thread_;
};

// Compiled once, in live_websocket_source.cpp, via explicit instantiation — not
// header-only. Every existing call site says qp::source::LiveWebSocketSource;
// only code that ever wants a *different* Parser/Rule (e.g. a spot venue)
// would need to spell out GenericLiveWebSocketSource<OtherParser, OtherRule>.
extern template class GenericLiveWebSocketSource<venue::binance::BinanceParser, FuturesAlignment>;
using LiveWebSocketSource = GenericLiveWebSocketSource<venue::binance::BinanceParser, FuturesAlignment>;

}  // namespace qp::source
