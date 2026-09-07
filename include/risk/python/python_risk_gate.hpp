#pragma once
#include <pybind11/pybind11.h>

#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include "risk_gate.hpp"
#include "types.hpp"
#include "viewable_pool.hpp"

namespace qp::risk::python {

/// Risk gate that forwards check/on_tick to Python callables. check returns
/// a RiskDecision, on_tick returns a span over an in-object scratch pool.
///
/// @tparam MaxOrders on_tick scratch capacity.
template <std::size_t MaxOrders = 32>
class PythonRiskGate {
   public:
    static constexpr std::size_t kMaxOrders = MaxOrders;

    PythonRiskGate(pybind11::object check_cb, pybind11::object on_tick_cb)
        : check_cb_{std::move(check_cb)}, on_tick_cb_{std::move(on_tick_cb)} {}

    RiskDecision check(Intent intent) {
        pybind11::gil_scoped_acquire gil;
        // Missing check callable and None result both mean "reject",
        // matching the outcome a tripped gate would return.
        if (check_cb_.is_none()) return {RiskOutcome::Rejected, std::nullopt};
        pybind11::object result = check_cb_(intent);
        if (result.is_none()) return {RiskOutcome::Rejected, std::nullopt};
        return result.cast<RiskDecision>();
    }

    std::span<const Order> on_tick() {
        pybind11::gil_scoped_acquire gil;
        orders_.reset();
        if (on_tick_cb_.is_none()) return orders_.view();
        pybind11::object result = on_tick_cb_();
        if (result.is_none()) return orders_.view();
        for (pybind11::handle h : result) orders_.push(h.cast<Order>());
        return orders_.view();
    }

   private:
    pybind11::object                check_cb_;
    pybind11::object                on_tick_cb_;
    ViewablePool<Order, kMaxOrders> orders_{};
};

static_assert(RiskGate<PythonRiskGate<>>);

}  // namespace qp::risk::python
