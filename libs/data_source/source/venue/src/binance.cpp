#include "binance.hpp"

#include <simdjson.h>

#include <algorithm>
#include <cctype>
#include <charconv>

namespace qp::venue::binance {

namespace {

// nullopt on any unparsed/trailing content — 0.0 is a valid price/qty (and
// qty==0 is the book-level-removal sentinel), so silently defaulting a
// malformed field to 0.0 would be misread as legitimate data, not rejected.
std::optional<double> to_double(std::string_view sv) {
    double v             = 0.0;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
    return v;
}

std::string lower(std::string_view sv) {
    std::string s{sv};
    std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string upper(std::string_view sv) {
    std::string s{sv};
    std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

// Reads a [price, qty] pair out of a depthUpdate "b"/"a" level array.
bool read_level(simdjson::ondemand::array pair, PriceLevel& lvl) {
    std::string_view px, qty;
    bool             have_px = false, have_qty = false;
    for (auto field : pair) {
        std::string_view sv;
        if (field.get_string().get(sv) != simdjson::SUCCESS) return false;
        if (!have_px) {
            px      = sv;
            have_px = true;
        } else {
            qty      = sv;
            have_qty = true;
            break;
        }
    }
    if (!have_px || !have_qty) return false;  // truncated pair — reject, don't default qty to 0
    auto p = to_double(px);
    auto q = to_double(qty);
    if (!p || !q) return false;
    lvl = {*p, *q};
    return true;
}

// Shared by parse_depth_snapshot's bids/asks and parse_message's depthUpdate
// bids/asks — same "read a JSON array of [price,qty] pairs" shape, only the
// field name and destination vector differ. `obj` is either the top-level
// `document` (snapshot) or the `"data"` value (depthUpdate).
template <class Json>
void read_levels(Json&& obj, std::string_view field, std::vector<PriceLevel>& out) {
    simdjson::ondemand::array arr;
    if (obj[field].get(arr) != simdjson::SUCCESS) return;
    for (auto level : arr) {
        simdjson::ondemand::array pair;
        PriceLevel                lvl;
        if (level.get(pair) == simdjson::SUCCESS && read_level(pair, lvl)) out.push_back(lvl);
    }
}

}  // namespace

// SymbolTable is fully inline in venue_types.hpp (aliased here
// as venue::binance::SymbolTable) — nothing to define out-of-line here.

std::string build_stream_path(const std::vector<std::string>& symbols,
                              std::string_view                depth_speed) {
    std::string path  = "/stream?streams=";
    bool        first = true;
    for (const auto& raw : symbols) {
        const std::string s = lower(raw);
        if (!first) path += '/';
        path +=
            s + "@depth@" + std::string(depth_speed) + "/" + s + "@aggTrade/" + s + "@markPrice";
        first = false;
    }
    return path;
}

std::string build_stream_url(const std::vector<std::string>& symbols, WsEndpoint endpoint,
                             std::string_view depth_speed) {
    return "wss://" + std::string(endpoint.host) + build_stream_path(symbols, depth_speed);
}

std::string depth_snapshot_url(std::string_view symbol, int limit, RestEndpoint endpoint) {
    return std::string(endpoint.base_url) + "/fapi/v1/depth?symbol=" + upper(symbol) +
           "&limit=" + std::to_string(limit);
}

std::optional<DepthSnapshot> parse_depth_snapshot(std::string_view json_body) {
    try {
        simdjson::padded_string      padded(json_body);
        simdjson::ondemand::parser   parser;
        simdjson::ondemand::document doc = parser.iterate(padded);

        DepthSnapshot snapshot;
        if (doc["lastUpdateId"].get(snapshot.last_update_id) != simdjson::SUCCESS)
            return std::nullopt;

        read_levels(doc, "bids", snapshot.bids);
        read_levels(doc, "asks", snapshot.asks);

        return snapshot;
    } catch (const simdjson::simdjson_error&) {
        return std::nullopt;
    }
}

bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out) {
    // Reused across calls (thread_local, not a member — this is a free
    // function called from exactly one I/O thread per source instance): a
    // fresh simdjson::ondemand::parser starts with zero internal buffer/tape
    // capacity, so allocating one per message forces a grow-from-scratch on
    // every single WS message instead of amortizing it across the
    // connection's lifetime — this runs on the highest-frequency path in the
    // collector.
    static thread_local simdjson::ondemand::parser parser;
    try {
        simdjson::padded_string      padded(msg);
        simdjson::ondemand::document doc = parser.iterate(padded);

        simdjson::ondemand::value data;
        if (doc["data"].get(data) != simdjson::SUCCESS) return false;  // not a stream payload

        std::string_view event_type;
        if (data["e"].get(event_type) != simdjson::SUCCESS) return false;

        std::string_view sym_str;
        if (data["s"].get(sym_str) != simdjson::SUCCESS) return false;
        out.symbol = symbols.intern(sym_str);

        if (event_type == "depthUpdate") {
            out.kind = EventKind::BookDiff;

            std::int64_t event_time_ms = 0;
            data["E"].get(event_time_ms);
            out.ts = event_time_ms * 1'000'000;  // ms -> ns

            std::uint64_t U = 0, u = 0, pu = 0;
            data["U"].get(U);
            if (data["u"].get(u) != simdjson::SUCCESS) return false;
            data["pu"].get(pu);
            out.first_seq = U;
            out.seq       = u;
            out.prev_seq  = pu;

            out.bids.clear();
            read_levels(data, "b", out.bids);

            out.asks.clear();
            read_levels(data, "a", out.asks);
            return true;
        }

        if (event_type == "aggTrade") {
            out.kind = EventKind::Trade;

            std::int64_t trade_time_ms = 0;
            data["T"].get(trade_time_ms);
            out.ts = trade_time_ms * 1'000'000;

            std::uint64_t agg_id = 0;
            data["a"].get(agg_id);
            out.seq = agg_id;

            std::string_view px, qty;
            if (data["p"].get(px) != simdjson::SUCCESS) return false;
            if (data["q"].get(qty) != simdjson::SUCCESS) return false;
            auto price    = to_double(px);
            auto quantity = to_double(qty);
            if (!price || !quantity) return false;
            out.price = *price;
            out.qty   = *quantity;

            bool is_buyer_maker = false;
            data["m"].get(is_buyer_maker);
            out.side = is_buyer_maker ? Side::Sell : Side::Buy;  // taker side
            return true;
        }

        if (event_type == "markPriceUpdate") {
            out.kind = EventKind::Funding;

            std::int64_t event_time_ms = 0;
            data["E"].get(event_time_ms);
            out.ts = event_time_ms * 1'000'000;

            std::string_view mark_px, rate;
            if (data["p"].get(mark_px) != simdjson::SUCCESS) return false;
            if (data["r"].get(rate) != simdjson::SUCCESS) return false;
            auto mp = to_double(mark_px);
            auto fr = to_double(rate);
            if (!mp || !fr) return false;
            out.mark_price   = *mp;
            out.funding_rate = *fr;
            return true;
        }

        return false;  // unrecognized event type (forceOrder, etc. — not wired up)
    } catch (const simdjson::simdjson_error&) {
        return false;
    }
}

}  // namespace qp::venue::binance
