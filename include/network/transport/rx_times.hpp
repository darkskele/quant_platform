#pragma once
#include <cstdint>

namespace qp::network::transport {

/// Nanoseconds.
using RxTime = std::int64_t;

/// When received bytes reached each layer. Zero when that stamp is off or not supported.
struct RxTimes {
    RxTime nic    = 0;  ///< Stamped by the NIC, on the NIC's own clock.
    RxTime kernel = 0;  ///< Stamped by the kernel on arrival, on the monotonic clock.
    RxTime user   = 0;  ///< Visible to user space, on the monotonic clock. Always taken.
};

/// Which receive stamps a backend takes besides the user one.
struct RxStamps {
    bool kernel = false;
    bool nic    = false;
};

}  // namespace qp::network::transport
