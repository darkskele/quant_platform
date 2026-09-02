#pragma once
#include "types.hpp"

namespace qp::data_source::sink {

// The seam every data sink satisfies. Compile-time policy. Mirrors
// source.hpp's Source concept. record() never blocks: false means the
// sink wasn't ready (e.g. a full queue), and it's the caller's call what
// to do about that.
template <class T>
concept Sink = requires(T s, MarketEvent ev) {
    { s.record(std::move(ev)) } -> std::same_as<bool>;
};

}  // namespace qp::data_source::sink
