#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

#include "control_channel.hpp"
#include "sink.hpp"
#include "source.hpp"
#include "types.hpp"

namespace qp::data_source {

namespace detail {

// Fold-expansion over Is...: for each not-yet-done source i, delivers its
// pending event via sinks[i].record().
// A source's Eof marks that leg done; NoData just means nothing this round.
template <class SourceTup, class SinkTup, std::size_t N, std::size_t... Is>
bool poll_round(SourceTup& sources, SinkTup& sinks,
                std::array<std::optional<MarketEvent>, N>& pending, std::array<bool, N>& done,
                std::index_sequence<Is...>) {
    bool any = false;
    (([&] {
         if (done[Is]) return;
         auto& slot = pending[Is];
         if (slot) {
             if (std::get<Is>(sinks).record(std::move(*slot))) {
                 slot.reset();
                 any = true;
             }
             return;
         }
         source::PullResult pulled = std::get<Is>(sources).next();
         if (!pulled) {
             if (pulled.error() == source::SourceStatus::Eof) done[Is] = true;
             return;  // NoData or Eof -> nothing to deliver this round
         }
         std::visit([](auto& e) { e.market = static_cast<MarketId>(Is); }, *pulled);
         if (std::get<Is>(sinks).record(std::move(*pulled)))
             any = true;
         else
             slot = std::move(*pulled);  // sink full -> stage the untouched event for retry
     }()),
     ...);
    return any;
}

template <std::size_t N, std::size_t... Is>
bool all_done(const std::array<bool, N>& done, std::index_sequence<Is...>) {
    return (done[Is] && ...);
}

}  // namespace detail

/// Drives N sources into N sinks, paired positionally, one poll round per
/// loop iteration, idle-sleeping only once a round delivers nothing and
/// pulls nothing new.
template <std::size_t NumControlConsumers, source::Source... Sources, sink::Sink... Sinks>
void run_data_source(std::tuple<Sources...>& sources, std::tuple<Sinks...>& sinks,
                     ControlChannel<NumControlConsumers>& control, std::size_t control_consumer,
                     std::chrono::milliseconds idle_sleep      = std::chrono::milliseconds(10),
                     std::size_t               stop_poll_every = 64) {
    static_assert(sizeof...(Sources) == sizeof...(Sinks),
                  "run_data_source pairs sources and sinks 1:1 by position");
    constexpr auto seq = std::index_sequence_for<Sources...>{};

    std::array<std::optional<MarketEvent>, sizeof...(Sources)> pending{};
    std::array<bool, sizeof...(Sources)>                       done{};
    for (std::size_t round = 0;; ++round) {
        bool any = detail::poll_round(sources, sinks, pending, done, seq);
        if (detail::all_done(done, seq)) return;

        if (round % stop_poll_every == 0 && control.poll(control_consumer) == ControlCommand::Stop)
            return;
        // Yield, not sleep, when idle_sleep is zero: a backtest replay wants to
        // blast through, not pace itself off a wall-clock delay.
        if (!any) {
            if (idle_sleep == std::chrono::milliseconds::zero())
                std::this_thread::yield();
            else
                std::this_thread::sleep_for(idle_sleep);
        }
    }
}

}  // namespace qp::data_source
