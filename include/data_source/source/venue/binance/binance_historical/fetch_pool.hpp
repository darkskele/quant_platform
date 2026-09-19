#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "spsc_queue.hpp"

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

/// Files in flight per stream. One fetch is ever outstanding, so this only
/// needs headroom for what the reader has not drained yet.
inline constexpr std::size_t kFileQueueCapacity = 4;

using FileQueue = SpscQueue<FetchedFile, kFileQueueCapacity>;

/// Runs GETs off the calling thread and drops each decompressed file into the
/// queue the caller names. False from submit means saturated, try again later.
/// quiesce stops the workers, so whoever owns the queues can outlive them.
template <class T>
concept FetchPool = requires(T& pool, std::string url, FileQueue* destination) {
    { pool.submit(std::move(url), destination) } -> std::same_as<bool>;
    { pool.quiesce() };
};

}  // namespace qp::data_source::source::venue::binance
