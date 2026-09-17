#pragma once
#include <chrono>
#include <cstddef>
#include <string_view>
#include <vector>

namespace qp::data_source::network::http {

struct HttpConfig {
    std::size_t               max_retries              = 5;
    std::chrono::milliseconds backoff_base             = std::chrono::milliseconds{200};
    std::size_t               max_connections_per_host = 16;
};

/// Fetches url over https, returns the body. Throws on failure.
std::vector<std::byte> get(std::string_view url, const HttpConfig& config = {});

}  // namespace qp::data_source::network::http
