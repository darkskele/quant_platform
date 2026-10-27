#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace qp::network::rig {

/// An IPv4 address and port, written ip:port.
struct Endpoint {
    std::string   ip;
    std::uint16_t port = 0;
};

inline std::optional<Endpoint> parse_endpoint(std::string_view text) {
    const auto colon = text.rfind(':');
    if (colon == std::string_view::npos) return std::nullopt;
    Endpoint   e{std::string(text.substr(0, colon)), 0};
    const auto port       = text.substr(colon + 1);
    const auto [end, err] = std::from_chars(port.data(), port.data() + port.size(), e.port);
    if (err != std::errc{} || end != port.data() + port.size()) return std::nullopt;
    return e;
}

inline std::optional<sockaddr_in> to_sockaddr(const Endpoint& e) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(e.port);
    if (::inet_pton(AF_INET, e.ip.c_str(), &addr.sin_addr) != 1) return std::nullopt;
    return addr;
}

/// A UDP socket bound to bind_to when given, connected to connect_to when given. -1 on failure.
inline int udp_socket(const Endpoint* bind_to, const Endpoint* connect_to) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    auto fail = [fd] {
        ::close(fd);
        return -1;
    };
    if (bind_to) {
        const auto addr = to_sockaddr(*bind_to);
        if (!addr || ::bind(fd, reinterpret_cast<const sockaddr*>(&*addr), sizeof *addr) != 0)
            return fail();
    }
    if (connect_to) {
        const auto addr = to_sockaddr(*connect_to);
        if (!addr || ::connect(fd, reinterpret_cast<const sockaddr*>(&*addr), sizeof *addr) != 0)
            return fail();
    }
    return fd;
}

}  // namespace qp::network::rig
