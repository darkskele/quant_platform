#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

#include "types.hpp"

namespace qp::transport {

/// Merges N Transport-satisfying sources into one — Engine only ever holds
/// a single Tx; this is how "more than one live/replay source" (e.g. a spot
/// leg and a perp leg for carry) becomes that one Tx, without Engine itself
/// knowing multiple sources exist. Sources can be different concrete types
/// (heterogeneous tuple, no vtable) or the same type constructed twice
/// (e.g. two InProcessTransports over different rings).
///
/// Round-robin, not always-index-0: each call resumes from the source
/// after wherever the last successful pull came from, so one
/// consistently-busy source can't starve the others.
template <class... Sources>
class CombinedTransport {
    static constexpr std::size_t kNumSources = sizeof...(Sources);
    static_assert(kNumSources >= 1);

    using NextFn = std::optional<MarketEvent> (*)(std::tuple<Sources...>&);

    template <std::size_t I>
    static std::optional<MarketEvent> call(std::tuple<Sources...>& sources) {
        return std::get<I>(sources).next();
    }

    template <std::size_t... Is>
    static constexpr std::array<NextFn, kNumSources> make_dispatch(std::index_sequence<Is...>) {
        return {&call<Is>...};
    }

    static constexpr std::array<NextFn, kNumSources> kDispatch =
        make_dispatch(std::index_sequence_for<Sources...>{});

   public:
    explicit CombinedTransport(Sources... sources) : sources_{std::move(sources)...} {}

    std::optional<MarketEvent> next() {
        for (std::size_t tried = 0; tried < kNumSources; ++tried) {
            std::size_t i = (start_ + tried) % kNumSources;
            if (auto event = kDispatch[i](sources_)) {
                start_ = (i + 1) % kNumSources;
                return event;
            }
        }
        return std::nullopt;
    }

   private:
    std::tuple<Sources...> sources_;
    std::size_t            start_ = 0;
};

}  // namespace qp::transport
