#pragma once
#include "types.hpp"

namespace qp {

/// The seam anything needing "now" satisfies.
template <class T>
concept Clock = requires(T c, Timestamp ts) {
    { c.now() } -> std::same_as<Timestamp>;
    { c.advance(ts) } -> std::same_as<void>;
};

}  // namespace qp
