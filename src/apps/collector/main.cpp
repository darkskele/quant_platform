#include <csignal>

#include "collector.hpp"
#include "control_channel.hpp"

namespace {

// Global, not passed via argv/state: a POSIX signal handler can only touch
// static storage. request_stop() only touches lock-free atomics and
// placement-news a trivial ControlCommand — no allocation, no locks, no
// exceptions on this path — so it's async-signal-safe in practice, though
// the C++ standard doesn't officially bless arbitrary class methods as
// such the way it does e.g. sig_atomic_t; same level of rigor the plain
// std::atomic<bool> this replaces already had.
qp::ControlChannel<1> g_control;

extern "C" void handle_signal(int) { g_control.request_stop(); }

}  // namespace

int main(int argc, char** argv) {
    auto config = qp::collector::parse_args(argc, argv);
    if (!config) return 1;

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    // attach() before the signal handler could plausibly fire (already
    // installed above, but nothing async has happened yet) — same ordering
    // note as collector.hpp's own doc comment on `control`.
    auto consumer = g_control.attach();
    return qp::collector::run(*config, g_control, consumer);
}
