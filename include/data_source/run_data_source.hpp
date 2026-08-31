#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

#include "sink.hpp"
#include "source.hpp"
#include "types.hpp"

namespace qp::data_source {

namespace detail {

// Fold-expansion over Is...: for each i, reads sources[i] and
// stamps event.venue = i and writes it via sinks[i].record(event). 
// The *same* compile-time i drives all three (source read, venue 
// stamp, sink write) because the compiler generates it from one fold
//  expansion. 
template <class SourceTup, class SinkTup, std::size_t... Is>
bool poll_round(SourceTup& sources, SinkTup& sinks, std::index_sequence<Is...>) {
    bool any = false;
    (([&] {
         if (auto ev = std::get<Is>(sources).next()) {
             std::visit([](auto& e) { e.venue = static_cast<VenueId>(Is); }, *ev);
             std::get<Is>(sinks).record(std::move(*ev));
             any = true;
         }
     }()),
     ...);
    return any;
}

}  // namespace detail

/// Drives N sources into N sinks, paired positionally one poll round per loop 
// iteration, idle-sleeping only once every source came back empty.
template <source::Source... Sources, sink::Sink... Sinks>
void run_data_source(std::tuple<Sources...>& sources, std::tuple<Sinks...>& sinks,
                     const std::atomic<bool>&  running,
                     std::chrono::milliseconds idle_sleep = std::chrono::milliseconds(10)) {
    static_assert(sizeof...(Sources) == sizeof...(Sinks),
                  "run_data_source pairs sources and sinks 1:1 by position");
    while (running.load(std::memory_order_acquire)) {
        bool any = detail::poll_round(sources, sinks, std::index_sequence_for<Sources...>{});
        if (!any) std::this_thread::sleep_for(idle_sleep);
    }
}

}  // namespace qp::data_source
