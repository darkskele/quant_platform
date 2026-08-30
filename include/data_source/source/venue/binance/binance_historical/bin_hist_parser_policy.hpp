#pragma once
#include <simdjson.h>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "types.hpp"
#include "venue.hpp"

namespace qp::data_source::source::venue::binance::binance_historical {

/// binance_historical's Parser policy — the venue-specific half of
/// venue::Parser<BinHistParserPolicy, Table> (venue.hpp owns the generic
/// shell + symbol table + byte utilities this uses). Not templated on
/// Table itself (parse<Table> is a member template instead) so this stays
/// nameable/testable independent of any one symbol table.
///
/// Dispatches on the raw text's shape, not a tag Source attaches: the two
/// record kinds this venue has are syntactically unambiguous (funding
/// entries are a JSON object, `{...}`; kline rows are plain CSV, never
/// start with `{`) — verified against real Binance responses
/// (fapi.binance.com/fapi/v1/fundingRate and data.binance.vision's daily
/// kline CSVs), not assumed.
struct BinHistParserPolicy {
    template <SymbolTable Table>
    static std::optional<MarketEvent> parse(std::span<const std::byte> raw) {
        std::string_view text  = bytes::as_text(raw);
        std::size_t      start = text.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return std::nullopt;

        if (text[start] == '{') return parse_funding<Table>(text.substr(start));
        return parse_kline<Table>(text.substr(start));
    }

   private:
    /// One JSON object per call — Source hands over a single funding
    /// entry, not the whole REST array response (parse() takes one
    /// packet/message, per venue::Parser's own contract). Self-describing
    /// (the JSON carries its own "symbol" field): verified against a live
    /// fapi.binance.com/fapi/v1/fundingRate response, e.g.
    /// {"symbol":"BTCUSDT","fundingTime":1787990400000,
    ///  "fundingRate":"0.00010000","markPrice":"77597.93110145",
    ///  "rateType":"Regular"} — fundingTime is milliseconds, like every
    ///  other Binance timestamp; MarketEvent::ts is nanoseconds (D26-style
    ///  unit note in types.hpp), hence the *1'000'000 below.
    template <SymbolTable Table>
    static std::optional<MarketEvent> parse_funding(std::string_view text) {
        simdjson::padded_string    padded{std::string(text)};
        simdjson::ondemand::parser parser;
        auto                       doc = parser.iterate(padded);
        auto                       obj = doc.get_object();
        if (obj.error()) return std::nullopt;

        std::string_view symbol_str;
        if (obj["symbol"].get_string().get(symbol_str)) return std::nullopt;
        auto id = Table::id_of(symbol_str);
        if (!id) return std::nullopt;

        std::string_view rate_str;
        std::string_view mark_str;
        if (obj["fundingRate"].get_string().get(rate_str)) return std::nullopt;
        if (obj["markPrice"].get_string().get(mark_str)) return std::nullopt;
        auto rate = bytes::parse_decimal(rate_str);
        auto mark = bytes::parse_decimal(mark_str);
        if (!rate || !mark) return std::nullopt;

        std::int64_t funding_time_ms;
        if (obj["fundingTime"].get_int64().get(funding_time_ms)) return std::nullopt;

        FundingEvent ev;
        ev.symbol       = *id;
        ev.ts           = funding_time_ms * 1'000'000;
        ev.funding_rate = *rate;
        ev.mark_price   = *mark;
        return ev;
    }

    /// Binance's historical kline CSV row carries no symbol field at all
    /// (verified against real data.binance.vision daily klines, both
    /// futures and spot: open_time,open,high,low,close,volume,close_time,
    /// quote_volume,count,taker_buy_volume,taker_buy_quote_volume,ignore —
    /// symbol is determined entirely by which file you're reading) — so
    /// Source has to attach it somewhere for this to be self-describing,
    /// per venue::Parser's contract. Convention: Source prepends the
    /// symbol as an extra leading CSV field, so a row here looks like
    /// SYMBOL,open_time,open,high,low,close,volume,close_time,... — this
    /// is Source's format to produce, not decided by FileReplaySource yet
    /// (still to be rewritten), so this convention may need to change
    /// alongside that rewrite.
    template <SymbolTable Table>
    static std::optional<MarketEvent> parse_kline(std::string_view text) {
        std::array<std::string_view, 8> fields{};
        std::size_t                     count = 0;
        std::size_t                     pos   = 0;
        while (count < fields.size()) {
            std::size_t comma = text.find(',', pos);
            fields[count++]   = text.substr(
                pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }
        if (count < fields.size()) return std::nullopt;

        auto id = Table::id_of(fields[0]);
        if (!id) return std::nullopt;

        auto open_time  = bytes::parse_decimal(fields[1]);
        auto open       = bytes::parse_decimal(fields[2]);
        auto high       = bytes::parse_decimal(fields[3]);
        auto low        = bytes::parse_decimal(fields[4]);
        auto close      = bytes::parse_decimal(fields[5]);
        auto volume     = bytes::parse_decimal(fields[6]);
        auto close_time = bytes::parse_decimal(fields[7]);
        if (!open_time || !open || !high || !low || !close || !volume || !close_time) {
            return std::nullopt;
        }

        KlineEvent ev;
        ev.symbol     = *id;
        ev.ts         = static_cast<Timestamp>(*open_time) * 1'000'000;  // ms -> ns
        ev.close_time = static_cast<Timestamp>(*close_time) * 1'000'000;
        ev.open       = *open;
        ev.high       = *high;
        ev.low        = *low;
        ev.close      = *close;
        ev.volume     = *volume;
        return ev;
    }
};

}  // namespace qp::data_source::source::venue::binance::binance_historical
