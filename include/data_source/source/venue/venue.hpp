#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "types.hpp"

namespace qp::data_source::source::venue {

/// A venue's fixed, compile-time-declared symbol universe.
///
/// No mutation, no discovery-from-live-data.
template <class T>
concept SymbolTable = requires(std::string_view name, SymbolId id) {
    { T::kSymbols.size() } -> std::convertible_to<std::size_t>;
    { T::id_of(name) } -> std::same_as<std::optional<SymbolId>>;
    { T::name_of(id) } -> std::same_as<std::string_view>;
};

/// A fixed-capacity inline string, usable as a non-type template
/// parameter.
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
constexpr char ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
}
}  // namespace detail

/// The actual symbol-table algorithm, venue-agnostic.
template <auto Symbols>
class FixedSymbolTable {
    using Symbol                         = typename decltype(Symbols)::value_type;
    static constexpr std::size_t kMaxLen = std::tuple_size_v<decltype(Symbol{}.data)>;

   public:
    static constexpr auto kSymbols = Symbols;

    /// Rejects anything longer than the longest known symbol before
    /// touching the buffer.
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

/// The seam a Source depends on to turn raw bytes into a MarketEvent.
/// Source's job ends at handing over a clean, self-describing view;
/// everything past that, including which kind of record this is, is this
/// seam's job.
template <class P>
concept Parser = requires(std::span<const std::byte> raw) {
    { P::parse(raw) } -> std::same_as<std::optional<MarketEvent>>;
};

/// The generic shell every venue's own <venue>_venue.hpp assembles into a
/// concrete Parser: Policy supplies the venue-specific interpretation,
/// Table supplies the symbol universe. Binds the two together, no logic
/// of its own
template <class Policy, SymbolTable Table>
class ComposedParser {
   public:
    static std::optional<MarketEvent> parse(std::span<const std::byte> raw) {
        return Policy::template parse<Table>(raw);
    }
};

}  // namespace qp::data_source::source::venue
