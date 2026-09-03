#pragma once
#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace qp::backtest::config {

// The directory convention tools/fetch_backtest_data.sh writes to and this
// backtest reads from: the two must agree; changing one without the other
// silently produces "no data" empty streams, not a build error.
//   <data_dir>/<SYMBOL>/futures/klines/<SYMBOL>-1m-<day>.csv        (K-tagged)
//   <data_dir>/<SYMBOL>/futures/markprice/<SYMBOL>-1m-<day>.csv     (M-tagged)
//   <data_dir>/<SYMBOL>/futures/funding/<SYMBOL>-fundingRate-<month>.csv (F-tagged)
//   <data_dir>/<SYMBOL>/spot/klines/<SYMBOL>-1m-<day>.csv           (K-tagged)
// klines/markprice are daily dumps (data.binance.vision has no other
// granularity); fundingRate is monthly-only there (verified: the daily
// fundingRate path 404s, only the monthly one resolves). Spot has no
// funding/markprice at all (no perpetual, no official mark).

/// "YYYY-MM-DD" -> year_month_day. Manual, not std::chrono::parse: this
/// repo's libstdc++ implements chrono's formatting but not its from_stream
/// parsing (same gap the old day-range CLI parser worked around). constexpr
/// so QP_BACKTEST_FIRST_DAY/LAST_DAY can be validated at compile time, not
/// just at whatever moment run() happens to execute.
constexpr std::optional<std::chrono::year_month_day> parse_day(std::string_view s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
    auto digits = [](std::string_view d) -> std::optional<int> {
        int value = 0;
        for (char c : d) {
            if (c < '0' || c > '9') return std::nullopt;
            value = value * 10 + (c - '0');
        }
        return value;
    };
    auto y = digits(s.substr(0, 4));
    auto m = digits(s.substr(5, 2));
    auto d = digits(s.substr(8, 2));
    if (!y || !m || !d) return std::nullopt;
    std::chrono::year_month_day ymd{std::chrono::year{*y}, std::chrono::month{unsigned(*m)},
                                    std::chrono::day{unsigned(*d)}};
    if (!ymd.ok()) return std::nullopt;
    return ymd;
}

inline std::string format_day(std::chrono::year_month_day day) {
    return std::format("{:%Y-%m-%d}", day);
}

inline std::string format_month(std::chrono::year_month day) {
    return std::format("{:%Y-%m}", day);
}

/// One path per UTC day in [first_day, last_day], inclusive, for the
/// daily klines/markprice dumps. `kind_dir` is "futures/klines",
/// "futures/markprice", or "spot/klines".
/// Only days that exist on disk are returned: data.binance.vision has
/// genuine one-off holes (e.g. no SOLUSDT markPrice for 2023-11-15), and a
/// vendor gap should leave that day out of the merge, not abort the run.
/// A wholly-missing symbol still yields an empty stream, the same soft
/// failure a wrong data_dir already produces.
inline std::vector<std::filesystem::path> daily_files(const std::filesystem::path& data_dir,
                                                      std::string_view             symbol,
                                                      std::string_view             kind_dir,
                                                      std::chrono::year_month_day  first_day,
                                                      std::chrono::year_month_day  last_day) {
    std::vector<std::filesystem::path> files;
    for (auto day = std::chrono::sys_days{first_day}; day <= std::chrono::sys_days{last_day};
         day += std::chrono::days{1}) {
        std::chrono::year_month_day ymd{day};
        auto                        path = data_dir / symbol / kind_dir /
                    (std::string(symbol) + "-1m-" + format_day(ymd) + ".csv");
        if (std::filesystem::exists(path)) files.push_back(std::move(path));
    }
    return files;
}

/// One path per distinct UTC month touched by [first_day, last_day], the
/// monthly-only fundingRate dumps.
inline std::vector<std::filesystem::path> monthly_funding_files(
    const std::filesystem::path& data_dir, std::string_view symbol,
    std::chrono::year_month_day first_day, std::chrono::year_month_day last_day) {
    std::vector<std::filesystem::path> files;
    std::chrono::year_month            month{first_day.year(), first_day.month()};
    std::chrono::year_month            last_month{last_day.year(), last_day.month()};
    for (; month <= last_month; month += std::chrono::months{1}) {
        files.push_back(data_dir / symbol / "futures" / "funding" /
                        (std::string(symbol) + "-fundingRate-" + format_month(month) + ".csv"));
    }
    return files;
}

}  // namespace qp::backtest::config
