#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>

#include "control_channel.hpp"

using qp::ControlChannel;
using qp::ControlCommand;

namespace {

// request_stop() only lands visibly via the background pump thread
// (kPumpInterval = 100ms, control_channel.hpp) — poll() alone can't
// observe it synchronously the way it used to when callers pumped
// themselves. Spins with a short sleep rather than a single fixed wait, so
// this doesn't need to guess the exact interval and isn't flaky under
// scheduler jitter; bounded well above kPumpInterval so a real regression
// still fails instead of hanging.
template <std::size_t N>
std::optional<ControlCommand> wait_for(ControlChannel<N>& channel, std::size_t consumer) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto cmd = channel.poll(consumer)) return cmd;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return std::nullopt;
}

}  // namespace

TEST(ControlChannel, AttachHandsOutSequentialIdsInCallOrder) {
    ControlChannel<3> control;
    EXPECT_EQ(control.attach(), 0u);
    EXPECT_EQ(control.attach(), 1u);
    EXPECT_EQ(control.attach(), 2u);
}

TEST(ControlChannel, AttachThrowsOnceEveryConsumerSlotIsTaken) {
    ControlChannel<1> control;
    control.attach();
    EXPECT_THROW(control.attach(), std::out_of_range);
}

TEST(ControlChannel, BroadcastIsVisibleToPollImmediatelyNoPumpInvolved) {
    ControlChannel<1> control;
    auto              consumer = control.attach();

    control.broadcast(ControlCommand::Start);

    EXPECT_EQ(control.poll(consumer), ControlCommand::Start);
    EXPECT_FALSE(control.poll(consumer).has_value());  // one command, already drained
}

TEST(ControlChannel, EachAttachedConsumerSeesTheSameBroadcastIndependently) {
    ControlChannel<2> control;
    auto              a = control.attach();
    auto              b = control.attach();

    control.broadcast(ControlCommand::Start);

    EXPECT_EQ(control.poll(a), ControlCommand::Start);
    EXPECT_EQ(control.poll(b),
              ControlCommand::Start);  // independent cursor, not consumed by a's poll
}

TEST(ControlChannel, CompileTimeIndexedPollSeesTheSameBroadcastAsRuntimeIndexed) {
    ControlChannel<1> control;
    control.attach();
    control.broadcast(ControlCommand::Stop);

    EXPECT_EQ((control.poll<0>()), ControlCommand::Stop);
}

TEST(ControlChannel, RequestStopIsEventuallyVisibleViaPollWithoutTheCallerPumping) {
    ControlChannel<1> control;
    auto              consumer = control.attach();

    EXPECT_TRUE(control.request_stop());
    EXPECT_FALSE(
        control.poll(consumer).has_value());  // not pumped yet -- request alone isn't enough

    auto seen = wait_for(control, consumer);
    ASSERT_TRUE(seen.has_value());
    EXPECT_EQ(*seen, ControlCommand::Stop);
}

TEST(ControlChannel, MultipleParticipantsAllEventuallyObserveOneRequestStop) {
    ControlChannel<2> control;
    auto              a = control.attach();
    auto              b = control.attach();

    EXPECT_TRUE(control.request_stop());

    EXPECT_EQ(wait_for(control, a), ControlCommand::Stop);
    EXPECT_EQ(wait_for(control, b), ControlCommand::Stop);
}
