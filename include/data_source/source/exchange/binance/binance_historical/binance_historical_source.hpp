#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "endpoints.hpp"
#include "exchange.hpp"
#include "fetch_pool.hpp"
#include "http_fetch_pool.hpp"
#include "listing.hpp"
#include "parsers/funding.hpp"
#include "parsers/klines.hpp"
#include "parsers/mark_klines.hpp"
#include "parsers/premium_klines.hpp"
#include "source.hpp"
#include "stream.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace qp::data_source::source::exchange::binance {

/// One dataset to pull for every symbol of a market. Interval is ignored by the
/// datasets that do not carry one.
struct StreamSpec {
    EndpointKind kind{};
    std::string  interval;
};

struct BinanceHistoricalConfig {
    /// Applied to every Binance market in the subscription. A dataset the
    /// market does not publish, such as spot funding, is skipped.
    std::vector<StreamSpec> streams;

    /// The source owns its pool, so this is where worker count and retry
    /// policy are set.
    HttpFetchPoolConfig pool{};

    /// Monthly is fewer requests for the same rows. Daily is for spans running
    /// up to now, since the current month is not published until it ends.
    Cadence cadence{Cadence::Monthly};

    Timestamp from{};
    Timestamp to{};

    /// Fetches each stream keeps outstanding. Streams times this is the
    /// concurrency the run can ask for, so it only pays while that is under the
    /// pool's worker count. Clamped to the slot ceiling.
    std::size_t prefetch_depth{kDefaultPrefetchDepth};
};

/// What one stream did over a run, paired with what it belongs to.
struct StreamReport {
    std::uint16_t market{};
    std::string   symbol;
    EndpointKind  kind{};
    std::string   interval;
    GapStats      stats;
};

/// Streams every configured Binance dataset and merges them into one ascending
/// timestamp order.
///
/// @tparam Pool the fetch pool the streams schedule against.
template <FetchPool Pool = HttpFetchPool>
class BinanceHistoricalSource {
   public:
    BinanceHistoricalSource(BinanceHistoricalConfig config, const Subscription& universe)
        : config_(std::move(config)), pool_(config_.pool) {
        build_streams(universe);
    }

    BinanceHistoricalSource(const BinanceHistoricalSource&)            = delete;
    BinanceHistoricalSource& operator=(const BinanceHistoricalSource&) = delete;

    /// Runs before any member is destroyed, so the workers are joined while the
    /// stream queues they write into are still alive.
    ~BinanceHistoricalSource() { pool_.quiesce(); }

    /// Exposed so a run can read fetch stats or stop early.
    Pool& pool() noexcept { return pool_; }

    const Pool& pool() const noexcept { return pool_; }

    /// Discovers each stream's files off the bucket.
    void plan() {
        plan_with([](std::string_view prefix) { return list_keys(prefix); });
    }

    /// Same, against a lister that answers with the keys under a prefix.
    template <class Lister>
    void plan_with(Lister lister) {
        plan_range(klines_, lister);
        plan_range(mark_, lister);
        plan_range(premium_, lister);
        plan_range(funding_, lister);
    }

    /// Earliest buffered event across every stream. NoData while any unfinished
    /// stream has nothing to compare, since it could still hold an earlier one.
    PullResult next() {
        // Only streams without a front are touched, so steady state is one
        // refill plus a heap pop rather than a walk over every stream.
        std::size_t keep    = 0;
        bool        waiting = false;
        for (const auto index : pending_) {
            if (refill(index)) {
                heap_.push_back({slots_[index]->event.base.ts, index});
                std::push_heap(heap_.begin(), heap_.end(), later_first);
                continue;
            }
            if (finished(index)) continue;
            pending_[keep++] = index;
            waiting          = true;
        }
        pending_.resize(keep);

        if (waiting) return std::unexpected(SourceStatus::NoData);
        if (heap_.empty()) return std::unexpected(SourceStatus::Eof);

        std::pop_heap(heap_.begin(), heap_.end(), later_first);
        const auto index = heap_.back().second;
        heap_.pop_back();

        pending_.push_back(index);
        slots_[index]->loaded = false;
        return std::move(slots_[index]->event);
    }

    std::size_t stream_count() const noexcept { return cursors_.size(); }

    std::vector<StreamReport> reports() const {
        std::vector<StreamReport> out;
        out.reserve(stream_count());
        collect(klines_, out);
        collect(mark_, out);
        collect(premium_, out);
        collect(funding_, out);
        return out;
    }

   private:
    template <parsers::RowParser P>
    using Stream = BinanceHistoricalStream<P, Pool>;

    /// One buffered event, the stream's place in the merge.
    struct Slot {
        MarketEvent event;
        bool        loaded{false};
    };

    /// Which stream a flat index refers to.
    struct Cursor {
        EndpointKind  kind{};
        std::uint32_t slot{};
    };

    /// Timestamp first, so the heap orders on it and breaks ties on the index.
    using Entry = std::pair<Timestamp, std::uint32_t>;

    /// The market slot and the lookahead travel with the stream, so nothing has
    /// to keep a parallel array in step with it.
    template <parsers::RowParser P>
    struct Held {
        std::unique_ptr<Stream<P>> stream;
        std::uint16_t              market{};
        Slot                       slot;
    };

    template <parsers::RowParser P>
    using Streams = std::vector<Held<P>>;

    void build_streams(const Subscription& universe) {
        const auto* binance = universe.find_exchange(ExchangeId::Binance);
        if (binance == nullptr) return;

        // The subscription decides which markets and symbols exist. The config
        // only says what to pull for them.
        for (const auto& slice : binance->markets) {
            const auto* path = market_of_slot(slice.market);
            if (path == nullptr) continue;

            for (const auto& symbol : slice.symbols) {
                const auto instrument = universe.resolve(ExchangeId::Binance, slice.market, symbol);
                if (!instrument) continue;

                for (const auto& spec : config_.streams)
                    add_stream(*path, spec, symbol, *instrument);
            }
        }

        index_streams();
    }

    void add_stream(BinanceMarket path, const StreamSpec& spec, const std::string& symbol,
                    Subscription::Instrument instrument) {
        switch (spec.kind) {
            case EndpointKind::Klines:
                add(klines_, path, spec, symbol, instrument);
                break;
            case EndpointKind::MarkPriceKlines:
                add(mark_, path, spec, symbol, instrument);
                break;
            case EndpointKind::PremiumIndexKlines:
                add(premium_, path, spec, symbol, instrument);
                break;
            case EndpointKind::FundingRate:
                add(funding_, path, spec, symbol, instrument);
                break;
        }
    }

    template <parsers::RowParser P>
    void add(Streams<P>& streams, BinanceMarket path, const StreamSpec& spec,
             const std::string& symbol, Subscription::Instrument instrument) {
        // Spot publishes no funding, mark or premium, so those are simply not
        // built for it rather than being an error.
        const auto* entry = find_endpoint(path, P::endpoint_kind);
        if (entry == nullptr) return;

        // Falls back rather than building a path the dataset does not publish.
        const auto cadence =
            supports(*entry, config_.cadence) ? config_.cadence : fallback_cadence(*entry);

        streams.push_back(
            Held<P>{.stream = std::make_unique<Stream<P>>(pool_, path, cadence, symbol,
                                                          P::intervalled ? spec.interval : "",
                                                          instrument, config_.prefetch_depth),
                    .market = instrument.market,
                    .slot   = {}});
    }

    template <parsers::RowParser P, class Lister>
    void plan_range(Streams<P>& streams, Lister& lister) {
        for (auto& held : streams)
            held.stream->plan(lister(held.stream->prefix()), config_.from, config_.to);
    }

    /// Min-heap order. Ties fall to the lower stream index, which is the order
    /// a linear scan would have given, so output stays reproducible.
    static bool later_first(const Entry& a, const Entry& b) noexcept { return a > b; }

    bool refill(std::uint32_t index) {
        const auto cursor = cursors_[index];
        switch (cursor.kind) {
            case EndpointKind::Klines:
                return load(klines_[cursor.slot]);
            case EndpointKind::MarkPriceKlines:
                return load(mark_[cursor.slot]);
            case EndpointKind::PremiumIndexKlines:
                return load(premium_[cursor.slot]);
            case EndpointKind::FundingRate:
                return load(funding_[cursor.slot]);
        }
        return false;
    }

    bool finished(std::uint32_t index) const {
        const auto cursor = cursors_[index];
        switch (cursor.kind) {
            case EndpointKind::Klines:
                return klines_[cursor.slot].stream->finished();
            case EndpointKind::MarkPriceKlines:
                return mark_[cursor.slot].stream->finished();
            case EndpointKind::PremiumIndexKlines:
                return premium_[cursor.slot].stream->finished();
            case EndpointKind::FundingRate:
                return funding_[cursor.slot].stream->finished();
        }
        return true;
    }

    template <parsers::RowParser P>
    static bool load(Held<P>& held) {
        if (!held.slot.loaded) held.slot.loaded = held.stream->next(held.slot.event);
        return held.slot.loaded;
    }

    /// Flat index over every stream, built once so the merge never walks the
    /// per kind vectors.
    void index_streams() {
        const auto add = [this](auto& streams, EndpointKind kind) {
            for (std::size_t i = 0; i < streams.size(); ++i) {
                cursors_.push_back({kind, static_cast<std::uint32_t>(i)});
                slots_.push_back(&streams[i].slot);
            }
        };
        add(klines_, EndpointKind::Klines);
        add(mark_, EndpointKind::MarkPriceKlines);
        add(premium_, EndpointKind::PremiumIndexKlines);
        add(funding_, EndpointKind::FundingRate);

        // Every stream is in exactly one of pending_ or heap_, or dropped once
        // finished, so neither grows past this and neither reallocates again.
        pending_.resize(cursors_.size());
        for (std::uint32_t i = 0; i < pending_.size(); ++i) pending_[i] = i;
        heap_.reserve(cursors_.size());
    }

    template <parsers::RowParser P>
    void collect(const Streams<P>& streams, std::vector<StreamReport>& out) const {
        for (const auto& held : streams)
            out.push_back(StreamReport{.market   = held.market,
                                       .symbol   = std::string(held.stream->symbol()),
                                       .kind     = P::endpoint_kind,
                                       .interval = std::string(held.stream->interval()),
                                       .stats    = held.stream->stats()});
    }

    BinanceHistoricalConfig config_;
    Pool                    pool_;

    Streams<parsers::KlineParser>             klines_;
    Streams<parsers::MarkPriceKlineParser>    mark_;
    Streams<parsers::PremiumIndexKlineParser> premium_;
    Streams<parsers::FundingParser>           funding_;

    std::vector<Cursor> cursors_;
    std::vector<Slot*>  slots_;

    /// Streams with no front event. Everything else is in the heap.
    std::vector<std::uint32_t> pending_;
    std::vector<Entry>         heap_;
};

}  // namespace qp::data_source::source::exchange::binance
