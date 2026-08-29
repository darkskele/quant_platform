#pragma once
#include <optional>

#include "types.hpp"

namespace qp::data_source::source {

// The seam every data source satisfies. Compile-time policy. Book reconstruction
// lives BEHIND this seam, so live and replay emit identical MarketEvents.
template <class T>
concept Source = requires(T s) {
    { s.next() } -> std::same_as<std::optional<MarketEvent>>;
};

}  // namespace qp::data_source::source
