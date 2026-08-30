#pragma once
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "types.hpp"

namespace qp::data_source::source::venue {

/// A venue's fixed, compile-time-declared symbol universe. id_of/name_of
/// are ordinary constexpr, not consteval: both get called at genuine
/// runtime too (id_of at the wire-parsing boundary, translating an
/// incoming message's symbol string; name_of by diagnostics/logging), not
/// only from constant-expression contexts — unlike venue_consumer_index
/// (core/venue_subscriptions.hpp), which is composition-root-only and has
/// no legitimate runtime caller.
///
/// Deliberately smaller than the old runtime-interning SymbolTable
/// (venue_types.hpp): no mutation, no discovery-from-live-data — every
/// symbol a venue's parser can ever intern is declared right here, so
/// id<->name resolution is provably deterministic (static_assert-able,
/// not just runtime-cross-checked) instead of depending on message
/// arrival order or which manifest file happened to be read first.
///
/// kSymbols isn't pinned to a specific storage type here (FixedSymbolTable
/// below uses std::array<FixedString<N>, M>, not std::array<string_view,
/// M> — see FixedString's own comment for why) — only that it's sized,
/// keeping the concept implementation-agnostic.
template <class T>
concept SymbolTable = requires(std::string_view name, SymbolId id) {
    { T::kSymbols.size() } -> std::convertible_to<std::size_t>;
    { T::id_of(name) } -> std::same_as<std::optional<SymbolId>>;
    { T::name_of(id) } -> std::same_as<std::string_view>;
};

/// A fixed-capacity inline string, usable as a non-type template
/// parameter — std::array<std::string_view, N> can't be (verified:
/// std::string_view's members aren't public in libstdc++/libc++, so it
/// isn't a structural type; a std::array of it inherits that). This
/// stores the bytes directly instead of a pointer, sidestepping the
/// problem entirely — a genuinely structural aggregate (public array +
/// size, no pointers), which is also why it can't just wrap a
/// std::string_view internally.
template <std::size_t N>
struct FixedString {
    std::array<char, N> data{};
    std::size_t         len = 0;

    constexpr FixedString() = default;

    template <std::size_t M>
    constexpr FixedString(const char (&str)[M]) {
        static_assert(M - 1 <= N, "symbol literal too long for this FixedString<N>");
        for (std::size_t i = 0; i < M - 1; ++i) data[i] = str[i];
        len = M - 1;
    }

    constexpr std::string_view view() const { return {data.data(), len}; }

    constexpr operator std::string_view() const { return view(); }

    friend constexpr bool operator==(const FixedString&, const FixedString&) = default;
};

namespace detail {
// std::toupper (<cctype>) isn't constexpr — FixedSymbolTable::id_of below
// needs to be, so a static_assert can actually exercise it, not just
// ordinary unit tests. ASCII-only is fine: every symbol any venue's wire
// format sends is ASCII by construction.
constexpr char ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
}
}  // namespace detail

/// The actual symbol-table algorithm, venue-agnostic — every venue
/// supplies just its data (Symbols, an ordered std::array<FixedString<N>,
/// M> non-type template parameter) and gets id_of/name_of for free.
/// Linear scan, deliberately: measured against both a hand-rolled binary
/// search over an alphabetically-sorted table and std::ranges::lower_bound
/// (2026-08-29, bin_hist_symbol_table.hpp's history) — both were slower
/// at N=10 (best case 17-24ns vs. this linear scan's 12.7-12.9ns; worst
/// case 27-30ns vs. 12.9ns). Ten short strings fit in a couple of cache
/// lines; a sequential scan with clean branch prediction beats both
/// binary-search approaches' index arithmetic and non-sequential access
/// pattern at this size. Don't reintroduce sorting/binary search without
/// new measurements showing otherwise at whatever N a future venue
/// actually needs.
///
/// Uppercases `name` once into a fixed local buffer, then scans kSymbols
/// (already all-uppercase) with plain equality — not a per-character
/// case-insensitive predicate re-applied per candidate, which would
/// re-uppercase every character of `name` once per candidate compared
/// against instead of once, total.
template <auto Symbols>
class FixedSymbolTable {
    using Symbol                         = typename decltype(Symbols)::value_type;
    static constexpr std::size_t kMaxLen = std::tuple_size_v<decltype(Symbol{}.data)>;

   public:
    static constexpr auto kSymbols = Symbols;

    /// Rejects anything longer than the longest known symbol before
    /// touching the buffer — both a correctness bound (never writes past
    /// buf's fixed size) and a fast rejection for garbage input.
    static constexpr std::optional<SymbolId> id_of(std::string_view name) {
        if (name.size() > kMaxLen) return std::nullopt;

        std::array<char, kMaxLen> buf{};
        for (std::size_t i = 0; i < name.size(); ++i) buf[i] = detail::ascii_upper(name[i]);
        std::string_view upper(buf.data(), name.size());

        for (std::size_t i = 0; i < kSymbols.size(); ++i)
            if (kSymbols[i].view() == upper) return static_cast<SymbolId>(i);
        return std::nullopt;
    }

    static constexpr std::string_view name_of(SymbolId id) { return kSymbols.at(id).view(); }
};

/// Shared, venue-agnostic byte/text-parsing helpers — reusable by any
/// *text*-based venue's Parser policy (CSV, JSON, whatever). Deliberately
/// NOT reusable by a hypothetical binary venue (ITCH/FIX-FAST/SBE-style
/// multicast feeds): "parse a float out of these bytes" means something
/// different there — the bytes ARE the float (a fixed-width binary
/// layout, often scaled-integer, not ASCII digits), not characters to
/// interpret. A binary venue needs its own field-extraction utilities,
/// not a variant of these; don't be tempted to generalize as_text/
/// parse_decimal to cover both, that would just hide two different
/// operations behind one name.
namespace bytes {

/// Free — a reinterpret_cast, not a copy. Every text-based venue's parser
/// starts here: Source hands over raw bytes (the seam makes no assumption
/// about text vs. binary), a text-format venue's Parser immediately
/// reinterprets them as characters.
inline std::string_view as_text(std::span<const std::byte> raw) {
    return {reinterpret_cast<const char*>(raw.data()), raw.size()};
}

/// std::from_chars wrapper — locale-independent, no allocation, and (per
/// the standard) doesn't require a null-terminated string the way
/// std::stod does, so it works directly on a substring view without
/// needing to copy it out first.
inline std::optional<double> parse_decimal(std::string_view field) {
    double value{};
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value);
    if (ec != std::errc{} || ptr != field.data() + field.size()) return std::nullopt;
    return value;
}

}  // namespace bytes

/// The seam a Source depends on to turn raw bytes into a MarketEvent — one
/// method, deliberately: Source's job ends at handing over a clean,
/// self-describing view (I/O framing stripped, symbol attached however
/// makes sense for that raw representation — see whichever venue's own
/// _venue.hpp for the concrete convention); everything past that,
/// including which *kind* of record this is (a venue may have several —
/// binance_historical has klines and funding entries, for instance), is
/// this seam's job, not Source's and not split across several methods on
/// it. std::span<const std::byte>, not std::string_view: this seam makes
/// no assumption that a venue's wire format is text (see bytes::as_text
/// above) — every real venue in this codebase today happens to be text,
/// but the seam itself doesn't bake that in.
///
/// Returned by value, not through an out-param: matches
/// transport::BacktestInProcessTransport::next()'s existing shape in this
/// codebase — std::optional<MarketEvent> by value costs at most a move
/// (MarketEvent's only non-trivial members are two vectors; moving one is
/// a pointer swap, not an element-wise copy), and the compiler elides even
/// that in the straightforward single-return-path shape a real
/// implementation has.
template <class P>
concept Parser = requires(std::span<const std::byte> raw) {
    { P::parse(raw) } -> std::same_as<std::optional<MarketEvent>>;
};

/// The generic shell every venue's own <venue>_venue.hpp assembles into a
/// concrete Parser: Policy supplies the venue-specific interpretation (a
/// plain type with a `parse<Table>(raw)` member template — not itself
/// templated on Table, so it stays testable/nameable independent of any
/// one symbol table), Table supplies the symbol universe. This class's
/// only job is binding the two together — no logic of its own. Named
/// ComposedParser, not Parser: that name's already the concept above,
/// and a class template can't share a name with a concept in the same
/// namespace.
template <class Policy, SymbolTable Table>
class ComposedParser {
   public:
    static std::optional<MarketEvent> parse(std::span<const std::byte> raw) {
        return Policy::template parse<Table>(raw);
    }
};

}  // namespace qp::data_source::source::venue
