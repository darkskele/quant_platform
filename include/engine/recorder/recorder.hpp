#pragma once
#include <concepts>

#include "types.hpp"

namespace qp::engine {

/// The per-step observation seam Engine drives once per processed event.
/// sample() sees the timestamp and the portfolio; reading equity() is the
/// recorder's own choice, so a recorder that wants nothing costs nothing.
template <class R, class Book>
concept Recorder = requires(R r, Timestamp ts, const Book& book) {
    { r.sample(ts, book) } -> std::same_as<void>;
};

}  // namespace qp::engine
