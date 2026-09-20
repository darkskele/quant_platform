#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "slot_ring.hpp"

namespace qp::data_source::source::venue::binance {

/// How one fetch ended. Everything but Ok arrives with an empty body.
enum class FetchStatus : std::uint8_t {
    Ok,
    NotFound,        // the listing and the bucket disagree, never retried
    ServerError,     // 5xx, retried
    TransportError,  // resolve, connect, TLS or read, retried
    ZipError,        // bytes arrived and were not a single entry archive
    Cancelled,       // the pool stopped before this one ran
};

constexpr bool is_failure(FetchStatus status) noexcept { return status != FetchStatus::Ok; }

/// One decompressed file on its way from a fetch worker to one stream. A
/// failure still travels, so the stream stops waiting on it.
struct FetchedFile {
    std::vector<std::byte> body;
    FetchStatus            status{FetchStatus::Ok};
};

/// Files in flight per stream.
inline constexpr std::size_t kFileSlotCount = 8;

using FileSlots = SlotRing<FetchedFile, kFileSlotCount>;

/// Runs GETs off the calling thread and places each decompressed file at the
/// position the caller names.
template <class T>
concept FetchPool = requires(T& pool, std::string url, FileSlots* destination, std::size_t at) {
    { pool.submit(std::move(url), destination, at) } -> std::same_as<bool>;
    { pool.quiesce() };
};

}  // namespace qp::data_source::source::venue::binance
