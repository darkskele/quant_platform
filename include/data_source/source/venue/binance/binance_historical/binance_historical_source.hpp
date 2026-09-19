#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "endpoints.hpp"
#include "exchange.hpp"
#include "fetch_pool.hpp"
#include "listing.hpp"
#include "parsers/funding.hpp"
#include "parsers/klines.hpp"
#include "parsers/mark_klines.hpp"
#include "parsers/premium_klines.hpp"
#include "source.hpp"
#include "stream.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance {

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

    /// Monthly is fewer requests for the same rows. Daily is for spans running
    /// up to now, since the current month is not published until it ends.
    Cadence cadence{Cadence::Monthly};

    Timestamp from{};
    Timestamp to{};
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
template <FetchPool Pool>
class BinanceHistoricalSource {
   public:
    BinanceHistoricalSource(BinanceHistoricalConfig config, const Subscription& universe,
                            Pool& pool)
        : config_(std::move(config)), pool_(&pool) {
        build_streams(universe);
    }

    BinanceHistoricalSource(const BinanceHistoricalSource&)            = delete;
    BinanceHistoricalSource& operator=(const BinanceHistoricalSource&) = delete;

    /// Stops the workers before the stream queues they write into go away.
    ~BinanceHistoricalSource() { pool_->quiesce(); }

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
        Slot* earliest = nullptr;
        bool  waiting  = false;

        // Every stream is visited even once one is known to be waiting, so they
        // all keep their fetches moving.
        scan(klines_, earliest, waiting);
        scan(mark_, earliest, waiting);
        scan(premium_, earliest, waiting);
        scan(funding_, earliest, waiting);

        if (waiting) return std::unexpected(SourceStatus::NoData);
        if (earliest == nullptr) return std::unexpected(SourceStatus::Eof);

        earliest->loaded = false;
        return std::move(earliest->event);
    }

    std::size_t stream_count() const noexcept {
        return klines_.size() + mark_.size() + premium_.size() + funding_.size();
    }

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
        const auto cadence = supports(*entry, config_.cadence) ? config_.cadence : Cadence::Monthly;

        streams.push_back(
            Held<P>{std::make_unique<Stream<P>>(*pool_, path, cadence, symbol,
                                                P::intervalled ? spec.interval : "", instrument),
                    instrument.market});
    }

    template <parsers::RowParser P, class Lister>
    void plan_range(Streams<P>& streams, Lister& lister) {
        for (auto& held : streams)
            held.stream->plan(lister(held.stream->prefix()), config_.from, config_.to);
    }

    /// Tops each stream's slot up and keeps the earliest of them. A stream with
    /// nothing buffered that is not finished blocks the merge, since it could
    /// still hold an earlier timestamp.
    template <parsers::RowParser P>
    void scan(Streams<P>& streams, Slot*& earliest, bool& waiting) {
        for (auto& held : streams) {
            auto& slot = held.slot;
            if (!slot.loaded) slot.loaded = held.stream->next(slot.event);

            if (!slot.loaded) {
                if (!held.stream->finished()) waiting = true;
                continue;
            }
            if (earliest == nullptr || slot.event.base.ts < earliest->event.base.ts)
                earliest = &slot;
        }
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
    Pool*                   pool_;

    Streams<parsers::KlineParser>             klines_;
    Streams<parsers::MarkPriceKlineParser>    mark_;
    Streams<parsers::PremiumIndexKlineParser> premium_;
    Streams<parsers::FundingParser>           funding_;
};

}  // namespace qp::data_source::source::venue::binance
