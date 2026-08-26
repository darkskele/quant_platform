#include <atomic>
#include <csignal>

#include "collector.hpp"

namespace {

std::atomic<bool> g_stop_requested{false};

extern "C" void handle_signal(int) { g_stop_requested.store(true, std::memory_order_release); }

}  // namespace

int main(int argc, char** argv) {
    auto config = qp::collector::parse_args(argc, argv);
    if (!config) return 1;

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    return qp::collector::run(*config, g_stop_requested);
}
