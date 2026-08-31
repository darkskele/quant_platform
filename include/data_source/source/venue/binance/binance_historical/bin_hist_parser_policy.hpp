#pragma once
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "types.hpp"
#include "venue.hpp"

namespace qp::data_source::source::venue::binance::binance_historical {

/// binance_historical's Parser policy — the venue-specific half of
/// venue::Parser<BinHistParserPolicy, Table> (venue.hpp owns the generic
/// shell + symbol table this uses). Not templated on Table itself
/// (parse<Table> is a member template instead) so this stays
/// nameable/testable independent of any one symbol table.
///
/// Hand-rolled field extraction for both record kinds, deliberately no
/// JSON library for the funding side: a funding entry is a flat, fixed
/// 4-field object — no nesting, no arrays, no escaped characters in any
/// field this venue actually sends (verified against a live
/// fapi.binance.com/fapi/v1/fundingRate response) — small enough that a
/// couple of substring searches beats paying a general parser's per-call
/// setup cost for something this size (measured: simdjson's ondemand
/// parser, reconstructed per call plus a double copy to get a padded
/// buffer, cost ~400ns/call here vs. this version — see
/// bench_bin_hist_parser_policy.cpp for the isolated numbers).
///
/// Dispatches on the raw text's shape, not a tag Source attaches: the two
/// record kinds this venue has are syntactically unambiguous (funding
/// entries are a JSON object, `{...}`; kline rows are plain CSV, never
/// start with `{`) — verified against real Binance responses
/// (fapi.binance.com/fapi/v1/fundingRate and data.binance.vision's daily
/// kline CSVs), not assumed. Garbage that matches neither shape falls
/// through to whichever branch its first character picked and fails there
/// (missing/unparseable fields, unknown symbol) rather than needing a
/// dedicated "malformed" path of its own.
struct BinHistParserPolicy {
    template <SymbolTable Table>
    static std::optional<MarketEvent> parse(std::span<const std::byte> raw) {
        std::string_view text{reinterpret_cast<const char*>(raw.data()), raw.size()};
        std::size_t      start = text.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return std::nullopt;

        if (text[start] == '{') return parse_funding<Table>(text.substr(start));
        return parse_kline<Table>(text.substr(start));
    }

   private:
    /// std::from_chars wrapper — locale-independent, no allocation, and (per
    /// the standard) doesn't require a null-terminated string the way
    /// std::stod does, so it works directly on a substring view without
    /// needing to copy it out first.
    static std::optional<double> parse_decimal(std::string_view field) {
        double value{};
        auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value);
        if (ec != std::errc{} || ptr != field.data() + field.size()) return std::nullopt;
        return value;
    }

    /// Finds `"key":` in text and returns the quoted string value that
    /// follows — zero-copy (a view into `text`, not an allocation).
    /// Searches by key name rather than assuming position: unlike
    /// parse_kline's CSV columns, JSON key order isn't a real contract,
    /// even though every real capture seen so far happens to agree on one.
    static std::optional<std::string_view> find_string(std::string_view text,
                                                       std::string_view quoted_key) {
        auto pos = text.find(quoted_key);
        if (pos == std::string_view::npos) return std::nullopt;
        pos += quoted_key.size();
        if (pos < text.size() && text[pos] == ' ') ++pos;  // tolerate "key": "value"
        if (pos >= text.size() || text[pos] != '"') return std::nullopt;
        ++pos;
        auto end = text.find('"', pos);
        if (end == std::string_view::npos) return std::nullopt;
        return text.substr(pos, end - pos);
    }

    /// Same idea as find_string, but for a bare numeric value
    /// (fundingTime is the only such field a funding entry carries) —
    /// std::from_chars stops at the first non-numeric character on its
    /// own, so no separate end-of-token scan is needed before parsing.
    static std::optional<std::int64_t> find_int(std::string_view text,
                                                std::string_view quoted_key) {
        auto pos = text.find(quoted_key);
        if (pos == std::string_view::npos) return std::nullopt;
        pos += quoted_key.size();
        if (pos < text.size() && text[pos] == ' ') ++pos;
        std::int64_t value{};
        auto [ptr, ec] = std::from_chars(text.data() + pos, text.data() + text.size(), value);
        if (ec != std::errc{}) return std::nullopt;
        return value;
    }

    /// Same idea again, but for a quoted decimal value (fundingRate/
    /// markPrice arrive as JSON strings, not bare numbers). Skips the
    /// separate closing-quote scan find_string would need: from_chars
    /// already reports where the number stopped, so checking that stop
    /// point is a `"` both confirms clean termination and does the job of
    /// that second scan for free.
    static std::optional<double> find_decimal(std::string_view text, std::string_view quoted_key) {
        auto pos = text.find(quoted_key);
        if (pos == std::string_view::npos) return std::nullopt;
        pos += quoted_key.size();
        if (pos < text.size() && text[pos] == ' ') ++pos;
        if (pos >= text.size() || text[pos] != '"') return std::nullopt;
        ++pos;
        double      value{};
        const char* end = text.data() + text.size();
        auto [ptr, ec]  = std::from_chars(text.data() + pos, end, value);
        if (ec != std::errc{} || ptr == end || *ptr != '"') return std::nullopt;
        return value;
    }

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
        auto symbol_str = find_string(text, R"("symbol":)");
        if (!symbol_str) return std::nullopt;
        auto id = Table::id_of(*symbol_str);
        if (!id) return std::nullopt;

        auto funding_time_ms = find_int(text, R"("fundingTime":)");
        auto rate            = find_decimal(text, R"("fundingRate":)");
        auto mark            = find_decimal(text, R"("markPrice":)");
        if (!funding_time_ms || !rate || !mark) return std::nullopt;

        return FundingEvent{
            .symbol       = *id,
            .ts           = *funding_time_ms * 1'000'000,
            .mark_price   = *mark,
            .funding_rate = *rate,
        };
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

        auto open_time  = parse_decimal(fields[1]);
        auto open       = parse_decimal(fields[2]);
        auto high       = parse_decimal(fields[3]);
        auto low        = parse_decimal(fields[4]);
        auto close      = parse_decimal(fields[5]);
        auto volume     = parse_decimal(fields[6]);
        auto close_time = parse_decimal(fields[7]);
        if (!open_time || !open || !high || !low || !close || !volume || !close_time) {
            return std::nullopt;
        }

        return KlineEvent{
            .symbol     = *id,
            .ts         = static_cast<Timestamp>(*open_time) * 1'000'000,
            .close_time = static_cast<Timestamp>(*close_time) * 1'000'000,
            .open       = *open,
            .high       = *high,
            .low        = *low,
            .close      = *close,
            .volume     = *volume,
        };
    }
};

}  // namespace qp::data_source::source::venue::binance::binance_historical
