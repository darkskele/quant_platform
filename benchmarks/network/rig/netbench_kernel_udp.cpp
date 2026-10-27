// netbench_kernel_udp. The rig receiver over KernelUdpChannel, or with send its sender. Slots hold
// the largest datagram the sweep sends.
#include <string_view>

#include "kernel_udp_channel.hpp"
#include "receiver.hpp"
#include "sender.hpp"

using namespace qp::network;

template <transport::RxStamps Stamps>
using Channel = transport::datagram::KernelUdpChannel<8192, 1024, 32, Stamps>;

int main(int argc, char** argv) {
    // send as the first argument makes it the sender under test.
    if (argc > 1 && std::string_view(argv[1]) == "send") {
        argv[1]      = argv[0];
        const auto s = rig::parse_sender_args(argc - 1, argv + 1);
        if (!s) return 2;
        return rig::run_channel_sender<Channel<transport::RxStamps{}>>(*s, "kernel_udp");
    }
    const auto a = rig::parse_receiver_args(argc, argv);
    if (!a) return 2;
    if (a->stamps == "kernel")
        return rig::run_channel_receiver<Channel<transport::RxStamps{.kernel = true}>>(
            *a, "kernel_udp");
    return rig::run_channel_receiver<Channel<transport::RxStamps{}>>(*a, "kernel_udp");
}
