#pragma once
#include <concepts>
#include <cstdint>

namespace qp::source {

/// The one place Binance's resync-alignment rule differs by market — see
/// docs/decisions.md D13: this project shipped with the wrong one once
/// already (spot's rule, while only ever targeting futures), and it passed
/// every synthetic test while failing 100% of the time against real data.
/// A compile-time policy makes wiring the wrong rule to a venue a type
/// error, not a silent runtime mistake.
template <class T>
concept AlignmentRule = requires(std::uint64_t U, std::uint64_t last_update_id, std::uint64_t u) {
    { T::brackets(U, last_update_id, u) } -> std::same_as<bool>;
};

/// USD-M futures: U <= last_update_id <= u, no offset. The only rule this
/// project has ever verified against real data (D13).
struct FuturesAlignment {
    static bool brackets(std::uint64_t U, std::uint64_t last_update_id, std::uint64_t u) {
        return U <= last_update_id && last_update_id <= u;
    }
};

/// Spot: U <= last_update_id+1 <= u. Binance's documented rule — not yet
/// independently verified against real spot data the way FuturesAlignment
/// was; confirm against real spot testnet/live traffic before trusting it,
/// same lesson D13 already paid for once.
struct SpotAlignment {
    static bool brackets(std::uint64_t U, std::uint64_t last_update_id, std::uint64_t u) {
        return U <= last_update_id + 1 && last_update_id + 1 <= u;
    }
};

}  // namespace qp::source
