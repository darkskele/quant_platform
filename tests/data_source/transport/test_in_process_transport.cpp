#include <gtest/gtest.h>

#include "in_process_transport.hpp"
#include "spmc_ring.hpp"
#include "transport.hpp"
#include "types.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::SpmcRing;
using qp::transport::InProcessTransport;
using qp::transport::Transport;

namespace {

using Ring = SpmcRing<std::shared_ptr<const MarketEvent>, 4, 2>;

MarketEvent trade(qp::SymbolId symbol, qp::Price price) {
    MarketEvent ev;
    ev.kind   = EventKind::Trade;
    ev.symbol = symbol;
    ev.price  = price;
    return ev;
}

}  // namespace

static_assert(Transport<InProcessTransport<Ring, 0>>);

TEST(InProcessTransport, EmptyRingReturnsNullopt) {
    Ring                        ring;
    InProcessTransport<Ring, 0> transport(ring);
    EXPECT_FALSE(transport.next().has_value());
}

TEST(InProcessTransport, ReadsBackPushedEventByValueNotPointer) {
    Ring ring;
    ring.push(std::make_shared<const MarketEvent>(trade(qp::SymbolId{1}, qp::Price{100})));

    InProcessTransport<Ring, 0> transport(ring);
    auto                        event = transport.next();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->symbol, qp::SymbolId{1});
    EXPECT_EQ(event->price, qp::Price{100});
    EXPECT_FALSE(transport.next().has_value());
}

TEST(InProcessTransport, IndependentConsumersEachSeeEveryEvent) {
    Ring ring;
    ring.push(std::make_shared<const MarketEvent>(trade(qp::SymbolId{1}, qp::Price{100})));

    InProcessTransport<Ring, 0> first(ring);
    InProcessTransport<Ring, 1> second(ring);

    ASSERT_TRUE(first.next().has_value());
    // second hasn't read yet — its own cursor, not shared with first's.
    ASSERT_TRUE(second.next().has_value());
}
