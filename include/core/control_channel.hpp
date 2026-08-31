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

/// The one shared start/stop signal every composition root wires its
/// participants to.
enum class ControlCommand { Start, Stop };

/// Coordinates start/stop across a fixed, compile-time-known set of
/// participants. Any attached participant may request_stop(); a dedicated
/// pump thread drains those requests and broadcasts Start/Stop to
/// everyone polling, so every participant sees the identical command.
template <std::size_t NumConsumers>
class ControlChannel {
   public:
    ControlChannel() {
        pump_thread_ = std::thread{[this] { pump_loop(); }};
    }

    // pump_thread_ captures `this` — a moved-to instance would leave it
    // pointing at the old address.
    ControlChannel(const ControlChannel&)            = delete;
    ControlChannel& operator=(const ControlChannel&) = delete;
    ControlChannel(ControlChannel&&)                 = delete;
    ControlChannel& operator=(ControlChannel&&)      = delete;

    ~ControlChannel() {
        stop_pumping_.store(true, std::memory_order_relaxed);
        if (pump_thread_.joinable()) pump_thread_.join();
    }

    /// Hands out the next consumer id for a new participant to poll()
    /// with.
    std::size_t attach() {
        auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
        if (id >= NumConsumers) {
            throw std::out_of_range("ControlChannel: attach() exceeds NumConsumers");
        }
        return id;
    }

    /// Owner-side: force a command out to every attached participant now,
    /// bypassing the request inbox.
    void broadcast(ControlCommand cmd) { broadcast_.push(cmd); }

    /// Any attached participant, any thread: ask the owner to stop
    /// everyone. False only if the request inbox is full — safe to retry.
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
    // Control commands are rare (Start once, Stop once, maybe a retry) —
    // not a data-rate queue.
    static constexpr std::size_t kCapacity = 8;

    // Stop-propagation latency this costs every participant.
    static constexpr std::chrono::milliseconds kPumpInterval{100};

    // Sleeps before pumping, not after, so every request waits a
    // consistent ~kPumpInterval regardless of when it lands.
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
