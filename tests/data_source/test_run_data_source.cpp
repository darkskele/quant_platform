#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <thread>
#include <tuple>
#include <vector>

#include "control_channel.hpp"
#include "run_data_source.hpp"
#include "sink.hpp"
#include "source.hpp"
#include "types.hpp"

using qp::ControlChannel;
using qp::ControlCommand;
using qp::MarketEvent;
using qp::data_source::run_data_source;
using qp::data_source::source::PullResult;
using qp::data_source::source::SourceStatus;

namespace {

// Finite: yields its queued prices in order, then Eof forever.
class FakeSource {
   public:
    explicit FakeSource(std::deque<qp::Price> prices) : prices_(std::move(prices)) {}

    PullResult next() {
        if (prices_.empty()) return std::unexpected(SourceStatus::Eof);
        qp::TradeEvent ev;
        ev.price = prices_.front();
        prices_.pop_front();
        return MarketEvent{ev};
    }

   private:
    std::deque<qp::Price> prices_;
};

// Never finishes, never has data — the shape a live feed satisfies, and
// the only way to exercise run_data_source's external-Stop exit.
struct NeverDoneSource {
    PullResult next() { return std::unexpected(SourceStatus::NoData); }
};

struct FakeSink {
    std::vector<MarketEvent> recorded;

    bool record(MarketEvent&& event) {
        recorded.push_back(std::move(event));
        return true;
    }
};

// Hard-capacity sink standing in for a queue a downstream consumer has
// stopped draining — the only way to exercise non-blocking delivery under
// real backpressure. `delivered` is an external atomic the test polls from
// another thread; `size` (the capacity check) is touched only by record()
// on the driver thread, so there's no cross-thread read of it.
struct BoundedSink {
    std::size_t               capacity;
    std::atomic<std::size_t>* delivered;
    std::size_t               size = 0;

    bool record(MarketEvent&&) {
        if (size >= capacity) return false;  // full -> don't consume, caller retries
        ++size;
        delivered->fetch_add(1, std::memory_order_release);
        return true;
    }
};

}  // namespace

static_assert(qp::data_source::source::Source<FakeSource>);
static_assert(qp::data_source::source::Source<NeverDoneSource>);
static_assert(qp::data_source::sink::Sink<FakeSink>);
static_assert(qp::data_source::sink::Sink<BoundedSink>);

// The property this driver exists for: source[i]'s event always lands in
// sink[i], stamped market=i, the pairing generated once by the fold. Both
// sources Eof after one item, so run_data_source returns on its own.
TEST(RunDataSource, PairsEachSourceWithTheMatchingSinkAndStampsVenue) {
    std::tuple<FakeSource, FakeSource> sources{FakeSource{{100.0}}, FakeSource{{200.0}}};
    std::tuple<FakeSink, FakeSink>     sinks;
    ControlChannel<1>                  control;
    std::size_t                        idx = control.attach();

    run_data_source(sources, sinks, control, idx);

    ASSERT_EQ(std::get<0>(sinks).recorded.size(), 1u);
    EXPECT_EQ(qp::header_of(std::get<0>(sinks).recorded[0]).market, 0);
    EXPECT_DOUBLE_EQ(std::get<qp::TradeEvent>(std::get<0>(sinks).recorded[0]).price, 100.0);

    ASSERT_EQ(std::get<1>(sinks).recorded.size(), 1u);
    EXPECT_EQ(qp::header_of(std::get<1>(sinks).recorded[0]).market, 1);
    EXPECT_DOUBLE_EQ(std::get<qp::TradeEvent>(std::get<1>(sinks).recorded[0]).price, 200.0);
}

// Returns once every source has reached Eof — the natural end of a
// backtest feed — having delivered every event first.
TEST(RunDataSource, ReturnsOnceEverySourceReachesEofHavingDeliveredEverything) {
    std::tuple<FakeSource, FakeSource> sources{FakeSource{{1.0, 2.0}}, FakeSource{{10.0}}};
    std::tuple<FakeSink, FakeSink>     sinks;
    ControlChannel<1>                  control;
    std::size_t                        idx = control.attach();

    run_data_source(sources, sinks, control, idx);  // returns synchronously

    EXPECT_EQ(std::get<0>(sinks).recorded.size(), 2u);
    EXPECT_EQ(std::get<1>(sinks).recorded.size(), 1u);
}

// A source that never reaches Eof (live-shaped) only stops on an external
// Stop over the control channel — how a live shutdown ends this loop.
TEST(RunDataSource, StopsPromptlyOnExternalStopWhenSourceNeverFinishes) {
    std::tuple<NeverDoneSource> sources{};
    std::tuple<FakeSink>        sinks;
    ControlChannel<1>           control;
    std::size_t                 idx = control.attach();

    auto        start = std::chrono::steady_clock::now();
    std::thread runner([&] {
        run_data_source(sources, sinks, control, idx, std::chrono::milliseconds(5),
                        /*stop_poll_every=*/1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    control.request_stop();
    runner.join();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::seconds(2));  // actually stopped, not hung
}

// The regression this design exists to fix: a full sink on leg 0 must
// never stop leg 1's source from being polled and delivered. A single
// driving thread blocking on leg 0's full sink would starve leg 1 forever;
// the pending-slot / non-blocking record() design prevents that. Leg 0
// never reaches Eof here (its pending event is stuck), so the run only
// ends on the external Stop.
TEST(RunDataSource, OneLegBackpressuringNeverStallsAnotherLeg) {
    std::tuple<FakeSource, FakeSource>   sources{FakeSource{{1.0, 2.0, 3.0}},
                                               FakeSource{{10.0, 20.0, 30.0}}};
    std::atomic<std::size_t>             d0{0}, d1{0};
    std::tuple<BoundedSink, BoundedSink> sinks{BoundedSink{1, &d0}, BoundedSink{1000, &d1}};
    ControlChannel<1>                    control;
    std::size_t                          idx = control.attach();

    std::thread runner(
        [&] { run_data_source(sources, sinks, control, idx, std::chrono::milliseconds(5)); });

    // Leg 1 (effectively unbounded) delivers all 3 of its own events even
    // though leg 0 is wedged after its first.
    for (int i = 0; i < 200 && d1.load(std::memory_order_acquire) < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_EQ(d1.load(std::memory_order_acquire), 3u);
    EXPECT_EQ(d0.load(std::memory_order_acquire), 1u);  // only the first ever fit

    control.request_stop();
    runner.join();
}
