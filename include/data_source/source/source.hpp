#pragma once
#include <expected>

#include "types.hpp"

namespace qp::data_source::source {

/// Why a next() call produced no event.
enum class SourceStatus { NoData, Eof };

/// One pulled event, or the reason there wasn't one.
using PullResult = std::expected<MarketEvent, SourceStatus>;

// The seam every data source satisfies: next() pulls MarketEvents. A source
// that never finishes just never returns Eof.
template <class T>
concept Source = requires(T s) {
    { s.next() } -> std::same_as<PullResult>;
};

}  // namespace qp::data_source::source
