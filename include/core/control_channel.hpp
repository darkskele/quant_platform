#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <thread>

#include "mpsc_queue.hpp"
#include "spmc_ring.hpp"

namespace qp {

/// Closed, fixed vocabulary — deliberately not a template parameter.
/// ControlChannel is meant to be the one shared coordination primitive
/// every composition root wires up (Engine, a data-source driver thread,
/// anything else that needs to start/stop together), not something
/// reinstantiated per message type per use site.
enum class ControlCommand { Start, Stop };

/// Central handle a fixed, compile-time-known set of participants attach
/// to (same attach()-hands-out-an-index convention as FanoutSink) — by
/// reference/pointer, not shared_ptr: the owning composition root's `run()`
/// always joins every thread that could touch this before it goes out of
/// scope (same lifetime discipline as InProcessTransport's Ring&,
/// run_data_source's `const std::atomic<bool>&`), so there's no shared-
/// ownership need to guard against.
///
/// Two queues, one per direction: any attached participant may
/// request_stop() (multi-producer — Engine noticing it's drained every
/// event, a data-source thread noticing true end-of-data, a future SIGINT
/// handler, all racing to be first) into an MpscQueue; a dedicated pump
/// thread drains that inbox and broadcast()s each command out to every
/// attached participant over an SpmcRing, so whoever's polling (Engine's
/// loop, run_data_source's driver thread, ...) all see the identical
/// Start/Stop, however it originated. Gated broadcast (SpmcRing never
/// drops) — a Stop must never be lost the way a MarketEvent under
/// backpressure is allowed to be.
///
/// The pump thread exists because "whichever loop is already polling the
/// channel" isn't singular once more than one participant polls
/// concurrently (D5x: BacktestInProcessTransport/collector.cpp used to
/// each pump() themselves on the assumption they were the only such loop —
/// with several Engines each owning their own Transport, that's several
/// concurrent callers racing on MpscQueue's single-consumer try_pop(), a
/// real data race, not just redundant work). Owning the pump here instead
/// makes correctness independent of how many participants exist: exactly
/// one thread ever calls the MpscQueue's consumer side, structurally, no
/// matter how many Engines attach. kPumpInterval trades Stop-propagation
/// latency for that guarantee — control commands are rare and
/// administrative (Start once, Stop once, maybe a retry), not
/// latency-sensitive, so a coarse interval costs nothing anything here
/// actually needs.
template <std::size_t NumConsumers>
class ControlChannel {
   public:
    // Assigned in the body, not the init list: pump_thread_ starts running
    // pump_loop() immediately, so every member it touches (requests_,
    // broadcast_, stop_pumping_) must already be constructed first — true
    // here only because they're all declared earlier in the class and C++
    // constructs members in declaration order regardless of init-list
    // order, but spelled out explicitly (matching RoundRobinPool's own
    // spawn-in-body pattern) rather than leaned on implicitly.
    ControlChannel() {
        pump_thread_ = std::thread{[this] { pump_loop(); }};
    }

    // pump_thread_ captures `this` — a moved-to instance would leave it
    // pointing at the old address (same reasoning as RoundRobinPool).
    ControlChannel(const ControlChannel&)            = delete;
    ControlChannel& operator=(const ControlChannel&) = delete;
    ControlChannel(ControlChannel&&)                 = delete;
    ControlChannel& operator=(ControlChannel&&)      = delete;

    ~ControlChannel() {
        stop_pumping_.store(true, std::memory_order_relaxed);
        if (pump_thread_.joinable()) pump_thread_.join();
    }

    /// Hands out the next consumer id (0..NumConsumers-1) for a new
    /// participant to poll() with. No detach — the participant set is
    /// fixed for the channel's lifetime, same as FanoutSink/SpmcRing.
    /// SpmcRing itself has no attach() (its consumer indices are assigned
    /// externally by whoever wires it up) — this counter is exactly
    /// FanoutSink's own attach() logic, duplicated because ControlChannel
    /// wraps SpmcRing directly rather than a FanoutSink (there's no
    /// separate "record" call shape to reuse here, only broadcast/poll).
    std::size_t attach() {
        auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
        if (id >= NumConsumers) {
            throw std::out_of_range("ControlChannel: attach() exceeds NumConsumers");
        }
        return id;
    }

    /// Owner-side: force a command out to every attached participant right
    /// now, bypassing the request inbox (e.g. the initial Start at
    /// kickoff).
    void broadcast(ControlCommand cmd) { broadcast_.push(cmd); }

    /// Any attached participant, any thread: ask the owner to stop
    /// everyone. Returns false only if the request inbox is full (caller
    /// may retry — a single dropped request here just delays the eventual
    /// pump, it doesn't lose the shutdown, since the caller still holds
    /// its own "I want to stop" fact and can call again).
    bool request_stop() { return requests_.push(ControlCommand::Stop); }

    /// Participant-side: has a new command arrived for `consumer` since
    /// its last poll()?
    std::optional<ControlCommand> poll(std::size_t consumer) {
        return broadcast_.try_pop(consumer);
    }

    /// Compile-time-indexed sibling of poll(std::size_t).
    template <std::size_t Consumer>
    std::optional<ControlCommand> poll() {
        return broadcast_.template try_pop<Consumer>();
    }

   private:
    // Small, fixed capacities: control commands are rare (Start once,
    // Stop once, maybe a retry) — this is not a data-rate queue.
    static constexpr std::size_t kCapacity = 8;

    // How stale a request_stop() is allowed to sit in the inbox before the
    // pump thread rebroadcasts it — the actual Stop-propagation latency
    // this class now costs every participant, in exchange for a single
    // structurally-guaranteed pump caller instead of an ownership question
    // every caller had to get right.
    static constexpr std::chrono::milliseconds kPumpInterval{100};

    // Sleeps before pumping, not after: pump-then-sleep would race a
    // request_stop() call against this thread's first, immediate pump
    // (whichever gets there first — no bound, could be near-instant or up
    // to a full kPumpInterval depending on scheduling). Sleep-first makes
    // every request wait out a consistent ~kPumpInterval minimum instead,
    // regardless of when request_stop() lands relative to thread start.
    void pump_loop() {
        while (!stop_pumping_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(kPumpInterval);
            while (auto cmd = requests_.try_pop()) broadcast(*cmd);
        }
    }

    SpmcRing<ControlCommand, kCapacity, NumConsumers> broadcast_;
    MpscQueue<ControlCommand, kCapacity>              requests_;
    std::atomic<std::size_t>                          next_id_{0};
    std::atomic<bool>                                 stop_pumping_{false};
    std::thread                                       pump_thread_;
};

}  // namespace qp
