#pragma once
#include "source.hpp"

namespace qp::data_source::source {

// Placeholder Source that reports Eof immediately. Stands in where a
// composition has no source wired up yet.
struct EofSource {
    PullResult next() { return std::unexpected(SourceStatus::Eof); }
};

}  // namespace qp::data_source::source
