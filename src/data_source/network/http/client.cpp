#include "client.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/beast/ssl.hpp>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace qp::data_source::network::http {

namespace beast = boost::beast;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp       = boost::asio::ip::tcp;

namespace {

struct Url {
    std::string host;
    std::string port{"443"};
    std::string target{"/"};
};

/// https URLs only.
Url parse_url(std::string_view url) {
    constexpr std::string_view prefix = "https://";
    if (url.substr(0, prefix.size()) != prefix)
        throw std::invalid_argument("http::get: only https:// URLs are supported");
    auto rest      = url.substr(prefix.size());
    auto slash_pos = rest.find('/');
    auto authority = slash_pos == std::string_view::npos ? rest : rest.substr(0, slash_pos);

    Url out;
    out.target = slash_pos == std::string_view::npos ? "/" : std::string(rest.substr(slash_pos));
    auto colon_pos = authority.find(':');
    if (colon_pos == std::string_view::npos) {
        out.host = std::string(authority);
    } else {
        out.host = std::string(authority.substr(0, colon_pos));
        out.port = std::string(authority.substr(colon_pos + 1));
    }
    return out;
}

ssl::context& tls_context() {
    static ssl::context ctx = [] {
        ssl::context c(ssl::context::tlsv12_client);
        c.set_default_verify_paths();
        c.set_verify_mode(ssl::verify_peer);
        return c;
    }();
    return ctx;
}

struct Connection {
    net::io_context                      ioc{1};
    beast::ssl_stream<beast::tcp_stream> stream{ioc, tls_context()};
};

class ConnectionPool {
   public:
    /// Dials, TLS handshake, SNI. Retries with exponential backoff. reused says
    /// whether this came off the pool, which decides if a failed request is
    /// worth retrying on a fresh socket.
    std::unique_ptr<Connection> acquire(const Url& url, const HttpConfig& config, bool& reused) {
        {
            std::lock_guard lock(mutex_);
            auto&           slot = pool_[url.host];
            if (!slot.empty()) {
                auto conn = std::move(slot.back());
                slot.pop_back();
                reused = true;
                return conn;
            }
        }
        reused = false;
        return dial(url, config);
    }

    /// Called only after a fully drained response.
    void release(const Url& url, std::unique_ptr<Connection> conn, const HttpConfig& config) {
        std::lock_guard lock(mutex_);
        auto&           slot = pool_[url.host];
        if (slot.size() < config.max_connections_per_host) slot.push_back(std::move(conn));
    }

   private:
    std::unique_ptr<Connection> dial(const Url& url, const HttpConfig& config) {
        for (std::size_t attempt = 0; attempt <= config.max_retries; ++attempt) {
            try {
                auto conn = std::make_unique<Connection>();
                if (!SSL_set_tlsext_host_name(conn->stream.native_handle(), url.host.c_str()))
                    throw std::runtime_error("http::get: SNI setup failed for " + url.host);

                tcp::resolver resolver(conn->ioc);
                auto          endpoints = resolver.resolve(url.host, url.port);

                beast::get_lowest_layer(conn->stream).expires_after(std::chrono::seconds(30));
                beast::get_lowest_layer(conn->stream).connect(endpoints);
                conn->stream.handshake(ssl::stream_base::client);
                return conn;
            } catch (const std::exception& e) {
                if (attempt == config.max_retries)
                    throw HttpTransportError("http::get: " + url.host + ": " + e.what());
                std::this_thread::sleep_for(config.backoff_base * (std::size_t{1} << attempt));
            }
        }
        throw HttpTransportError("http::get: unreachable");
    }

    std::mutex                                                               mutex_;
    std::unordered_map<std::string, std::deque<std::unique_ptr<Connection>>> pool_;
};

ConnectionPool& pool() {
    static ConnectionPool instance;
    return instance;
}

}  // namespace

std::vector<std::byte> get(std::string_view url_str, const HttpConfig& config) {
    const Url url = parse_url(url_str);

    for (std::size_t attempt = 0;; ++attempt) {
        bool reused = false;
        auto conn   = pool().acquire(url, config, reused);

        beast::http::request<beast::http::empty_body> req{beast::http::verb::get, url.target, 11};
        req.set(beast::http::field::host, url.host);
        req.set(beast::http::field::user_agent, "qp-platform");

        beast::flat_buffer                                                buffer;
        beast::http::response_parser<beast::http::vector_body<std::byte>> parser;
        parser.body_limit(std::numeric_limits<std::uint64_t>::max());

        try {
            beast::get_lowest_layer(conn->stream).expires_after(std::chrono::seconds(60));
            beast::http::write(conn->stream, req);
            beast::http::read(conn->stream, buffer, parser);
        } catch (const std::exception& e) {
            // A pooled socket the server already closed surfaces here, as a
            // write error or an end of stream. Redialing is the fix, and it is
            // not a failed request, so it does not count as one.
            if (reused && attempt < config.max_retries) continue;
            throw HttpTransportError("http::get: " + std::string(url_str) + ": " + e.what());
        }

        if (const int status = parser.get().result_int(); status / 100 != 2)
            // Not returned to the pool.
            throw HttpStatusError(
                status, "http::get: " + std::string(url_str) + " -> " + std::to_string(status));

        pool().release(url, std::move(conn), config);
        return std::move(parser.release().body());
    }
}

}  // namespace qp::data_source::network::http
