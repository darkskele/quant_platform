#pragma once
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace qp::data_source::network::http {

/// A request that reached the server and came back non 2xx. status is the code.
struct HttpStatusError : std::runtime_error {
    HttpStatusError(int code, const std::string& what) : std::runtime_error(what), status(code) {}

    int status;
};

/// A request that never produced a response. Resolve, connect, TLS or read.
struct HttpTransportError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct HttpConfig {
    std::size_t               max_retries              = 5;
    std::chrono::milliseconds backoff_base             = std::chrono::milliseconds{200};
    std::size_t               max_connections_per_host = 16;
};

/// Fetches url over https, returns the body. Throws on failure.
std::vector<std::byte> get(std::string_view url, const HttpConfig& config = {});

/// Closes every pooled connection.
void close_idle_connections();

/// Pooled connections held across every host.
std::size_t idle_connection_count();

}  // namespace qp::data_source::network::http
