#pragma once
#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fetch_pool.hpp"
#include "http_fetch_pool.hpp"

namespace qp::testing {

/// Records submissions and delivers them on demand, so a stream can be driven
/// through a whole fetch cycle on one thread.
class FakeFetchPool {
   public:
    using FileSlots   = data_source::source::exchange::binance::FileSlots;
    using FetchedFile = data_source::source::exchange::binance::FetchedFile;
    using FetchStatus = data_source::source::exchange::binance::FetchStatus;

    FakeFetchPool() = default;

    /// Takes the real pool's config so a source can own either one.
    explicit FakeFetchPool(const data_source::source::exchange::binance::HttpFetchPoolConfig&) {}

    bool submit(std::string url, FileSlots* destination, std::size_t at) {
        if (saturated_) return false;
        submitted_.push_back(url);
        pending_.push_back({std::move(url), destination, at});
        return true;
    }

    /// Completes the oldest submission with body. False when none is waiting.
    bool deliver(std::string_view body) {
        const auto* bytes = reinterpret_cast<const std::byte*>(body.data());
        return complete(
            FetchedFile{std::vector<std::byte>(bytes, bytes + body.size()), FetchStatus::Ok});
    }

    /// Completes the oldest submission as a failure, body empty.
    bool deliver_failure(FetchStatus status) { return complete(FetchedFile{{}, status}); }

    /// Completes the newest submission instead, so a stream can be shown
    /// reading in plan order while fetches finish out of it.
    bool deliver_newest(std::string_view body) {
        if (pending_.empty()) return false;
        auto request = std::move(pending_.back());
        pending_.pop_back();
        const auto* bytes = reinterpret_cast<const std::byte*>(body.data());
        return request.destination->place(
            request.at,
            FetchedFile{std::vector<std::byte>(bytes, bytes + body.size()), FetchStatus::Ok});
    }

    /// Part of the pool seam. Nothing runs off thread here.
    void quiesce() noexcept { saturated_ = true; }

    void saturate(bool on) noexcept { saturated_ = on; }

    std::size_t in_flight() const noexcept { return pending_.size(); }

    const std::vector<std::string>& submitted() const noexcept { return submitted_; }

   private:
    struct Request {
        std::string url;
        FileSlots*  destination;
        std::size_t at;
    };

    bool complete(FetchedFile file) {
        if (pending_.empty()) return false;
        auto request = std::move(pending_.front());
        pending_.pop_front();
        return request.destination->place(request.at, std::move(file));
    }

    std::deque<Request>      pending_;
    std::vector<std::string> submitted_;
    bool                     saturated_{false};
};

}  // namespace qp::testing
