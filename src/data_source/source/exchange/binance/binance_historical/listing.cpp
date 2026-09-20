#include "listing.hpp"

#include <utility>

#include "client.hpp"
#include "endpoints.hpp"

namespace qp::data_source::source::exchange::binance {

using detail::last_segment;
using detail::parse_listing;
using detail::S3Listing;

namespace {

/// Text between the next open and close tag at or after pos. Advances pos past
/// the close tag. Returns false when the open tag is absent.
bool next_element(std::string_view xml, std::string_view open, std::string_view close,
                  std::size_t& pos, std::string_view& out) {
    const auto begin = xml.find(open, pos);
    if (begin == std::string_view::npos) return false;
    const auto value_start = begin + open.size();
    const auto value_end   = xml.find(close, value_start);
    if (value_end == std::string_view::npos) return false;
    out = xml.substr(value_start, value_end - value_start);
    pos = value_end + close.size();
    return true;
}

std::string_view element(std::string_view xml, std::string_view open, std::string_view close) {
    std::size_t      pos = 0;
    std::string_view out;
    return next_element(xml, open, close, pos, out) ? out : std::string_view{};
}

std::string build_url(std::string_view prefix, std::string_view delimiter,
                      std::string_view marker) {
    std::string url;
    url.reserve(256);
    url += kListingHost;
    url += "?prefix=";
    url += prefix;
    if (!delimiter.empty()) {
        url += "&delimiter=";
        url += delimiter;
    }
    if (!marker.empty()) {
        url += "&marker=";
        url += marker;
    }
    return url;
}

/// Fetches one page. An empty delimiter lists keys, "/" lists child prefixes.
S3Listing list_page(std::string_view prefix, std::string_view delimiter, std::string_view marker) {
    const auto body = network::http::get(build_url(prefix, delimiter, marker));
    return parse_listing(std::string_view(reinterpret_cast<const char*>(body.data()), body.size()));
}

}  // namespace

namespace detail {

S3Listing parse_listing(std::string_view xml) {
    S3Listing out;

    std::size_t      pos = 0;
    std::string_view value;
    while (next_element(xml, "<Key>", "</Key>", pos, value)) out.keys.emplace_back(value);

    // Prefix also appears at document level as the echoed request, so the scan
    // anchors on CommonPrefixes rather than on Prefix.
    pos = 0;
    while (next_element(xml, "<CommonPrefixes>", "</CommonPrefixes>", pos, value))
        out.common_prefixes.emplace_back(element(value, "<Prefix>", "</Prefix>"));

    out.truncated   = element(xml, "<IsTruncated>", "</IsTruncated>") == "true";
    out.next_marker = element(xml, "<NextMarker>", "</NextMarker>");
    return out;
}

std::string_view last_segment(std::string_view path) {
    if (!path.empty() && path.back() == '/') path.remove_suffix(1);
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

}  // namespace detail

std::vector<std::string> list_keys(std::string_view prefix) {
    std::vector<std::string> out;
    std::string              marker;
    for (;;) {
        auto page = list_page(prefix, {}, marker);
        if (page.keys.empty()) return out;
        // NextMarker is only sent when a delimiter was passed, so keyed
        // pagination resumes from the last key of the page.
        std::string last = page.keys.back();
        for (auto& key : page.keys)
            if (!key.ends_with(".CHECKSUM")) out.push_back(std::move(key));
        if (!page.truncated) return out;
        marker = std::move(last);
    }
}

std::vector<std::string> list_children(std::string_view prefix) {
    std::vector<std::string> out;
    std::string              marker;
    for (;;) {
        auto page = list_page(prefix, "/", marker);
        for (const auto& child : page.common_prefixes) {
            const auto name = last_segment(child);
            if (!name.empty()) out.emplace_back(name);
        }
        if (!page.truncated || page.common_prefixes.empty()) return out;
        marker = !page.next_marker.empty() ? page.next_marker : page.common_prefixes.back();
    }
}

}  // namespace qp::data_source::source::exchange::binance
