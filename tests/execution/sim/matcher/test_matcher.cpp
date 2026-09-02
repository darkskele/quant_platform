#include <gtest/gtest.h>

#include <variant>

#include "matcher.hpp"
#include "types.hpp"

using qp::execution::sim::matcher::Matcher;

namespace {

struct GoodMatcher {
    void on_market_event(const qp::MarketEvent&) {}

    std::variant<qp::Fill, qp::Reject> try_fill(qp::Order, qp::Timestamp) { return qp::Fill{}; }
};

struct MissingTryFill {
    void on_market_event(const qp::MarketEvent&) {}
};

struct WrongTryFillReturn {
    void on_market_event(const qp::MarketEvent&) {}

    qp::Fill try_fill(qp::Order, qp::Timestamp) { return {}; }  // must be variant<Fill, Reject>
};

}  // namespace

static_assert(Matcher<GoodMatcher>);
static_assert(!Matcher<MissingTryFill>);
static_assert(!Matcher<WrongTryFillReturn>);

TEST(Matcher, PlaceholderKeepsTargetNonEmpty) {
    GoodMatcher m;
    EXPECT_TRUE(std::holds_alternative<qp::Fill>(m.try_fill(qp::Order{}, 0)));
}
