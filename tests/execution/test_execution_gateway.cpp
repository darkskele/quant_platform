#include <gtest/gtest.h>

#include <span>

#include "execution_gateway.hpp"
#include "types.hpp"

using qp::execution::ExecutionGateway;

namespace {

struct GoodGateway {
    void on_market_event(const qp::MarketEvent&) {}

    void submit(qp::Order, qp::Timestamp) {}

    std::span<const qp::Fill> fills() const noexcept { return {}; }

    std::span<const qp::Reject> rejects() const noexcept { return {}; }
};

struct MissingSubmit {
    void on_market_event(const qp::MarketEvent&) {}

    std::span<const qp::Fill> fills() const noexcept { return {}; }

    std::span<const qp::Reject> rejects() const noexcept { return {}; }
};

struct WrongFillsType {
    void on_market_event(const qp::MarketEvent&) {}

    void submit(qp::Order, qp::Timestamp) {}

    std::span<qp::Fill> fills() const noexcept { return {}; }  // must be span<const Fill>

    std::span<const qp::Reject> rejects() const noexcept { return {}; }
};

}  // namespace

static_assert(ExecutionGateway<GoodGateway>);
static_assert(!ExecutionGateway<MissingSubmit>);
static_assert(!ExecutionGateway<WrongFillsType>);

TEST(ExecutionGateway, PlaceholderKeepsTargetNonEmpty) {
    GoodGateway g;
    EXPECT_TRUE(g.fills().empty());
}
