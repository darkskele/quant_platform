#pragma once
#include <concepts>
#include <cstdint>

namespace qp::source {

/// The two places a market's sequence-integrity rule differs (D13, D40):
/// `brackets` is the one-time resync-boundary check; `continues` is the
/// steady-state, every-diff continuity check. Futures and spot answer both
/// differently — see FuturesAlignment/SpotAlignment. A compile-time policy
/// makes wiring the wrong rule to a venue a type error, not a silent
/// runtime mistake (D13 shipped with spot's `brackets` rule under a
/// futures-only build; it passed every synthetic test and failed 100% of
/// the time against real data).
template <class T>
concept AlignmentRule =
    requires(std::uint64_t U, std::uint64_t last_update_id, std::uint64_t u,
             std::uint64_t first_seq, std::uint64_t prev_seq, std::uint64_t last_seq) {
        { T::brackets(U, last_update_id, u) } -> std::same_as<bool>;
        { T::continues(first_seq, prev_seq, last_seq) } -> std::same_as<bool>;
    };

/// USD-M futures: `brackets` is U <= last_update_id <= u, no offset — the
/// only `brackets` rule this project has ever verified against real data
/// (D13). `continues` uses Binance's own `pu` field (a diff's `prev_seq`),
/// defined by Binance to equal the previous message's `u` in a healthy
/// stream — kept as its own comparison rather than folded into a generic
/// `U == last+1` formula (see SpotAlignment), since `pu` is Binance's
/// authoritative signal for futures and isn't guaranteed to always equal
/// `U - 1` (D40).
struct FuturesAlignment {
    static bool brackets(std::uint64_t U, std::uint64_t last_update_id, std::uint64_t u) {
        return U <= last_update_id && last_update_id <= u;
    }

    static bool continues(std::uint64_t /*first_seq*/, std::uint64_t prev_seq,
                          std::uint64_t last_seq) {
        return prev_seq == last_seq;
    }
};

/// Spot: `brackets` is U <= last_update_id+1 <= u, Binance's documented
/// rule — not yet independently verified against real spot data the way
/// FuturesAlignment was; confirm against real spot testnet/live traffic
/// before trusting it, same lesson D13 already paid for once. `continues`
/// has no `pu` to compare (spot's depthUpdate doesn't carry that field at
/// all, D40) — Binance's documented steady-state rule instead is that a
/// new diff's `U` (first_seq) must equal the previous diff's `u` + 1.
struct SpotAlignment {
    static bool brackets(std::uint64_t U, std::uint64_t last_update_id, std::uint64_t u) {
        return U <= last_update_id + 1 && last_update_id + 1 <= u;
    }

    static bool continues(std::uint64_t first_seq, std::uint64_t /*prev_seq*/,
                          std::uint64_t last_seq) {
        return first_seq == last_seq + 1;
    }
};

}  // namespace qp::source
