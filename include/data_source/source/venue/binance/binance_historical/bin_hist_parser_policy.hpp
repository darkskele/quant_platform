#pragma once
#include <array>
#include <charconv>
#include <cstddef>
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
/// All three record kinds are plain CSV (verified against real
/// data.binance.vision dumps: klines, markPriceKlines, and — despite the
/// live REST API's JSON shape — even historical funding is CSV,
/// `calc_time,funding_interval_hours,last_funding_rate`). None of these
/// files carry a symbol column (it's implicit in which file you're
/// reading), and klines/markPriceKlines are byte-identical in shape
/// besides — nothing in the bytes alone can tell a trade-price bar from a
/// mark-price bar. So this venue's ingestion pipeline prepends
/// `SYMBOL,KIND,` to every raw line before it ever reaches this parser
/// (KIND one of `K`/`M`/`F` — see the tagging script, not FileReplaySource
/// or any other Source: making the files self-describing once, offline,
/// keeps venue::Parser's own one-argument `parse(raw)` contract intact
/// rather than needing Source to pass a kind out-of-band on every call).
/// parse() reads that prefix and dispatches on the tag; garbage that
/// matches no tag falls through to nullopt, same as any other malformed
/// line.
struct BinHistParserPolicy {
    template <SymbolTable Table>
    static std::optional<MarketEvent> parse(std::span<const std::byte> raw) {
        std::string_view text{reinterpret_cast<const char*>(raw.data()), raw.size()};
        std::size_t      start = text.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return std::nullopt;
        text.remove_prefix(start);

        auto symbol_end = text.find(',');
        if (symbol_end == std::string_view::npos) return std::nullopt;
        auto id = Table::id_of(text.substr(0, symbol_end));
        if (!id) return std::nullopt;
        text.remove_prefix(symbol_end + 1);

        auto kind_end = text.find(',');
        if (kind_end == std::string_view::npos) return std::nullopt;
        std::string_view kind = text.substr(0, kind_end);
        text.remove_prefix(kind_end + 1);

        if (kind == "K") return parse_kline(*id, text);
        if (kind == "M") return parse_mark(*id, text);
        if (kind == "F") return parse_funding(*id, text);
        return std::nullopt;
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

    /// Splits `text` into exactly N comma-delimited fields, left to right —
    /// nullopt if there aren't enough (a truncated/malformed row), extra
    /// trailing columns beyond the Nth simply never get looked at (real
    /// kline rows carry 5 more columns after close_time that no event kind
    /// here needs).
    template <std::size_t N>
    static std::optional<std::array<std::string_view, N>> split_fields(std::string_view text) {
        std::array<std::string_view, N> fields{};
        std::size_t                     count = 0;
        std::size_t                     pos   = 0;
        while (count < N) {
            std::size_t comma = text.find(',', pos);
            fields[count++]   = text.substr(
                pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }
        if (count < N) return std::nullopt;
        return fields;
    }

    /// open_time,open,high,low,close,volume,close_time — the first 7 of a
    /// real kline row's 12 columns (quote_volume/count/taker_buy_*/ignore
    /// unused, never even split out). fundingTime-style ms->ns conversion
    /// (D26-style unit note in types.hpp): every Binance timestamp is
    /// milliseconds, MarketEvent::ts is nanoseconds.
    static std::optional<MarketEvent> parse_kline(SymbolId id, std::string_view text) {
        auto fields = split_fields<7>(text);
        if (!fields) return std::nullopt;
        auto open_time  = parse_decimal((*fields)[0]);
        auto open       = parse_decimal((*fields)[1]);
        auto high       = parse_decimal((*fields)[2]);
        auto low        = parse_decimal((*fields)[3]);
        auto close      = parse_decimal((*fields)[4]);
        auto volume     = parse_decimal((*fields)[5]);
        auto close_time = parse_decimal((*fields)[6]);
        if (!open_time || !open || !high || !low || !close || !volume || !close_time) {
            return std::nullopt;
        }

        return KlineEvent{
            .symbol     = id,
            .ts         = static_cast<Timestamp>(*open_time) * 1'000'000,
            .close_time = static_cast<Timestamp>(*close_time) * 1'000'000,
            .open       = *open,
            .high       = *high,
            .low        = *low,
            .close      = *close,
            .volume     = *volume,
        };
    }

    /// Same 7-column shape as parse_kline (Binance publishes markPriceKlines
    /// in the identical CSV layout) — volume (field 5) is always 0 for mark
    /// price (not a traded quantity), so it's split out like the rest but
    /// never stored.
    static std::optional<MarketEvent> parse_mark(SymbolId id, std::string_view text) {
        auto fields = split_fields<7>(text);
        if (!fields) return std::nullopt;
        auto open_time  = parse_decimal((*fields)[0]);
        auto open       = parse_decimal((*fields)[1]);
        auto high       = parse_decimal((*fields)[2]);
        auto low        = parse_decimal((*fields)[3]);
        auto close      = parse_decimal((*fields)[4]);
        auto close_time = parse_decimal((*fields)[6]);
        if (!open_time || !open || !high || !low || !close || !close_time) return std::nullopt;

        return MarkPriceKlineEvent{
            .symbol     = id,
            .ts         = static_cast<Timestamp>(*open_time) * 1'000'000,
            .close_time = static_cast<Timestamp>(*close_time) * 1'000'000,
            .open       = *open,
            .high       = *high,
            .low        = *low,
            .close      = *close,
        };
    }

    /// calc_time,funding_interval_hours,last_funding_rate — the real
    /// historical shape (verified against a downloaded
    /// BTCUSDT-fundingRate-*.csv), not the live REST API's
    /// {"symbol":...,"fundingRate":...,"markPrice":...} JSON this used to
    /// assume. funding_interval_hours (field 1) isn't carried on
    /// FundingEvent — nothing here currently varies it per record.
    static std::optional<MarketEvent> parse_funding(SymbolId id, std::string_view text) {
        auto fields = split_fields<3>(text);
        if (!fields) return std::nullopt;
        auto calc_time = parse_decimal((*fields)[0]);
        auto rate      = parse_decimal((*fields)[2]);
        if (!calc_time || !rate) return std::nullopt;

        return FundingEvent{
            .symbol       = id,
            .ts           = static_cast<Timestamp>(*calc_time) * 1'000'000,
            .funding_rate = *rate,
        };
    }
};

}  // namespace qp::data_source::source::venue::binance::binance_historical
