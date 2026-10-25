// netbench_kernel_tcp. The rig receiver over KernelTcpStream.
#include "kernel_tcp_stream.hpp"
#include "receiver.hpp"

using namespace qp::network;

template <transport::RxStamps Stamps>
using Stream = transport::byte_stream::KernelTcpStream<1 << 20, 64, 1 << 16, Stamps>;

int main(int argc, char** argv) {
    const auto a = rig::parse_receiver_args(argc, argv);
    if (!a) return 2;
    if (a->stamps == "kernel")
        return rig::run_stream_receiver<Stream<transport::RxStamps{.kernel = true}>>(*a,
                                                                                     "kernel_tcp");
    return rig::run_stream_receiver<Stream<transport::RxStamps{}>>(*a, "kernel_tcp");
}
