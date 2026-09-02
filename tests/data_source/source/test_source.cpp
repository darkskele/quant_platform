#include <gtest/gtest.h>

#include <expected>

#include "source.hpp"
#include "types.hpp"

using qp::data_source::source::PullResult;
using qp::data_source::source::Source;
using qp::data_source::source::SourceStatus;

namespace {

struct GoodSource {
    PullResult next() { return std::unexpected(SourceStatus::Eof); }
};

struct MissingNext {};

struct WrongNextReturn {
    qp::MarketEvent next() { return {}; }  // must return PullResult
};

}  // namespace

static_assert(Source<GoodSource>);
static_assert(!Source<MissingNext>);
static_assert(!Source<WrongNextReturn>);

TEST(Source, PlaceholderKeepsTargetNonEmpty) {
    GoodSource s;
    EXPECT_FALSE(s.next().has_value());
}
