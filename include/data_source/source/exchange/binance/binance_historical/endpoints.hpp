#pragma once
#include <array>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace qp::data_source::source::exchange::binance {

/// Host serving the data files.
inline constexpr std::string_view kDataHost = "https://data.binance.vision";

/// Host serving the S3 bucket listing. The data host does not answer list queries.
inline constexpr std::string_view kListingHost =
    "https://s3-ap-northeast-1.amazonaws.com/data.binance.vision";

/// Doubles as Binance's market slot numbering in a Subscription, so a slot maps
/// to its path without anybody restating it.
enum class BinanceMarket : std::uint8_t { Spot = 0, UsdM = 1, CoinM = 2 };

/// Null when the slot is not one of Binance's markets.
inline constexpr const BinanceMarket* market_of_slot(std::uint16_t slot) noexcept {
    static constexpr BinanceMarket kMarkets[] = {BinanceMarket::Spot, BinanceMarket::UsdM,
                                                 BinanceMarket::CoinM};
    return slot < std::size(kMarkets) ? &kMarkets[slot] : nullptr;
}

enum class EndpointKind : std::uint8_t { Klines, MarkPriceKlines, PremiumIndexKlines, FundingRate };

enum class Cadence : std::uint8_t { Daily, Monthly };

enum class CadenceSupport : std::uint8_t { DailyOnly, MonthlyOnly, Both };

/// One historical dataset, enough to build any of its prefixes and file names.
struct Endpoint {
    std::string_view market_path;
    std::string_view kind_path;
    CadenceSupport   cadence;
    bool             intervalled;
    std::uint8_t     column_count;

    /// Column holding volume in base asset units. Coin-M is inverse, so its
    /// column 5 counts contracts and column 7 is the base asset.
    std::uint8_t volume_column;
};

/// Verified against live files. Header row and timestamp unit are absent here
/// because both vary by file date within a single dataset. Sniff them per file.
inline constexpr std::array kEndpoints = {
    Endpoint{"spot", "klines", CadenceSupport::Both, true, 12, 5},
    Endpoint{"futures/um", "klines", CadenceSupport::Both, true, 12, 5},
    Endpoint{"futures/um", "markPriceKlines", CadenceSupport::Both, true, 12, 5},
    Endpoint{"futures/um", "premiumIndexKlines", CadenceSupport::Both, true, 12, 5},
    Endpoint{"futures/um", "fundingRate", CadenceSupport::MonthlyOnly, false, 3, 0},
    // Coin-M is inverse, so its klines count contracts in column 5 and carry
    // the base asset in column 7. Reading 7 keeps volume one unit everywhere
    // and avoids needing per symbol contract sizes.
    Endpoint{"futures/cm", "klines", CadenceSupport::Both, true, 12, 7},
    Endpoint{"futures/cm", "markPriceKlines", CadenceSupport::Both, true, 12, 5},
    Endpoint{"futures/cm", "premiumIndexKlines", CadenceSupport::Both, true, 12, 5},
    Endpoint{"futures/cm", "fundingRate", CadenceSupport::MonthlyOnly, false, 3, 0},
};

/// Null when the market does not publish that dataset, such as spot funding.
inline constexpr const Endpoint* find_endpoint(BinanceMarket market, EndpointKind kind) noexcept {
    const std::string_view market_path = [market] {
        switch (market) {
            case BinanceMarket::Spot:
                return "spot";
            case BinanceMarket::UsdM:
                return "futures/um";
            case BinanceMarket::CoinM:
                return "futures/cm";
        }
        return "";
    }();
    const std::string_view kind_path = [kind] {
        switch (kind) {
            case EndpointKind::Klines:
                return "klines";
            case EndpointKind::MarkPriceKlines:
                return "markPriceKlines";
            case EndpointKind::PremiumIndexKlines:
                return "premiumIndexKlines";
            case EndpointKind::FundingRate:
                return "fundingRate";
        }
        return "";
    }();

    for (const auto& e : kEndpoints)
        if (e.market_path == market_path && e.kind_path == kind_path) return &e;
    return nullptr;
}

/// Throws if the pair has no dataset.
inline constexpr const Endpoint& endpoint(BinanceMarket market, EndpointKind kind) {
    const auto* found = find_endpoint(market, kind);
    if (found == nullptr) throw std::out_of_range("binance: no endpoint for this market and kind");
    return *found;
}

inline constexpr bool supports(const Endpoint& e, Cadence cadence) {
    switch (e.cadence) {
        case CadenceSupport::DailyOnly:
            return cadence == Cadence::Daily;
        case CadenceSupport::MonthlyOnly:
            return cadence == Cadence::Monthly;
        case CadenceSupport::Both:
            return true;
    }
    return false;
}

/// The cadence to fall back to when the asked for one is not published. Both
/// prefers monthly, since it is fewer requests for the same rows.
inline constexpr Cadence fallback_cadence(const Endpoint& e) noexcept {
    return e.cadence == CadenceSupport::DailyOnly ? Cadence::Daily : Cadence::Monthly;
}

/// Key prefix holding every file for one stream. Interval is ignored when the
/// endpoint does not carry one.
inline std::string prefix(const Endpoint& e, std::string_view symbol, std::string_view interval,
                          Cadence cadence) {
    std::string out;
    out.reserve(64);
    out += "data/";
    out += e.market_path;
    out += cadence == Cadence::Daily ? "/daily/" : "/monthly/";
    out += e.kind_path;
    out += '/';
    out += symbol;
    out += '/';
    if (e.intervalled) {
        out += interval;
        out += '/';
    }
    return out;
}

/// stamp is YYYY-MM-DD for daily and YYYY-MM for monthly.
inline std::string file_name(const Endpoint& e, std::string_view symbol, std::string_view interval,
                             std::string_view stamp) {
    std::string out;
    out.reserve(48);
    out += symbol;
    out += '-';
    out += e.intervalled ? interval : e.kind_path;
    out += '-';
    out += stamp;
    out += ".zip";
    return out;
}

inline std::string file_url(const Endpoint& e, std::string_view symbol, std::string_view interval,
                            Cadence cadence, std::string_view stamp) {
    std::string out;
    out.reserve(160);
    out += kDataHost;
    out += '/';
    out += prefix(e, symbol, interval, cadence);
    out += file_name(e, symbol, interval, stamp);
    return out;
}

/// Column indices for the twelve column kline layout, shared by klines,
/// markPriceKlines and premiumIndexKlines.
namespace kline_col {
inline constexpr std::uint8_t kOpenTime  = 0;
inline constexpr std::uint8_t kOpen      = 1;
inline constexpr std::uint8_t kHigh      = 2;
inline constexpr std::uint8_t kLow       = 3;
inline constexpr std::uint8_t kClose     = 4;
inline constexpr std::uint8_t kVolume    = 5;
inline constexpr std::uint8_t kCloseTime = 6;
}  // namespace kline_col

namespace funding_col {
inline constexpr std::uint8_t kCalcTime        = 0;
inline constexpr std::uint8_t kIntervalHours   = 1;
inline constexpr std::uint8_t kLastFundingRate = 2;
}  // namespace funding_col

}  // namespace qp::data_source::source::exchange::binance
