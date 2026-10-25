// netbench_kernel_udp. The rig receiver over KernelUdpChannel. Slots hold the largest datagram
// the sweep sends.
#include "kernel_udp_channel.hpp"
#include "receiver.hpp"

using namespace qp::network;

template <transport::RxStamps Stamps>
using Channel = transport::datagram::KernelUdpChannel<8192, 1024, 32, Stamps>;

int main(int argc, char** argv) {
    const auto a = rig::parse_receiver_args(argc, argv);
    if (!a) return 2;
    if (a->stamps == "kernel")
        return rig::run_channel_receiver<Channel<transport::RxStamps{.kernel = true}>>(
            *a, "kernel_udp");
    return rig::run_channel_receiver<Channel<transport::RxStamps{}>>(*a, "kernel_udp");
}
