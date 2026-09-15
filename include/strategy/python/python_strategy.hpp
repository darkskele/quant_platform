#pragma once
#include <pybind11/pybind11.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "strategy.hpp"
#include "types.hpp"

namespace qp::strategy::python {

/// Strategy that forwards on_event/on_timer to Python callables and
/// returns their intent iterable as a span over an in-object scratch pool.
///
/// @tparam MaxIntents Scratch capacity per call.
template <std::size_t MaxIntents = 32>
class PythonStrategy {
   public:
    static constexpr std::size_t kMaxIntents = MaxIntents;

    PythonStrategy(pybind11::object on_event_cb, pybind11::object on_timer_cb,
                   std::uint8_t kind_mask = 0xFF)
        : on_event_cb_{std::move(on_event_cb)},
          on_timer_cb_{std::move(on_timer_cb)},
          kind_mask_{kind_mask} {}

    std::span<const Intent> on_event(const MarketEvent& event) {
        // Skip the GIL and the Python call entirely for unsubscribed kinds.
        // A funding-only strategy pays nothing per kline this way.
        if (!(kind_mask_ & (1u << static_cast<unsigned>(base_of(event).kind)))) return {};
        return invoke(on_event_cb_, event);
    }

    std::span<const Intent> on_timer(Timestamp now) { return invoke(on_timer_cb_, now); }

   private:
    template <class Arg>
    std::span<const Intent> invoke(const pybind11::object& cb, const Arg& arg) {
        pybind11::gil_scoped_acquire gil;
        buffer_.reset();
        if (cb.is_none()) return buffer_.view();
        pybind11::object result = cb(arg);
        // None from Python is the empty iterable, so a no-op callback
        // doesn't have to construct a list to say nothing.
        if (result.is_none()) return buffer_.view();
        for (pybind11::handle h : result) buffer_.push(h.cast<Intent>());
        return buffer_.view();
    }

    pybind11::object          on_event_cb_;
    pybind11::object          on_timer_cb_;
    std::uint8_t              kind_mask_;
    IntentBuffer<kMaxIntents> buffer_{};
};

static_assert(Strategy<PythonStrategy<>>);

}  // namespace qp::strategy::python
