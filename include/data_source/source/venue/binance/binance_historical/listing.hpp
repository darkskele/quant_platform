#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace qp::data_source::source::venue::binance {

/// Every object key under prefix, paged to exhaustion. CHECKSUM keys are dropped.
std::vector<std::string> list_keys(std::string_view prefix);

/// Every immediate child name under prefix, paged to exhaustion. Empty names are
/// dropped, since the bucket holds empty directories.
std::vector<std::string> list_children(std::string_view prefix);

namespace detail {

/// One page of an S3 bucket listing.
struct S3Listing {
    std::vector<std::string> keys;
    std::vector<std::string> common_prefixes;
    bool                     truncated{false};
    std::string              next_marker;
};

/// Parses a ListBucketResult document. Reads only Key, CommonPrefixes/Prefix,
/// IsTruncated and NextMarker.
S3Listing parse_listing(std::string_view xml);

/// Final path segment of a key or prefix, ignoring a trailing slash.
std::string_view last_segment(std::string_view path);

}  // namespace detail

}  // namespace qp::data_source::source::venue::binance
