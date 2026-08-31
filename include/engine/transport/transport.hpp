#pragma once
#include <optional>

#include "types.hpp"

namespace qp::engine::transport {

/// The seam Engine actually consumes — one MarketEvent at a time, or
/// nullopt.
template <class T>
concept Transport = requires(T t) {
    { t.next() } -> std::same_as<std::optional<MarketEvent>>;
};

}  // namespace qp::engine::transport
