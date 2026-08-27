#pragma once
#include <atomic>
#include <cstddef>
#include <optional>
#include <stdexcept>

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
/// handler, all racing to be first) into an MpscQueue; the owner's pump()
/// drains that inbox and broadcast()s the same command out to every
/// attached participant over an SpmcRing, so whoever's polling
/// (Engine's loop, run_data_source's driver thread, ...) all see the
/// identical Start/Stop, however it originated. Gated broadcast (SpmcRing
/// never drops) — a Stop must never be lost the way a MarketEvent under
/// backpressure is allowed to be.
template <std::size_t NumConsumers>
class ControlChannel {
   public:
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
    /// pump(), it doesn't lose the shutdown, since the caller still holds
    /// its own "I want to stop" fact and can call again).
    bool request_stop() { return requests_.push(ControlCommand::Stop); }

    /// Owner-side: drains whatever's been requested since the last call
    /// and broadcasts each out. The owner calls this periodically from
    /// whichever loop is already polling the channel (Engine's own driving
    /// loop is the natural place) — ControlChannel spawns no thread of its
    /// own.
    void pump() {
        while (auto cmd = requests_.try_pop()) broadcast(*cmd);
    }

    /// Participant-side: has a new command arrived for `consumer` since
    /// its last poll()?
    std::optional<ControlCommand> poll(std::size_t consumer) { return broadcast_.try_pop(consumer); }

    /// Compile-time-indexed sibling of poll(std::size_t).
    template <std::size_t Consumer>
    std::optional<ControlCommand> poll() {
        return broadcast_.template try_pop<Consumer>();
    }

   private:
    // Small, fixed capacities: control commands are rare (Start once,
    // Stop once, maybe a retry) — this is not a data-rate queue.
    static constexpr std::size_t kCapacity = 8;

    SpmcRing<ControlCommand, kCapacity, NumConsumers> broadcast_;
    MpscQueue<ControlCommand, kCapacity>              requests_;
    std::atomic<std::size_t>                          next_id_{0};
};

}  // namespace qp
