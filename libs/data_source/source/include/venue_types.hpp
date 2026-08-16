#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace qp::source {

// The 4 venue-agnostic types the Parser concept and GenericLiveWebSocketSource
// are built against — content has zero exchange-specific logic (see
// docs/decisions.md), so they live here rather than under any venue::
// namespace. venue::binance:: keeps aliases to these (binance.hpp) so its own
// code/tests read naturally; a second venue would do the same.

// Maps exchange symbol strings <-> internal SymbolId.
class SymbolTable {
   public:
    // Case-normalized (upper) before compare/store: CLI-provided symbols and
    // a venue's wire-format symbol field aren't guaranteed to agree on case
    // (Binance's JSON is always upper; argv isn't) — without this, the same
    // symbol pre-interned from argv and later re-interned from a live
    // message becomes two different SymbolIds, and every SymbolId-indexed
    // structure sized off the pre-interned count (e.g. FileRecorder's
    // partitions_) goes out of bounds the first time a live message arrives.
    //
    // Takes string_view and compares case-insensitively against the already-
    // uppercased names_ without materializing an uppercase copy of `sym` —
    // only the insert path (a genuinely new symbol) allocates. Re-interning
    // an already-known symbol, the steady-state case once the owner has
    // finished pre-interning (see names() below), is allocation-free, not
    // just logically a no-op. This runs once per message on the I/O thread.
    SymbolId intern(std::string_view sym) {
        auto case_insensitive_eq = [](std::string_view a, std::string_view b) {
            return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) {
                return std::toupper(x) == std::toupper(y);
            });
        };
        for (SymbolId i = 0; i < names_.size(); ++i)
            if (case_insensitive_eq(names_[i], sym)) return i;

        std::string upper{sym};
        std::ranges::transform(upper, upper.begin(),
                               [](unsigned char c) { return std::toupper(c); });
        names_.push_back(std::move(upper));
        return static_cast<SymbolId>(names_.size() - 1);
    }

    const std::string& name(SymbolId id) const { return names_.at(id); }

    // Every interned name, indexed by SymbolId. Safe to call from any thread
    // once the owner has finished pre-interning its full symbol set (see
    // GenericLiveWebSocketSource's constructor) — intern() is then a no-op
    // lookup for every symbol that'll ever be seen, so names_ stops mutating.
    const std::vector<std::string>& names() const { return names_; }

   private:
    std::vector<std::string> names_;
};

// A venue's WebSocket connection target.
struct WsEndpoint {
    std::string_view host;
    std::string_view port;
    bool             use_tls = true;  // false only for local test/mock servers
};

// A venue's REST base URL for the depth-snapshot fetch.
struct RestEndpoint {
    std::string_view base_url;
};

// A REST depth snapshot: the anchor point resync aligns buffered diffs
// against. Field names follow Binance's ("last_update_id") since it's the
// only venue today; a second venue reuses this type as-is if its snapshot
// shape matches, or the type genericizes further if it doesn't.
struct DepthSnapshot {
    std::uint64_t           last_update_id;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

}  // namespace qp::source
