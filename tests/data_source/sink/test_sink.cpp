#include <gtest/gtest.h>

#include "sink.hpp"
#include "types.hpp"

using qp::data_source::sink::Sink;

namespace {

struct GoodSink {
    bool record(qp::MarketEvent&&) { return true; }
};

struct MissingRecord {};

struct WrongRecordReturn {
    void record(qp::MarketEvent&&) {}  // must return bool
};

}  // namespace

static_assert(Sink<GoodSink>);
static_assert(!Sink<MissingRecord>);
static_assert(!Sink<WrongRecordReturn>);

TEST(Sink, PlaceholderKeepsTargetNonEmpty) {
    GoodSink s;
    EXPECT_TRUE(s.record(qp::MarketEvent{}));
}
