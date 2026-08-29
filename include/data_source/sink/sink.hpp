#pragma once
#include "types.hpp"

namespace qp::data_source::sink {

// The seam every data sink satisfies. Compile-time policy — mirrors
// source.hpp's Source concept.
template <class T>
concept Sink = requires(T s, MarketEvent ev) {
    { s.record(std::move(ev)) } -> std::same_as<void>;
};

}  // namespace qp::data_source::sink
