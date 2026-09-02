#pragma once
#include <optional>

#include "types.hpp"

namespace qp::engine::transport {

/// The seam Engine actually consumes: one MarketEvent at a time, or
/// nullopt. flush() switches next() out of its "wait for every leg" mode
/// into "drain what's buffered without waiting".
template <class T>
concept Transport = requires(T t) {
    { t.next() } -> std::same_as<std::optional<MarketEvent>>;
    { t.flush() } -> std::same_as<void>;
};

}  // namespace qp::engine::transport
