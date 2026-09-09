#include <gtest/gtest.h>

#include <optional>

#include "cost_aware/cost_model/cost_model.hpp"
#include "types.hpp"

using qp::execution::sim::matcher::cost_aware::cost_model::CostModel;
using qp::execution::sim::matcher::cost_aware::cost_model::FillPricing;

namespace {

struct GoodCost {
    std::optional<FillPricing> price(const qp::Order&, qp::Price ref,
                                     qp::Timestamp) const noexcept {
        return FillPricing{.fill_price = ref, .fee = 0.0};
    }
};

struct MissingPrice {
    // No price() at all.
};

struct WrongPriceReturn {
    qp::Price price(const qp::Order&, qp::Price, qp::Timestamp) const noexcept { return 0.0; }
};

}  // namespace

static_assert(CostModel<GoodCost>);
static_assert(!CostModel<MissingPrice>);
static_assert(!CostModel<WrongPriceReturn>);

TEST(CostModel, PlaceholderKeepsTargetNonEmpty) {
    GoodCost c;
    auto     out = c.price(qp::Order{}, 100.0, 0);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0);
}
