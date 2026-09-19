#pragma once
#include <concepts>
#include <string>

#include "spsc_queue.hpp"

namespace qp::data_source::source::venue::binance {

/// Files in flight per stream. One fetch is ever outstanding, so this only
/// needs headroom for what the reader has not drained yet.
inline constexpr std::size_t kFileQueueCapacity = 4;

/// Decompressed files on their way from a fetch worker to one stream.
using FileQueue = SpscQueue<std::string, kFileQueueCapacity>;

/// Runs GETs off the calling thread and drops each decompressed file into the
/// queue the caller names. False from submit means saturated, try again later.
template <class T>
concept FetchPool = requires(T& pool, std::string url, FileQueue* destination) {
    { pool.submit(std::move(url), destination) } -> std::same_as<bool>;
};

}  // namespace qp::data_source::source::venue::binance
