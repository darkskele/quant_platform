// netbench_kernel_tcp. The rig receiver over KernelTcpStream, or with send its sender.
#include <string_view>

#include "kernel_tcp_stream.hpp"
#include "receiver.hpp"
#include "sender.hpp"

using namespace qp::network;

template <transport::RxStamps Stamps>
using Stream = transport::byte_stream::KernelTcpStream<1 << 20, 64, 1 << 20, Stamps>;

int main(int argc, char** argv) {
    // send as the first argument makes it the sender under test.
    if (argc > 1 && std::string_view(argv[1]) == "send") {
        argv[1]      = argv[0];
        const auto s = rig::parse_sender_args(argc - 1, argv + 1);
        if (!s) return 2;
        return rig::run_stream_sender<Stream<transport::RxStamps{}>>(*s, "kernel_tcp");
    }
    const auto a = rig::parse_receiver_args(argc, argv);
    if (!a) return 2;
    if (a->stamps == "kernel")
        return rig::run_stream_receiver<Stream<transport::RxStamps{.kernel = true}>>(*a,
                                                                                     "kernel_tcp");
    return rig::run_stream_receiver<Stream<transport::RxStamps{}>>(*a, "kernel_tcp");
}
