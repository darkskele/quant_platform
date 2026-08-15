#include "qp/marketdata/live_ws_source.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>

#include "qp/marketdata/backoff.hpp"

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = net::ssl;
using tcp           = net::ip::tcp;

namespace qp {

// ---------------------------------------------------------------------------
// WebSocket transport (unchanged in shape from before D10 — this is the
// existing connect/handshake/read state machine, not new for this change).
// ---------------------------------------------------------------------------
namespace {

// on_message/on_idle are template parameters, not std::function, all the
// way down this call chain (run_ws_session -> connect_tls/connect_plain ->
// connect_and_stream). Both are always called with the same two concrete
// lambda types from exactly one call site (LiveWebSocketSource::run()) —
// there's no runtime polymorphism actually happening here, so type erasure
// would only be paying for a capability nothing uses. Per D4 (compile-time
// dispatch on fixed seams), this is a fixed seam: template it, let the
// compiler see straight through to the real call, don't force an indirect
// call through a vtable-shaped std::function for something that's never
// swapped at runtime.

// Read loop polls on a short timeout rather than blocking indefinitely on
// one read. Why: the resync response queue is only drained reactively
// (drain_resync_responses() runs at the top of on_message), so a burst of
// messages followed by quiet — buffer some diffs, resync completes, then
// nothing else arrives for a while — would otherwise leave a fully-resolved
// resync sitting unclaimed, with the buffered events never replayed to the
// output queue, until the *next* message happens to arrive. on_idle() gives
// the caller a chance to drain on every poll instead of only on message
// arrival. A real dead connection is still detected — just via cumulative
// silence (kDeadConnectionTimeout) tracked across polls, not the per-read
// wait itself.
template <class NextLayer, class OnMessage, class OnIdle>
void run_ws_session(websocket::stream<NextLayer>& ws, const std::string& host, const std::string& path,
                     const std::atomic<bool>& running, const OnMessage& on_message, const OnIdle& on_idle) {
    static constexpr auto kPollInterval          = std::chrono::milliseconds(200);
    static constexpr auto kDeadConnectionTimeout = std::chrono::seconds(30);

    ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(beast::http::field::user_agent, "quant-platform-collector/0.1");
    }));

    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(10));
    ws.handshake(host, path);

    beast::flat_buffer buffer;
    auto                last_activity = std::chrono::steady_clock::now();
    while (running.load(std::memory_order_acquire)) {
        buffer.clear();
        beast::get_lowest_layer(ws).expires_after(kPollInterval);

        beast::error_code ec;
        ws.read(buffer, ec);

        if (ec == websocket::error::closed) return;  // server closed cleanly

        if (ec == beast::error::timeout) {
            on_idle();
            if (std::chrono::steady_clock::now() - last_activity > kDeadConnectionTimeout) {
                throw beast::system_error(ec);  // genuinely silent connection — reconnect
            }
            continue;
        }
        if (ec) throw beast::system_error(ec);

        last_activity = std::chrono::steady_clock::now();
        on_message(beast::buffers_to_string(buffer.data()));
    }

    beast::error_code ec;
    ws.close(websocket::close_code::normal, ec);
}

template <class OnMessage, class OnIdle>
void connect_tls(const std::string& host, const std::string& port, const std::string& path,
                  const std::atomic<bool>& running, const OnMessage& on_message, const OnIdle& on_idle) {
    net::io_context ioc;
    ssl::context    ssl_ctx{ssl::context::tlsv12_client};
    ssl_ctx.set_verify_mode(ssl::verify_peer);
    ssl_ctx.set_default_verify_paths();

    tcp::resolver                                            resolver{ioc};
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws{ioc, ssl_ctx};

    ws.next_layer().set_verify_callback(net::ssl::host_name_verification(host));

    const auto results = resolver.resolve(host, port);

    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(10));
    beast::get_lowest_layer(ws).connect(results);

    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
    }

    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(10));
    ws.next_layer().handshake(ssl::stream_base::client);

    run_ws_session(ws, host, path, running, on_message, on_idle);
}

template <class OnMessage, class OnIdle>
void connect_plain(const std::string& host, const std::string& port, const std::string& path,
                    const std::atomic<bool>& running, const OnMessage& on_message, const OnIdle& on_idle) {
    net::io_context                      ioc;
    tcp::resolver                        resolver{ioc};
    websocket::stream<beast::tcp_stream> ws{ioc};

    const auto results = resolver.resolve(host, port);

    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(10));
    beast::get_lowest_layer(ws).connect(results);

    run_ws_session(ws, host, path, running, on_message, on_idle);
}

template <class OnMessage, class OnIdle>
void connect_and_stream(const std::vector<std::string>& symbols, venue::binance::WsEndpoint endpoint,
                         const std::atomic<bool>& running, const OnMessage& on_message, const OnIdle& on_idle) {
    const std::string host = std::string(endpoint.host);
    const std::string port = std::string(endpoint.port);
    const std::string path = venue::binance::build_stream_path(symbols);

    if (endpoint.use_tls) {
        connect_tls(host, port, path, running, on_message, on_idle);
    } else {
        connect_plain(host, port, path, running, on_message, on_idle);
    }
}

// ---------------------------------------------------------------------------
// REST transport for the depth-snapshot fetch (D10) — same TLS/plain split
// as the WS side, for the same reason: tests hit a local plain-HTTP mock,
// production hits Binance over HTTPS.
// ---------------------------------------------------------------------------

struct ParsedRestUrl {
    std::string host;
    std::string port;
    bool        use_tls;
};

// Only ever applied to our own RestEndpoint constants/test values, never to
// untrusted input — no need for a general-purpose URL parser here.
ParsedRestUrl parse_rest_base_url(std::string_view base_url) {
    const bool       use_tls = base_url.starts_with("https://");
    std::string_view rest    = base_url.substr(use_tls ? 8 : 7);
    if (auto colon = rest.find(':'); colon != std::string_view::npos) {
        return {std::string(rest.substr(0, colon)), std::string(rest.substr(colon + 1)), use_tls};
    }
    return {std::string(rest), use_tls ? "443" : "80", use_tls};
}

std::optional<venue::binance::DepthSnapshot> http_get_json_tls(const std::string& host, const std::string& port,
                                                                 const std::string& target) {
    net::io_context ioc;
    ssl::context    ssl_ctx{ssl::context::tlsv12_client};
    ssl_ctx.set_verify_mode(ssl::verify_peer);
    ssl_ctx.set_default_verify_paths();

    tcp::resolver                         resolver{ioc};
    beast::ssl_stream<beast::tcp_stream> stream{ioc, ssl_ctx};
    stream.set_verify_callback(net::ssl::host_name_verification(host));

    if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
    }

    const auto results = resolver.resolve(host, port);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(10));
    beast::get_lowest_layer(stream).connect(results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "quant-platform-collector/0.1");
    req.prepare_payload();  // sets Content-Length (0) explicitly rather than relying on empty_body's implicit framing
    http::write(stream, req);

    beast::flat_buffer                 buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.shutdown(ec);  // peer often closes abruptly here; not an error we care about

    if (res.result() != http::status::ok) return std::nullopt;
    return venue::binance::parse_depth_snapshot(res.body());
}

std::optional<venue::binance::DepthSnapshot> http_get_json_plain(const std::string& host, const std::string& port,
                                                                   const std::string& target) {
    net::io_context   ioc;
    tcp::resolver     resolver{ioc};
    beast::tcp_stream stream{ioc};

    const auto results = resolver.resolve(host, port);
    stream.expires_after(std::chrono::seconds(10));
    stream.connect(results);

    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "quant-platform-collector/0.1");
    req.prepare_payload();  // sets Content-Length (0) explicitly rather than relying on empty_body's implicit framing
    http::write(stream, req);

    beast::flat_buffer                 buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    if (res.result() != http::status::ok) return std::nullopt;
    return venue::binance::parse_depth_snapshot(res.body());
}

std::optional<venue::binance::DepthSnapshot> fetch_depth_snapshot(venue::binance::RestEndpoint endpoint,
                                                                     const std::string& symbol_name) {
    const auto [host, port, use_tls] = parse_rest_base_url(endpoint.base_url);
    const std::string full_url       = venue::binance::depth_snapshot_url(symbol_name, 1000, endpoint);
    const std::string target         = full_url.substr(std::string(endpoint.base_url).size());

    try {
        return use_tls ? http_get_json_tls(host, port, target) : http_get_json_plain(host, port, target);
    } catch (const std::exception& e) {
        std::cerr << "[LiveWebSocketSource] snapshot fetch failed for " << symbol_name << ": " << e.what()
                  << "\n";
        return std::nullopt;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// LiveWebSocketSource
// ---------------------------------------------------------------------------

LiveWebSocketSource::LiveWebSocketSource(std::vector<std::string> symbols, venue::binance::WsEndpoint ws_endpoint,
                                          venue::binance::RestEndpoint rest_endpoint)
    : symbols_(std::move(symbols)), ws_endpoint_(ws_endpoint), rest_endpoint_(rest_endpoint) {
    io_thread_     = std::thread([this] { run(); });
    resync_thread_ = std::thread([this] { resync_run(); });
}

LiveWebSocketSource::~LiveWebSocketSource() {
    running_.store(false, std::memory_order_release);
    if (io_thread_.joinable()) io_thread_.join();
    if (resync_thread_.joinable()) resync_thread_.join();
}

std::optional<MarketEvent> LiveWebSocketSource::next() { return queue_.pop(); }

std::size_t LiveWebSocketSource::dropped_count() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
}
std::size_t LiveWebSocketSource::gap_count() const noexcept { return gaps_.load(std::memory_order_relaxed); }
std::size_t LiveWebSocketSource::resync_count() const noexcept {
    return resync_count_.load(std::memory_order_relaxed);
}
std::size_t LiveWebSocketSource::resync_retry_count() const noexcept {
    return resync_retry_count_.load(std::memory_order_relaxed);
}
std::size_t LiveWebSocketSource::resync_request_dropped_count() const noexcept {
    return resync_request_dropped_.load(std::memory_order_relaxed);
}
std::size_t LiveWebSocketSource::output_queue_high_water_mark() const noexcept {
    return output_queue_high_water_.load(std::memory_order_relaxed);
}
std::size_t LiveWebSocketSource::resync_request_queue_high_water_mark() const noexcept {
    return resync_request_queue_high_water_.load(std::memory_order_relaxed);
}

// --- I/O thread side ---------------------------------------------------

void LiveWebSocketSource::begin_resync(SymbolId symbol) {
    resync_count_.fetch_add(1, std::memory_order_relaxed);

    ResyncRequest req{symbol, symbol_table_.name(symbol)};
    if (!resync_requests_.push(std::move(req))) {
        resync_request_dropped_.fetch_add(1, std::memory_order_relaxed);
    }
}

void LiveWebSocketSource::drain_resync_responses() {
    while (auto result = resync_responses_.pop()) {
        if (!result->snapshot) {
            resync_retry_count_.fetch_add(1, std::memory_order_relaxed);
            begin_resync(result->symbol);
            continue;
        }

        auto outcome = coordinator_.on_snapshot(result->symbol, *result->snapshot);
        if (outcome.need_retry) {
            // No valid alignment point in what was buffered — the snapshot
            // and the buffer don't overlap correctly. Try again.
            resync_retry_count_.fetch_add(1, std::memory_order_relaxed);
            begin_resync(result->symbol);
            continue;
        }

        // Discontinuities inside the coordinator's own buffer would mean TCP
        // itself delivered out of order, which shouldn't happen — counted
        // rather than assumed, but not recursively resynced over mid-replay.
        gaps_.fetch_add(outcome.internal_gaps, std::memory_order_relaxed);
        for (auto& ev : outcome.to_replay) {
            if (!queue_.push(std::move(ev))) dropped_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void LiveWebSocketSource::on_message(std::string_view msg) {
    drain_resync_responses();  // let a just-arrived snapshot unblock its symbol before this message is processed

    MarketEvent ev;
    if (!venue::binance::parse_message(msg, symbol_table_, ev)) return;

    if (ev.symbol >= kMaxSymbols) {
        // Structurally shouldn't happen — SymbolTable only interns symbols
        // we subscribed to — but don't act on an out-of-range index either.
        return;
    }

    if (ev.kind != EventKind::BookDiff) {
        // Trades aren't sequence-tracked/resynced; forward regardless of
        // the symbol's book-resync state.
        if (!queue_.push(std::move(ev))) dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const SymbolId symbol  = ev.symbol;
    auto            verdict = coordinator_.on_event(std::move(ev));

    switch (verdict.action) {
        case ResyncCoordinator::Action::Forward:
            if (!queue_.push(std::move(*verdict.event))) dropped_.fetch_add(1, std::memory_order_relaxed);
            break;
        case ResyncCoordinator::Action::Buffer:
            break;
        case ResyncCoordinator::Action::BufferAndRequest:
            if (verdict.gap_detected) {
                gaps_.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "[LiveWebSocketSource] sequence gap on " << symbol_table_.name(symbol)
                          << " — resyncing\n";
            }
            begin_resync(symbol);
            break;
    }
}

void LiveWebSocketSource::run() {
    ExponentialBackoff backoff;
    auto                on_message_fn = [this](std::string_view msg) { on_message(msg); };
    auto                on_idle_fn    = [this] { drain_resync_responses(); };

    while (running_.load(std::memory_order_acquire)) {
        try {
            connect_and_stream(symbols_, ws_endpoint_, running_, on_message_fn, on_idle_fn);
            backoff.reset();  // clean session; next disconnect starts fresh
        } catch (const std::exception& e) {
            std::cerr << "[LiveWebSocketSource] connection error: " << e.what() << "\n";
        }

        // A resync can complete right as (or just after) the connection
        // ends — e.g. the server sends its last message and closes with no
        // idle gap in between, so on_idle's polling never gets a chance to
        // fire before run_ws_session returns on ec == closed. Drain here so
        // that data doesn't sit resynced-but-unclaimed if reconnects then
        // keep failing.
        drain_resync_responses();

        if (!running_.load(std::memory_order_acquire)) break;

        auto delay = backoff.next();
        std::cerr << "[LiveWebSocketSource] reconnecting in " << delay.count() << "ms\n";

        // Sleep in short increments, draining between each — a resync that
        // completes mid-backoff (this thread just isn't connected right
        // now, the resync thread runs independently) shouldn't sit
        // unclaimed for up to the full backoff delay.
        static constexpr auto kDrainInterval = std::chrono::milliseconds(100);
        auto                  remaining      = delay;
        while (remaining.count() > 0 && running_.load(std::memory_order_acquire)) {
            auto chunk = std::min(kDrainInterval, remaining);
            std::this_thread::sleep_for(chunk);
            remaining -= chunk;
            drain_resync_responses();
        }
    }
}

// --- Resync thread side --------------------------------------------------

void LiveWebSocketSource::sample_queue_depths() {
    auto update_high_water = [](std::atomic<std::size_t>& hwm, std::size_t observed) {
        std::size_t current = hwm.load(std::memory_order_relaxed);
        while (observed > current && !hwm.compare_exchange_weak(current, observed, std::memory_order_relaxed)) {
        }
    };
    update_high_water(output_queue_high_water_, queue_.size());
    update_high_water(resync_request_queue_high_water_, resync_requests_.size());
}

void LiveWebSocketSource::resync_run() {
    while (running_.load(std::memory_order_acquire)) {
        bool did_work = false;

        while (auto req = resync_requests_.pop()) {
            did_work       = true;
            auto snapshot  = fetch_depth_snapshot(rest_endpoint_, req->symbol_name);
            ResyncResult result{req->symbol, std::move(snapshot)};
            // push() only touches its args on the success path (the
            // full-check happens before construction), so std::move here is
            // safe even inside a retry loop: a failed push leaves `result`
            // untouched, not moved-from.
            while (!resync_responses_.push(std::move(result))) {
                if (!running_.load(std::memory_order_acquire)) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        // Idle time doubles as the sampling tick — no third thread needed
        // just for observability.
        sample_queue_depths();

        if (!did_work) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

}  // namespace qp
