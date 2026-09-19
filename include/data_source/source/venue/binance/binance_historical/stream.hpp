#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "endpoints.hpp"
#include "fetch_pool.hpp"
#include "listing.hpp"
#include "parsers/csv_field.hpp"
#include "parsers/parser.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance {

/// What a sweep of one stream did and did not manage to read.
struct GapStats {
    std::size_t  files_planned{};
    std::size_t  files_read{};
    std::size_t  files_failed{};
    std::size_t  header_rows{};
    std::size_t  blank_rows{};
    std::int64_t rows_expected{};
    std::size_t  rows_parsed{};
    std::size_t  rows_rejected{};
    std::size_t  backwards_stamps{};
};

/// One (market, symbol, kind, interval) and the events read out of it. Plans its
/// own files, keeps its own queue fed off the pool, and parses on the calling
/// thread.
///
/// @tparam P the row parser, which fixes the dataset and the payload type.
/// @tparam Pool the fetch pool the stream schedules against.
template <parsers::RowParser P, FetchPool Pool>
class BinanceHistoricalStream {
   public:
    BinanceHistoricalStream(Pool& pool, BinanceMarket market_path, Cadence cadence,
                            std::string symbol, std::string interval,
                            Subscription::Instrument instrument)
        : pool_(&pool),
          endpoint_(endpoint(market_path, P::endpoint_kind)),
          cadence_(cadence),
          symbol_(std::move(symbol)),
          interval_(std::move(interval)),
          instrument_(instrument) {}

    /// Discovers the files for this stream and keeps those inside the span.
    void plan(Timestamp from, Timestamp to) {
        plan(list_keys(prefix(endpoint_, symbol_, interval_, cadence_)), from, to);
    }

    /// Same, against a key list already in hand.
    void plan(std::vector<std::string> keys, Timestamp from, Timestamp to) {
        for (auto& key : keys) {
            const auto stamp = stamp_of(key);
            if (stamp.empty()) continue;
            const auto start = stamp_start_nanos(stamp);
            if (start < from || start > to) continue;
            paths_.push_back(std::move(key));
        }
        stats_.files_planned = paths_.size();
    }

    /// Tops the queue up and pops one event. False means nothing is buffered
    /// right now, which is not the same as finished.
    bool next(MarketEvent& out) {
        // Starved always pumps, otherwise a stream would sit idle until the
        // tick came round. While rows are flowing it amortises, so the per
        // event cost is a local increment rather than the queue's atomics.
        if (rows_.empty() || (++pump_tick_ & kPumpMask) == 0) pump();
        for (;;) {
            if (rows_.empty() && !load_next_file()) return false;

            while (!rows_.empty()) {
                const auto row = take_row();
                if (row.empty()) {
                    ++stats_.blank_rows;
                    continue;
                }
                if (parsers::is_header_row(row)) {
                    ++stats_.header_rows;
                    continue;
                }

                Timestamp         ts{};
                typename P::Event payload{};
                if (!P::parse(row, ts, payload)) {
                    ++stats_.rows_rejected;
                    continue;
                }
                if (ts < last_ts_) ++stats_.backwards_stamps;
                last_ts_ = ts;
                ++stats_.rows_parsed;

                out.base    = EventBase{.kind     = P::event_kind,
                                        .exchange = instrument_.exchange,
                                        .market   = instrument_.market,
                                        .symbol   = instrument_.symbol,
                                        .ts       = ts};
                out.payload = payload;
                return true;
            }
        }
    }

    /// Every planned file fetched, read and drained.
    bool finished() const noexcept {
        return fetch_cursor_ >= paths_.size() && outstanding() == 0 && queue_.size() == 0 &&
               rows_.empty();
    }

    std::size_t planned_files() const noexcept { return paths_.size(); }

    const GapStats& stats() const noexcept { return stats_; }

    std::string_view symbol() const noexcept { return symbol_; }

   private:
    /// Files asked for that have not landed in the queue yet. Both terms are
    /// consumer owned, so no shared counter is needed.
    std::size_t outstanding() const noexcept { return scheduled_ - popped_ - queue_.size(); }

    /// One ring slot always stays empty, so this is what it can really hold.
    static constexpr std::size_t kMaxBufferedFiles = FileQueue::capacity() - 1;

    /// At most one fetch in flight, which keeps the queue single producer and
    /// keeps files landing in plan order. The second gate leaves a slot for the
    /// file being asked for, so a delivery can never find the ring full.
    void pump() {
        if (fetch_cursor_ >= paths_.size()) return;
        if (outstanding() > 0) return;
        if (queue_.size() + outstanding() >= kMaxBufferedFiles) return;
        if (!pool_->submit(file_url(fetch_cursor_), &queue_)) return;
        ++fetch_cursor_;
        ++scheduled_;
    }

    std::string file_url(std::size_t index) const {
        std::string url;
        url.reserve(160);
        url += kDataHost;
        url += '/';
        url += paths_[index];
        return url;
    }

    /// Pops until a readable file turns up. A failed fetch still counts its
    /// expected rows, so the shortfall shows against rows_parsed rather than
    /// vanishing.
    bool load_next_file() {
        for (;;) {
            auto file = queue_.pop();
            if (!file) return false;

            ++popped_;
            if (read_cursor_ < paths_.size())
                stats_.rows_expected += expected_rows(stamp_of(paths_[read_cursor_++]));
            pump();

            if (is_failure(file->status)) {
                ++stats_.files_failed;
                continue;
            }

            current_ = std::move(file->body);
            rows_ =
                std::string_view(reinterpret_cast<const char*>(current_.data()), current_.size());
            ++stats_.files_read;
            return true;
        }
    }

    std::string_view take_row() noexcept {
        const auto       newline = rows_.find('\n');
        std::string_view row;
        if (newline == std::string_view::npos) {
            row   = rows_;
            rows_ = {};
        } else {
            row = rows_.substr(0, newline);
            rows_.remove_prefix(newline + 1);
        }
        if (!row.empty() && row.back() == '\r') row.remove_suffix(1);
        return row;
    }

    static bool digits_at(std::string_view s, std::size_t pos, std::size_t count) noexcept {
        for (std::size_t i = 0; i < count; ++i)
            if (!parsers::is_digit(s[pos + i])) return false;
        return true;
    }

    /// Trailing YYYY-MM-DD or YYYY-MM of the file name. Daily and monthly
    /// cadences differ only in whether the day is there.
    static std::string_view stamp_of(std::string_view key) noexcept {
        auto name = detail::last_segment(key);
        if (!name.ends_with(".zip")) return {};
        name.remove_suffix(4);

        if (name.size() >= 10) {
            const auto tail = name.substr(name.size() - 10);
            if (digits_at(tail, 0, 4) && tail[4] == '-' && digits_at(tail, 5, 2) &&
                tail[7] == '-' && digits_at(tail, 8, 2))
                return tail;
        }
        if (name.size() >= 7) {
            const auto tail = name.substr(name.size() - 7);
            if (digits_at(tail, 0, 4) && tail[4] == '-' && digits_at(tail, 5, 2)) return tail;
        }
        return {};
    }

    static Timestamp stamp_start_nanos(std::string_view stamp) noexcept {
        if (stamp.size() < 7) return 0;
        const int year = (stamp[0] - '0') * 1000 + (stamp[1] - '0') * 100 + (stamp[2] - '0') * 10 +
                         (stamp[3] - '0');
        const int month = (stamp[5] - '0') * 10 + (stamp[6] - '0');
        const int day   = stamp.size() >= 10 ? (stamp[8] - '0') * 10 + (stamp[9] - '0') : 1;
        return days_from_civil(year, month, day) * 86'400LL * 1'000'000'000LL;
    }

    /// Bar width of a Binance interval token. Zero when the token is unknown or
    /// the dataset carries no interval.
    static constexpr std::int64_t interval_nanos(std::string_view interval) noexcept {
        if (interval.empty()) return 0;

        std::int64_t count = 0;
        std::size_t  i     = 0;
        for (; i < interval.size() && parsers::is_digit(interval[i]); ++i)
            count = count * 10 + (interval[i] - '0');
        if (count == 0) return 0;

        constexpr std::int64_t kSecond = 1'000'000'000LL;
        const auto             unit    = interval.substr(i);
        if (unit == "s") return count * kSecond;
        if (unit == "m") return count * 60 * kSecond;
        if (unit == "h") return count * 3'600 * kSecond;
        if (unit == "d") return count * 86'400 * kSecond;
        if (unit == "w") return count * 7 * 86'400 * kSecond;
        return 0;
    }

    static constexpr int days_in_month(int year, int month) noexcept {
        constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month < 1 || month > 12) return 0;
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return month == 2 && leap ? 29 : kDays[month - 1];
    }

    /// Bars a file of this stamp should hold. Zero when it cannot be known,
    /// which is every dataset whose interval is not in the path.
    std::int64_t expected_rows(std::string_view stamp) const noexcept {
        const auto width = interval_nanos(interval_);
        if (width == 0 || stamp.size() < 7) return 0;

        constexpr std::int64_t kDay = 86'400LL * 1'000'000'000LL;
        std::int64_t           span = kDay;
        if (stamp.size() < 10) {
            const int year = (stamp[0] - '0') * 1000 + (stamp[1] - '0') * 100 +
                             (stamp[2] - '0') * 10 + (stamp[3] - '0');
            const int month = (stamp[5] - '0') * 10 + (stamp[6] - '0');
            span            = days_in_month(year, month) * kDay;
        }
        return span / width;
    }

    /// Days since the unix epoch, Howard Hinnant's civil calendar algorithm.
    static constexpr std::int64_t days_from_civil(int y, int m, int d) noexcept {
        y -= m <= 2;
        const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
        const auto         yoe = static_cast<unsigned>(y - era * 400);
        const auto doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
        const auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146'097 + static_cast<std::int64_t>(doe) - 719'468;
    }

    Pool*                    pool_;
    const Endpoint&          endpoint_;
    Cadence                  cadence_;
    std::string              symbol_;
    std::string              interval_;
    Subscription::Instrument instrument_;

    std::vector<std::string> paths_;
    std::size_t              fetch_cursor_{};
    std::size_t              read_cursor_{};
    std::size_t              scheduled_{};
    std::size_t              popped_{};

    static constexpr std::size_t kPumpMask = 0xFF;

    FileQueue              queue_;
    std::size_t            pump_tick_{};
    std::vector<std::byte> current_;
    std::string_view       rows_;
    Timestamp              last_ts_{};
    GapStats               stats_;
};

}  // namespace qp::data_source::source::venue::binance
