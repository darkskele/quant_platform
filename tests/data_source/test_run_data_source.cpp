#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <optional>
#include <thread>
#include <tuple>
#include <vector>

#include "run_data_source.hpp"
#include "sink.hpp"
#include "source.hpp"
#include "types.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::data_source::run_data_source;

namespace {

class FakeSource {
   public:
    explicit FakeSource(std::deque<qp::Price> prices) : prices_(std::move(prices)) {}

    std::optional<MarketEvent> next() {
        if (prices_.empty()) return std::nullopt;
        qp::TradeEvent ev;
        ev.price = prices_.front();
        prices_.pop_front();
        return ev;
    }

   private:
    std::deque<qp::Price> prices_;
};

struct FakeSink {
    std::vector<MarketEvent> recorded;

    void record(MarketEvent event) { recorded.push_back(std::move(event)); }
};

}  // namespace

static_assert(qp::data_source::source::Source<FakeSource>);
static_assert(qp::data_source::sink::Sink<FakeSink>);

// The actual property this driver exists for: source[i]'s event always
// lands in sink[i], stamped venue=i — the pairing is generated once by
// the fold, not hand-written per call site.
TEST(RunDataSource, PairsEachSourceWithTheMatchingSinkAndStampsVenue) {
    std::tuple<FakeSource, FakeSource> sources{FakeSource{{100.0}}, FakeSource{{200.0}}};
    std::tuple<FakeSink, FakeSink>     sinks;
    std::atomic<bool>                  running{true};

    std::thread runner([&] { run_data_source(sources, sinks, running); });
    // Both sources drain on the very first round; this just gives the
    // driver thread room to actually get scheduled and run it.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running.store(false, std::memory_order_release);
    runner.join();

    ASSERT_EQ(std::get<0>(sinks).recorded.size(), 1u);
    EXPECT_EQ(qp::header_of(std::get<0>(sinks).recorded[0]).venue, 0);
    EXPECT_DOUBLE_EQ(std::get<qp::TradeEvent>(std::get<0>(sinks).recorded[0]).price, 100.0);

    ASSERT_EQ(std::get<1>(sinks).recorded.size(), 1u);
    EXPECT_EQ(qp::header_of(std::get<1>(sinks).recorded[0]).venue, 1);
    EXPECT_DOUBLE_EQ(std::get<qp::TradeEvent>(std::get<1>(sinks).recorded[0]).price, 200.0);
}

TEST(RunDataSource, StopsPromptlyWhenRunningFlagClears) {
    std::tuple<FakeSource> sources{FakeSource{{}}};  // never produces anything
    std::tuple<FakeSink>   sinks;
    std::atomic<bool>      running{true};

    auto        start = std::chrono::steady_clock::now();
    std::thread runner(
        [&] { run_data_source(sources, sinks, running, std::chrono::milliseconds(5)); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running.store(false, std::memory_order_release);
    runner.join();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::seconds(2));  // actually stopped, not hung
}
