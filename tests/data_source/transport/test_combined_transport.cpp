#include <gtest/gtest.h>

#include <deque>
#include <optional>

#include "combined_transport.hpp"
#include "transport.hpp"
#include "types.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::SymbolId;
using qp::transport::CombinedTransport;
using qp::transport::Transport;

namespace {

// Vector/deque-backed fake, same shape used elsewhere in this session
// (test_engine.cpp's FakeTransport) — pops front-to-back, nullopt once
// drained.
class FakeSource {
   public:
    explicit FakeSource(std::deque<SymbolId> symbols) : symbols_(std::move(symbols)) {}

    std::optional<MarketEvent> next() {
        if (symbols_.empty()) return std::nullopt;
        MarketEvent ev;
        ev.kind   = EventKind::Trade;
        ev.symbol = symbols_.front();
        symbols_.pop_front();
        return ev;
    }

   private:
    std::deque<SymbolId> symbols_;
};

}  // namespace

static_assert(Transport<FakeSource>);
static_assert(Transport<CombinedTransport<FakeSource, FakeSource>>);

TEST(CombinedTransport, SingleSourcePassesThrough) {
    CombinedTransport<FakeSource> combined{FakeSource{{1, 2}}};

    auto first = combined.next();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->symbol, 1u);

    auto second = combined.next();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->symbol, 2u);

    EXPECT_FALSE(combined.next().has_value());
}

TEST(CombinedTransport, RoundRobinsAcrossTwoSourcesBeforeRepeatingEither) {
    // spot leg vs perp leg, each with its own symbol range, mirrors the
    // motivating case: a backtest carry transport over two legs.
    CombinedTransport<FakeSource, FakeSource> combined{FakeSource{{1, 1}}, FakeSource{{2, 2}}};

    auto a = combined.next();
    auto b = combined.next();
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    // First call starts at source 0, second resumes at source 1 — one pull
    // from each leg before either leg is revisited.
    EXPECT_EQ(a->symbol, 1u);
    EXPECT_EQ(b->symbol, 2u);
}

TEST(CombinedTransport, ExhaustedSourceDoesNotStarveTheOther) {
    CombinedTransport<FakeSource, FakeSource> combined{FakeSource{{}}, FakeSource{{2, 2}}};

    auto a = combined.next();
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->symbol, 2u);

    auto b = combined.next();
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->symbol, 2u);
}

TEST(CombinedTransport, ReturnsNulloptOnlyOnceEverySourceIsDrained) {
    CombinedTransport<FakeSource, FakeSource> combined{FakeSource{{1}}, FakeSource{{2}}};

    EXPECT_TRUE(combined.next().has_value());
    EXPECT_TRUE(combined.next().has_value());
    EXPECT_FALSE(combined.next().has_value());
}
