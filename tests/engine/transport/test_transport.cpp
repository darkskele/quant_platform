#include <gtest/gtest.h>

#include <optional>

#include "transport.hpp"
#include "types.hpp"

using qp::MarketEvent;
using qp::engine::transport::Transport;

namespace {

struct GoodTransport {
    std::optional<MarketEvent> next() { return std::nullopt; }

    void flush() noexcept {}
};

struct WrongReturnType {
    bool next() { return false; }

    void flush() noexcept {}
};

struct MissingNext {
    void flush() noexcept {}
};

struct MissingFlush {
    std::optional<MarketEvent> next() { return std::nullopt; }
};

}  // namespace

static_assert(Transport<GoodTransport>);
static_assert(!Transport<WrongReturnType>);
static_assert(!Transport<MissingNext>);
static_assert(!Transport<MissingFlush>);

TEST(Transport, PlaceholderKeepsTargetNonEmpty) {
    // The interesting checks are the static_asserts above (compile-time);
    // gtest needs at least one TEST for the binary to be a meaningful target.
    GoodTransport t;
    EXPECT_FALSE(t.next().has_value());
}
