#include "live_websocket_source.hpp"

#include <algorithm>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>
#include <iostream>

#include "backoff.hpp"

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = net::ssl;
using tcp           = net::ip::tcp;

namespace qp::source {

// ---------------------------------------------------------------------------
// WebSocket transport — no Parser dependency at all below run_ws_session and
// connect_tls/connect_plain: they never referenced Binance to begin with,
// only connect_and_stream (which builds the subscribe URL) does.
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
void run_ws_session(websocket::stream<NextLayer>& ws, const std::string& host,
                    const std::string& path, const std::atomic<bool>& running,
                    const OnMessage& on_message, const OnIdle& on_idle) {
    static constexpr auto kPollInterval          = std::chrono::milliseconds(200);
    static constexpr auto kDeadConnectionTimeout = std::chrono::seconds(30);

    ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(beast::http::field::user_agent, "quant-platform-collector/0.1");
    }));

    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(10));
    ws.handshake(host, path);

    beast::flat_buffer buffer;
    auto               last_activity = std::chrono::steady_clock::now();
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
        // flat_buffer's readable region is contiguous and stays valid until
        // buffer.clear() at the top of the next iteration, so on_message
        // (called synchronously, never stores the view) can read it as a
        // string_view directly — buffers_to_string would pay a full-payload
        // heap allocation and copy per message on this loop's hot path for
        // no reason.
        const auto data = buffer.data();
        on_message(std::string_view(static_cast<const char*>(data.data()), data.size()));
    }

    beast::error_code ec;
    ws.close(websocket::close_code::normal, ec);
}

template <class OnMessage, class OnIdle>
void connect_tls(const std::string& host, const std::string& port, const std::string& path,
                 const std::atomic<bool>& running, const OnMessage& on_message,
                 const OnIdle& on_idle) {
    net::io_context ioc;
    ssl::context    ssl_ctx{ssl::context::tlsv12_client};
    ssl_ctx.set_verify_mode(ssl::verify_peer);
    ssl_ctx.set_default_verify_paths();

    tcp::resolver                                           resolver{ioc};
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
                   const std::atomic<bool>& running, const OnMessage& on_message,
                   const OnIdle& on_idle) {
    net::io_context                      ioc;
    tcp::resolver                        resolver{ioc};
    websocket::stream<beast::tcp_stream> ws{ioc};

    const auto results = resolver.resolve(host, port);

    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(10));
    beast::get_lowest_layer(ws).connect(results);

    run_ws_session(ws, host, path, running, on_message, on_idle);
}

template <Parser P, class OnMessage, class OnIdle>
void connect_and_stream(const std::vector<std::string>& symbols, WsEndpoint endpoint,
                        const std::atomic<bool>& running, const OnMessage& on_message,
                        const OnIdle& on_idle) {
    const std::string host = std::string(endpoint.host);
    const std::string port = std::string(endpoint.port);
    const std::string path = P::build_stream_path(symbols);

    if (endpoint.use_tls) {
        connect_tls(host, port, path, running, on_message, on_idle);
    } else {
        connect_plain(host, port, path, running, on_message, on_idle);
    }
}

// ---------------------------------------------------------------------------
// REST transport for the depth-snapshot fetch (D10) — same TLS/plain split
// as the WS side, for the same reason: tests hit a local plain-HTTP mock,
// production hits Binance over HTTPS. The GET itself has no Parser
// dependency (returns the raw body); only fetch_depth_snapshot, which
// builds the request URL and interprets the response, needs one.
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

std::optional<std::string> http_get_tls(const std::string& host, const std::string& port,
                                        const std::string& target) {
    net::io_context ioc;
    ssl::context    ssl_ctx{ssl::context::tlsv12_client};
    ssl_ctx.set_verify_mode(ssl::verify_peer);
    ssl_ctx.set_default_verify_paths();

    tcp::resolver                        resolver{ioc};
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
    req.prepare_payload();  // sets Content-Length (0) explicitly rather than relying on
                            // empty_body's implicit framing
    http::write(stream, req);

    beast::flat_buffer                buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.shutdown(ec);  // peer often closes abruptly here; not an error we care about

    if (res.result() != http::status::ok) return std::nullopt;
    return res.body();
}

std::optional<std::string> http_get_plain(const std::string& host, const std::string& port,
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
    req.prepare_payload();  // sets Content-Length (0) explicitly rather than relying on
                            // empty_body's implicit framing
    http::write(stream, req);

    beast::flat_buffer                buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    if (res.result() != http::status::ok) return std::nullopt;
    return res.body();
}

template <Parser P>
std::optional<DepthSnapshot> fetch_depth_snapshot(RestEndpoint       endpoint,
                                                  const std::string& symbol_name) {
    const auto [host, port, use_tls] = parse_rest_base_url(endpoint.base_url);
    const std::string full_url       = P::depth_snapshot_url(symbol_name, 1000, endpoint);
    const std::string target         = full_url.substr(std::string(endpoint.base_url).size());

    try {
        auto body = use_tls ? http_get_tls(host, port, target) : http_get_plain(host, port, target);
        if (!body) return std::nullopt;
        return P::parse_depth_snapshot(*body);
    } catch (const std::exception& e) {
        std::cerr << "[LiveWebSocketSource] snapshot fetch failed for " << symbol_name << ": "
                  << e.what() << "\n";
        return std::nullopt;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// GenericLiveWebSocketSource
// ---------------------------------------------------------------------------

template <Parser P, AlignmentRule Rule>
GenericLiveWebSocketSource<P, Rule>::GenericLiveWebSocketSource(std::vector<std::string> symbols,
                                                                WsEndpoint   ws_endpoint,
                                                                RestEndpoint rest_endpoint)
    : symbols_(std::move(symbols)), ws_endpoint_(ws_endpoint), rest_endpoint_(rest_endpoint) {
    // Pre-intern up front rather than leaving it to lazy first-message
    // interning: makes SymbolId assignment deterministic (matches symbols_'s
    // order) and, more importantly, makes symbol_table_ logically immutable
    // once the threads start — every later intern() call from parse_message
    // is just a lookup against an already-complete set (the venue only ever
    // sends messages for symbols we subscribed to), never a mutating
    // push_back. That's what makes symbol_names() safe from any thread.
    for (const auto& s : symbols_) symbol_table_.intern(s);

    io_thread_     = std::thread([this] { run(); });
    resync_thread_ = std::thread([this] { resync_run(); });
}

template <Parser P, AlignmentRule Rule>
const std::vector<std::string>& GenericLiveWebSocketSource<P, Rule>::symbol_names() const noexcept {
    return symbol_table_.names();
}

template <Parser P, AlignmentRule Rule>
GenericLiveWebSocketSource<P, Rule>::~GenericLiveWebSocketSource() {
    running_.store(false, std::memory_order_release);
    if (io_thread_.joinable()) io_thread_.join();
    if (resync_thread_.joinable()) resync_thread_.join();
}

template <Parser P, AlignmentRule Rule>
std::optional<MarketEvent> GenericLiveWebSocketSource<P, Rule>::next() {
    return queue_.pop();
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::dropped_count() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::gap_count() const noexcept {
    return gaps_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_count() const noexcept {
    return resync_count_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_retry_count() const noexcept {
    return resync_retry_count_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_rest_failure_count() const noexcept {
    return resync_rest_failure_count_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_buffer_overflow_count() const noexcept {
    return resync_buffer_overflow_count_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_no_alignment_count() const noexcept {
    return resync_no_alignment_count_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_request_dropped_count() const noexcept {
    return resync_request_dropped_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::output_queue_high_water_mark() const noexcept {
    return output_queue_high_water_.load(std::memory_order_relaxed);
}

template <Parser P, AlignmentRule Rule>
std::size_t GenericLiveWebSocketSource<P, Rule>::resync_request_queue_high_water_mark()
    const noexcept {
    return resync_request_queue_high_water_.load(std::memory_order_relaxed);
}

// --- I/O thread side ---------------------------------------------------

template <Parser P, AlignmentRule Rule>
void GenericLiveWebSocketSource<P, Rule>::begin_resync(SymbolId symbol) {
    resync_count_.fetch_add(1, std::memory_order_relaxed);

    ResyncRequest req{symbol, symbol_table_.name(symbol)};
    if (!resync_requests_.push(std::move(req))) {
        resync_request_dropped_.fetch_add(1, std::memory_order_relaxed);
    }
}

template <Parser P, AlignmentRule Rule>
void GenericLiveWebSocketSource<P, Rule>::drain_resync_responses() {
    while (auto result = resync_responses_.pop()) {
        if (!result->snapshot) {
            resync_retry_count_.fetch_add(1, std::memory_order_relaxed);
            resync_rest_failure_count_.fetch_add(1, std::memory_order_relaxed);
            begin_resync(result->symbol);
            continue;
        }

        auto outcome = coordinator_.on_snapshot(result->symbol, result->snapshot->last_update_id,
                                                std::move(result->snapshot->bids),
                                                std::move(result->snapshot->asks));
        if (outcome.need_retry) {
            // No valid alignment point in what was buffered — the snapshot
            // and the buffer don't overlap correctly. Try again. Logged
            // unconditionally (not behind a debug flag) — see D13: this
            // exact "why didn't it align" question needed real
            // instrumentation to answer once already.
            resync_retry_count_.fetch_add(1, std::memory_order_relaxed);
            resync_no_alignment_count_.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[LiveWebSocketSource] resync retry for "
                      << symbol_table_.name(result->symbol) << ": " << outcome.retry_detail << "\n";
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

template <Parser P, AlignmentRule Rule>
void GenericLiveWebSocketSource<P, Rule>::on_message(std::string_view msg) {
    drain_resync_responses();  // let a just-arrived snapshot unblock its symbol before this message
                               // is processed

    // Fresh MarketEvent per message rather than reusing one across calls —
    // pays a bids/asks vector reallocation every message. Known, not yet
    // fixed; see qp_core_bench's BM_Spsc_PushPopMarketEvent, which measures
    // this cost.
    MarketEvent ev;
    if (!P::parse_message(msg, symbol_table_, ev)) return;

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
    auto           verdict = coordinator_.on_event(std::move(ev));

    switch (verdict.action) {
        case ResyncCoordinator<Rule>::Action::Forward:
            if (!queue_.push(std::move(*verdict.event)))
                dropped_.fetch_add(1, std::memory_order_relaxed);
            break;
        case ResyncCoordinator<Rule>::Action::Buffer:
            break;
        case ResyncCoordinator<Rule>::Action::BufferAndRequest:
            if (verdict.gap_detected) {
                gaps_.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "[LiveWebSocketSource] sequence gap on " << symbol_table_.name(symbol)
                          << " — resyncing\n";
            }
            if (verdict.buffer_overflowed) {
                // Was already documented as counting toward resync_retry_count()
                // (see the header) but never actually did until now — the
                // buffer filled before a snapshot arrived, so this restart
                // wasn't a fresh/gap-triggered start.
                resync_retry_count_.fetch_add(1, std::memory_order_relaxed);
                resync_buffer_overflow_count_.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "[LiveWebSocketSource] resync buffer overflowed for "
                          << symbol_table_.name(symbol)
                          << " — snapshot round trip too slow vs. arrival rate\n";
            }
            begin_resync(symbol);
            break;
    }
}

template <Parser P, AlignmentRule Rule>
void GenericLiveWebSocketSource<P, Rule>::run() {
    ExponentialBackoff backoff;
    auto               on_message_fn = [this](std::string_view msg) { on_message(msg); };
    auto               on_idle_fn    = [this] { drain_resync_responses(); };

    while (running_.load(std::memory_order_acquire)) {
        try {
            connect_and_stream<P>(symbols_, ws_endpoint_, running_, on_message_fn, on_idle_fn);
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

template <Parser P, AlignmentRule Rule>
void GenericLiveWebSocketSource<P, Rule>::sample_queue_depths() {
    auto update_high_water = [](std::atomic<std::size_t>& hwm, std::size_t observed) {
        std::size_t current = hwm.load(std::memory_order_relaxed);
        while (observed > current &&
               !hwm.compare_exchange_weak(current, observed, std::memory_order_relaxed)) {
        }
    };
    update_high_water(output_queue_high_water_, queue_.size());
    update_high_water(resync_request_queue_high_water_, resync_requests_.size());
}

template <Parser P, AlignmentRule Rule>
void GenericLiveWebSocketSource<P, Rule>::resync_run() {
    while (running_.load(std::memory_order_acquire)) {
        bool did_work = false;

        while (auto req = resync_requests_.pop()) {
            did_work              = true;
            auto         snapshot = fetch_depth_snapshot<P>(rest_endpoint_, req->symbol_name);
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

// See live_websocket_source.hpp's extern template declarations — one per
// combination actually used (futures: apps/collector's default leg; spot:
// apps/collector's second leg, D41).
template class GenericLiveWebSocketSource<
    venue::binance::BinanceParser<venue::binance::FuturesMarket>, FuturesAlignment>;
template class GenericLiveWebSocketSource<venue::binance::BinanceParser<venue::binance::SpotMarket>,
                                          SpotAlignment>;

}  // namespace qp::source
