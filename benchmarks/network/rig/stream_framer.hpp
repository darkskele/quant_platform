#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "message.hpp"

namespace qp::network::rig {

/// Splits a byte stream back into rig messages. Whole messages are read in place. One that
/// straddles two reads is copied together first.
class StreamFramer {
   public:
    StreamFramer() { staging_.reserve(MAX_MESSAGE); }

    /// Calls on(header) for each message bytes completes. The count completed, nothing when a
    /// header's size is impossible.
    template <class F>
    std::optional<std::size_t> feed(std::span<const std::byte> bytes, F&& on) {
        std::size_t done = 0;
        while (!bytes.empty()) {
            if (!staging_.empty()) {
                if (!stage(bytes, sizeof(MessageHeader))) break;
                const MessageHeader h = read_header(staging_);
                if (!valid(h)) return std::nullopt;
                if (!stage(bytes, h.size)) break;
                on(h);
                ++done;
                staging_.clear();
                continue;
            }
            if (bytes.size() < sizeof(MessageHeader)) {
                stage(bytes, sizeof(MessageHeader));
                break;
            }
            const MessageHeader h = read_header(bytes);
            if (!valid(h)) return std::nullopt;
            if (bytes.size() < h.size) {
                stage(bytes, h.size);
                break;
            }
            on(h);
            ++done;
            bytes = bytes.subspan(h.size);
        }
        return done;
    }

   private:
    static bool valid(const MessageHeader& h) noexcept {
        return h.size >= sizeof(MessageHeader) && h.size <= MAX_MESSAGE;
    }

    // Moves bytes into staging until it holds want. True once it does.
    bool stage(std::span<const std::byte>& bytes, std::size_t want) {
        const std::size_t take = std::min(want - std::min(want, staging_.size()), bytes.size());
        staging_.insert(staging_.end(), bytes.begin(), bytes.begin() + take);
        bytes = bytes.subspan(take);
        return staging_.size() >= want;
    }

    std::vector<std::byte> staging_;
};

}  // namespace qp::network::rig
