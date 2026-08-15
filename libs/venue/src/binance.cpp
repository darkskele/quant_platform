#include "qp/venue/binance.hpp"

#include <simdjson.h>

#include <algorithm>
#include <cctype>
#include <charconv>

namespace qp::venue::binance {

namespace {

double to_double(std::string_view sv) {
    double v = 0.0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
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
    bool             have_px = false;
    for (auto field : pair) {
        std::string_view sv;
        if (field.get_string().get(sv) != simdjson::SUCCESS) return false;
        if (!have_px) {
            px      = sv;
            have_px = true;
        } else {
            qty = sv;
            break;
        }
    }
    if (!have_px) return false;
    lvl = {to_double(px), to_double(qty)};
    return true;
}

}  // namespace

SymbolId SymbolTable::intern(const std::string& sym) {
    for (SymbolId i = 0; i < names_.size(); ++i)
        if (names_[i] == sym) return i;
    names_.push_back(sym);
    return static_cast<SymbolId>(names_.size() - 1);
}

const std::string& SymbolTable::name(SymbolId id) const { return names_.at(id); }

std::string build_stream_path(const std::vector<std::string>& symbols, std::string_view depth_speed) {
    std::string path = "/stream?streams=";
    bool        first = true;
    for (const auto& raw : symbols) {
        const std::string s = lower(raw);
        if (!first) path += '/';
        path += s + "@depth@" + std::string(depth_speed) + "/" + s + "@aggTrade";
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
        if (doc["lastUpdateId"].get(snapshot.last_update_id) != simdjson::SUCCESS) return std::nullopt;

        simdjson::ondemand::array bids;
        if (doc["bids"].get(bids) == simdjson::SUCCESS) {
            for (auto level : bids) {
                simdjson::ondemand::array pair;
                PriceLevel                lvl;
                if (level.get(pair) == simdjson::SUCCESS && read_level(pair, lvl)) snapshot.bids.push_back(lvl);
            }
        }

        simdjson::ondemand::array asks;
        if (doc["asks"].get(asks) == simdjson::SUCCESS) {
            for (auto level : asks) {
                simdjson::ondemand::array pair;
                PriceLevel                lvl;
                if (level.get(pair) == simdjson::SUCCESS && read_level(pair, lvl)) snapshot.asks.push_back(lvl);
            }
        }

        return snapshot;
    } catch (const simdjson::simdjson_error&) {
        return std::nullopt;
    }
}

bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out) {
    try {
        simdjson::padded_string      padded(msg);
        simdjson::ondemand::parser   parser;
        simdjson::ondemand::document doc = parser.iterate(padded);

        simdjson::ondemand::value data;
        if (doc["data"].get(data) != simdjson::SUCCESS) return false;  // not a stream payload

        std::string_view event_type;
        if (data["e"].get(event_type) != simdjson::SUCCESS) return false;

        std::string_view sym_str;
        if (data["s"].get(sym_str) != simdjson::SUCCESS) return false;
        out.symbol = symbols.intern(std::string(sym_str));

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
            simdjson::ondemand::array bids;
            if (data["b"].get(bids) == simdjson::SUCCESS) {
                for (auto level : bids) {
                    simdjson::ondemand::array pair;
                    PriceLevel                lvl;
                    if (level.get(pair) == simdjson::SUCCESS && read_level(pair, lvl))
                        out.bids.push_back(lvl);
                }
            }

            out.asks.clear();
            simdjson::ondemand::array asks;
            if (data["a"].get(asks) == simdjson::SUCCESS) {
                for (auto level : asks) {
                    simdjson::ondemand::array pair;
                    PriceLevel                lvl;
                    if (level.get(pair) == simdjson::SUCCESS && read_level(pair, lvl))
                        out.asks.push_back(lvl);
                }
            }
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
            data["p"].get(px);
            data["q"].get(qty);
            out.price = to_double(px);
            out.qty   = to_double(qty);

            bool is_buyer_maker = false;
            data["m"].get(is_buyer_maker);
            out.side = is_buyer_maker ? Side::Sell : Side::Buy;  // taker side
            return true;
        }

        return false;  // unrecognized event type (markPrice, etc. — not wired up yet)
    } catch (const simdjson::simdjson_error&) {
        return false;
    }
}

}  // namespace qp::venue::binance
