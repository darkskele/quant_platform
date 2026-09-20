#pragma once
#include "portfolio.hpp"
#include "recorder/recorder.hpp"
#include "types.hpp"

namespace qp::engine {

/// Records nothing. The default, and what a latency-sensitive path uses.
struct NullRecorder {
    template <class Book>
    void sample(Timestamp, const Book&) noexcept {}
};

static_assert(Recorder<NullRecorder, qp::Portfolio>);

}  // namespace qp::engine
